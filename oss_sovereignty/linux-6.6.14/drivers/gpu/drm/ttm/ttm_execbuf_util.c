 
 

#include <drm/ttm/ttm_execbuf_util.h>
#include <drm/ttm/ttm_bo.h>

static void ttm_eu_backoff_reservation_reverse(struct list_head *list,
					      struct ttm_validate_buffer *entry)
{
	list_for_each_entry_continue_reverse(entry, list, head) {
		struct ttm_buffer_object *bo = entry->bo;

		dma_resv_unlock(bo->base.resv);
	}
}

void ttm_eu_backoff_reservation(struct ww_acquire_ctx *ticket,
				struct list_head *list)
{
	struct ttm_validate_buffer *entry;

	if (list_empty(list))
		return;

	list_for_each_entry(entry, list, head) {
		struct ttm_buffer_object *bo = entry->bo;

		ttm_bo_move_to_lru_tail_unlocked(bo);
		dma_resv_unlock(bo->base.resv);
	}

	if (ticket)
		ww_acquire_fini(ticket);
}
EXPORT_SYMBOL(ttm_eu_backoff_reservation);

 

int ttm_eu_reserve_buffers(struct ww_acquire_ctx *ticket,
			   struct list_head *list, bool intr,
			   struct list_head *dups)
{
	struct ttm_validate_buffer *entry;
	int ret;

	if (list_empty(list))
		return 0;

	if (ticket)
		ww_acquire_init(ticket, &reservation_ww_class);

	list_for_each_entry(entry, list, head) {
		struct ttm_buffer_object *bo = entry->bo;
		unsigned int num_fences;

		ret = ttm_bo_reserve(bo, intr, (ticket == NULL), ticket);
		if (ret == -EALREADY && dups) {
			struct ttm_validate_buffer *safe = entry;
			entry = list_prev_entry(entry, head);
			list_del(&safe->head);
			list_add(&safe->head, dups);
			continue;
		}

		num_fences = max(entry->num_shared, 1u);
		if (!ret) {
			ret = dma_resv_reserve_fences(bo->base.resv,
						      num_fences);
			if (!ret)
				continue;
		}

		 
		ttm_eu_backoff_reservation_reverse(list, entry);

		if (ret == -EDEADLK) {
			ret = ttm_bo_reserve_slowpath(bo, intr, ticket);
		}

		if (!ret)
			ret = dma_resv_reserve_fences(bo->base.resv,
						      num_fences);

		if (unlikely(ret != 0)) {
			if (ticket) {
				ww_acquire_done(ticket);
				ww_acquire_fini(ticket);
			}
			return ret;
		}

		 
		list_del(&entry->head);
		list_add(&entry->head, list);
	}

	return 0;
}
EXPORT_SYMBOL(ttm_eu_reserve_buffers);

void ttm_eu_fence_buffer_objects(struct ww_acquire_ctx *ticket,
				 struct list_head *list,
				 struct dma_fence *fence)
{
	struct ttm_validate_buffer *entry;

	if (list_empty(list))
		return;

	list_for_each_entry(entry, list, head) {
		struct ttm_buffer_object *bo = entry->bo;

		dma_resv_add_fence(bo->base.resv, fence, entry->num_shared ?
				   DMA_RESV_USAGE_READ : DMA_RESV_USAGE_WRITE);
		ttm_bo_move_to_lru_tail_unlocked(bo);
		dma_resv_unlock(bo->base.resv);
	}
	if (ticket)
		ww_acquire_fini(ticket);
}
EXPORT_SYMBOL(ttm_eu_fence_buffer_objects);
