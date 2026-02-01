
 
#include "vmwgfx_bo.h"
#include "vmwgfx_drv.h"
#include "vmwgfx_resource_priv.h"
#include "vmwgfx_validation.h"

#include <linux/slab.h>


#define VMWGFX_VALIDATION_MEM_GRAN (16*PAGE_SIZE)

 
struct vmw_validation_bo_node {
	struct ttm_validate_buffer base;
	struct vmwgfx_hash_item hash;
	unsigned int coherent_count;
};
 
struct vmw_validation_res_node {
	struct list_head head;
	struct vmwgfx_hash_item hash;
	struct vmw_resource *res;
	struct vmw_bo *new_guest_memory_bo;
	unsigned long new_guest_memory_offset;
	u32 no_buffer_needed : 1;
	u32 switching_guest_memory_bo : 1;
	u32 first_usage : 1;
	u32 reserved : 1;
	u32 dirty : 1;
	u32 dirty_set : 1;
	unsigned long private[];
};

 
void *vmw_validation_mem_alloc(struct vmw_validation_context *ctx,
			       unsigned int size)
{
	void *addr;

	size = vmw_validation_align(size);
	if (size > PAGE_SIZE)
		return NULL;

	if (ctx->mem_size_left < size) {
		struct page *page;

		if (ctx->vm && ctx->vm_size_left < PAGE_SIZE) {
			ctx->vm_size_left += VMWGFX_VALIDATION_MEM_GRAN;
			ctx->total_mem += VMWGFX_VALIDATION_MEM_GRAN;
		}

		page = alloc_page(GFP_KERNEL | __GFP_ZERO);
		if (!page)
			return NULL;

		if (ctx->vm)
			ctx->vm_size_left -= PAGE_SIZE;

		list_add_tail(&page->lru, &ctx->page_list);
		ctx->page_address = page_address(page);
		ctx->mem_size_left = PAGE_SIZE;
	}

	addr = (void *) (ctx->page_address + (PAGE_SIZE - ctx->mem_size_left));
	ctx->mem_size_left -= size;

	return addr;
}

 
static void vmw_validation_mem_free(struct vmw_validation_context *ctx)
{
	struct page *entry, *next;

	list_for_each_entry_safe(entry, next, &ctx->page_list, lru) {
		list_del_init(&entry->lru);
		__free_page(entry);
	}

	ctx->mem_size_left = 0;
	if (ctx->vm && ctx->total_mem) {
		ctx->total_mem = 0;
		ctx->vm_size_left = 0;
	}
}

 
static struct vmw_validation_bo_node *
vmw_validation_find_bo_dup(struct vmw_validation_context *ctx,
			   struct vmw_bo *vbo)
{
	struct  vmw_validation_bo_node *bo_node = NULL;

	if (!ctx->merge_dups)
		return NULL;

	if (ctx->sw_context) {
		struct vmwgfx_hash_item *hash;
		unsigned long key = (unsigned long) vbo;

		hash_for_each_possible_rcu(ctx->sw_context->res_ht, hash, head, key) {
			if (hash->key == key) {
				bo_node = container_of(hash, typeof(*bo_node), hash);
				break;
			}
		}
	} else {
		struct  vmw_validation_bo_node *entry;

		list_for_each_entry(entry, &ctx->bo_list, base.head) {
			if (entry->base.bo == &vbo->tbo) {
				bo_node = entry;
				break;
			}
		}
	}

	return bo_node;
}

 
static struct vmw_validation_res_node *
vmw_validation_find_res_dup(struct vmw_validation_context *ctx,
			    struct vmw_resource *res)
{
	struct  vmw_validation_res_node *res_node = NULL;

	if (!ctx->merge_dups)
		return NULL;

	if (ctx->sw_context) {
		struct vmwgfx_hash_item *hash;
		unsigned long key = (unsigned long) res;

		hash_for_each_possible_rcu(ctx->sw_context->res_ht, hash, head, key) {
			if (hash->key == key) {
				res_node = container_of(hash, typeof(*res_node), hash);
				break;
			}
		}
	} else {
		struct  vmw_validation_res_node *entry;

		list_for_each_entry(entry, &ctx->resource_ctx_list, head) {
			if (entry->res == res) {
				res_node = entry;
				goto out;
			}
		}

		list_for_each_entry(entry, &ctx->resource_list, head) {
			if (entry->res == res) {
				res_node = entry;
				break;
			}
		}

	}
out:
	return res_node;
}

 
int vmw_validation_add_bo(struct vmw_validation_context *ctx,
			  struct vmw_bo *vbo)
{
	struct vmw_validation_bo_node *bo_node;

	bo_node = vmw_validation_find_bo_dup(ctx, vbo);
	if (!bo_node) {
		struct ttm_validate_buffer *val_buf;

		bo_node = vmw_validation_mem_alloc(ctx, sizeof(*bo_node));
		if (!bo_node)
			return -ENOMEM;

		if (ctx->sw_context) {
			bo_node->hash.key = (unsigned long) vbo;
			hash_add_rcu(ctx->sw_context->res_ht, &bo_node->hash.head,
				bo_node->hash.key);
		}
		val_buf = &bo_node->base;
		val_buf->bo = ttm_bo_get_unless_zero(&vbo->tbo);
		if (!val_buf->bo)
			return -ESRCH;
		val_buf->num_shared = 0;
		list_add_tail(&val_buf->head, &ctx->bo_list);
	}

	return 0;
}

 
int vmw_validation_add_resource(struct vmw_validation_context *ctx,
				struct vmw_resource *res,
				size_t priv_size,
				u32 dirty,
				void **p_node,
				bool *first_usage)
{
	struct vmw_validation_res_node *node;

	node = vmw_validation_find_res_dup(ctx, res);
	if (node) {
		node->first_usage = 0;
		goto out_fill;
	}

	node = vmw_validation_mem_alloc(ctx, sizeof(*node) + priv_size);
	if (!node) {
		VMW_DEBUG_USER("Failed to allocate a resource validation entry.\n");
		return -ENOMEM;
	}

	if (ctx->sw_context) {
		node->hash.key = (unsigned long) res;
		hash_add_rcu(ctx->sw_context->res_ht, &node->hash.head, node->hash.key);
	}
	node->res = vmw_resource_reference_unless_doomed(res);
	if (!node->res)
		return -ESRCH;

	node->first_usage = 1;
	if (!res->dev_priv->has_mob) {
		list_add_tail(&node->head, &ctx->resource_list);
	} else {
		switch (vmw_res_type(res)) {
		case vmw_res_context:
		case vmw_res_dx_context:
			list_add(&node->head, &ctx->resource_ctx_list);
			break;
		case vmw_res_cotable:
			list_add_tail(&node->head, &ctx->resource_ctx_list);
			break;
		default:
			list_add_tail(&node->head, &ctx->resource_list);
			break;
		}
	}

out_fill:
	if (dirty) {
		node->dirty_set = 1;
		 
		node->dirty = (dirty & VMW_RES_DIRTY_SET) ? 1 : 0;
	}
	if (first_usage)
		*first_usage = node->first_usage;
	if (p_node)
		*p_node = &node->private;

	return 0;
}

 
void vmw_validation_res_set_dirty(struct vmw_validation_context *ctx,
				  void *val_private, u32 dirty)
{
	struct vmw_validation_res_node *val;

	if (!dirty)
		return;

	val = container_of(val_private, typeof(*val), private);
	val->dirty_set = 1;
	 
	val->dirty = (dirty & VMW_RES_DIRTY_SET) ? 1 : 0;
}

 
void vmw_validation_res_switch_backup(struct vmw_validation_context *ctx,
				      void *val_private,
				      struct vmw_bo *vbo,
				      unsigned long guest_memory_offset)
{
	struct vmw_validation_res_node *val;

	val = container_of(val_private, typeof(*val), private);

	val->switching_guest_memory_bo = 1;
	if (val->first_usage)
		val->no_buffer_needed = 1;

	val->new_guest_memory_bo = vbo;
	val->new_guest_memory_offset = guest_memory_offset;
}

 
int vmw_validation_res_reserve(struct vmw_validation_context *ctx,
			       bool intr)
{
	struct vmw_validation_res_node *val;
	int ret = 0;

	list_splice_init(&ctx->resource_ctx_list, &ctx->resource_list);

	list_for_each_entry(val, &ctx->resource_list, head) {
		struct vmw_resource *res = val->res;

		ret = vmw_resource_reserve(res, intr, val->no_buffer_needed);
		if (ret)
			goto out_unreserve;

		val->reserved = 1;
		if (res->guest_memory_bo) {
			struct vmw_bo *vbo = res->guest_memory_bo;

			vmw_bo_placement_set(vbo,
					     res->func->domain,
					     res->func->busy_domain);
			ret = vmw_validation_add_bo(ctx, vbo);
			if (ret)
				goto out_unreserve;
		}

		if (val->switching_guest_memory_bo && val->new_guest_memory_bo &&
		    res->coherent) {
			struct vmw_validation_bo_node *bo_node =
				vmw_validation_find_bo_dup(ctx,
							   val->new_guest_memory_bo);

			if (WARN_ON(!bo_node)) {
				ret = -EINVAL;
				goto out_unreserve;
			}
			bo_node->coherent_count++;
		}
	}

	return 0;

out_unreserve:
	vmw_validation_res_unreserve(ctx, true);
	return ret;
}

 
void vmw_validation_res_unreserve(struct vmw_validation_context *ctx,
				 bool backoff)
{
	struct vmw_validation_res_node *val;

	list_splice_init(&ctx->resource_ctx_list, &ctx->resource_list);
	if (backoff)
		list_for_each_entry(val, &ctx->resource_list, head) {
			if (val->reserved)
				vmw_resource_unreserve(val->res,
						       false, false, false,
						       NULL, 0);
		}
	else
		list_for_each_entry(val, &ctx->resource_list, head) {
			if (val->reserved)
				vmw_resource_unreserve(val->res,
						       val->dirty_set,
						       val->dirty,
						       val->switching_guest_memory_bo,
						       val->new_guest_memory_bo,
						       val->new_guest_memory_offset);
		}
}

 
static int vmw_validation_bo_validate_single(struct ttm_buffer_object *bo,
					     bool interruptible)
{
	struct vmw_bo *vbo = to_vmw_bo(&bo->base);
	struct ttm_operation_ctx ctx = {
		.interruptible = interruptible,
		.no_wait_gpu = false
	};
	int ret;

	if (atomic_read(&vbo->cpu_writers))
		return -EBUSY;

	if (vbo->tbo.pin_count > 0)
		return 0;

	ret = ttm_bo_validate(bo, &vbo->placement, &ctx);
	if (ret == 0 || ret == -ERESTARTSYS)
		return ret;

	 
	ctx.allow_res_evict = true;

	return ttm_bo_validate(bo, &vbo->placement, &ctx);
}

 
int vmw_validation_bo_validate(struct vmw_validation_context *ctx, bool intr)
{
	struct vmw_validation_bo_node *entry;
	int ret;

	list_for_each_entry(entry, &ctx->bo_list, base.head) {
		struct vmw_bo *vbo = to_vmw_bo(&entry->base.bo->base);

		ret = vmw_validation_bo_validate_single(entry->base.bo, intr);

		if (ret)
			return ret;

		 
		if (entry->coherent_count) {
			unsigned int coherent_count = entry->coherent_count;

			while (coherent_count) {
				ret = vmw_bo_dirty_add(vbo);
				if (ret)
					return ret;

				coherent_count--;
			}
			entry->coherent_count -= coherent_count;
		}

		if (vbo->dirty)
			vmw_bo_dirty_scan(vbo);
	}
	return 0;
}

 
int vmw_validation_res_validate(struct vmw_validation_context *ctx, bool intr)
{
	struct vmw_validation_res_node *val;
	int ret;

	list_for_each_entry(val, &ctx->resource_list, head) {
		struct vmw_resource *res = val->res;
		struct vmw_bo *backup = res->guest_memory_bo;

		ret = vmw_resource_validate(res, intr, val->dirty_set &&
					    val->dirty);
		if (ret) {
			if (ret != -ERESTARTSYS)
				DRM_ERROR("Failed to validate resource.\n");
			return ret;
		}

		 
		if (backup && res->guest_memory_bo && backup != res->guest_memory_bo) {
			struct vmw_bo *vbo = res->guest_memory_bo;

			vmw_bo_placement_set(vbo, res->func->domain,
					     res->func->busy_domain);
			ret = vmw_validation_add_bo(ctx, vbo);
			if (ret)
				return ret;
		}
	}
	return 0;
}

 
void vmw_validation_drop_ht(struct vmw_validation_context *ctx)
{
	struct vmw_validation_bo_node *entry;
	struct vmw_validation_res_node *val;

	if (!ctx->sw_context)
		return;

	list_for_each_entry(entry, &ctx->bo_list, base.head)
		hash_del_rcu(&entry->hash.head);

	list_for_each_entry(val, &ctx->resource_list, head)
		hash_del_rcu(&val->hash.head);

	list_for_each_entry(val, &ctx->resource_ctx_list, head)
		hash_del_rcu(&entry->hash.head);

	ctx->sw_context = NULL;
}

 
void vmw_validation_unref_lists(struct vmw_validation_context *ctx)
{
	struct vmw_validation_bo_node *entry;
	struct vmw_validation_res_node *val;

	list_for_each_entry(entry, &ctx->bo_list, base.head) {
		ttm_bo_put(entry->base.bo);
		entry->base.bo = NULL;
	}

	list_splice_init(&ctx->resource_ctx_list, &ctx->resource_list);
	list_for_each_entry(val, &ctx->resource_list, head)
		vmw_resource_unreference(&val->res);

	 
	INIT_LIST_HEAD(&ctx->bo_list);
	INIT_LIST_HEAD(&ctx->resource_list);

	vmw_validation_mem_free(ctx);
}

 
int vmw_validation_prepare(struct vmw_validation_context *ctx,
			   struct mutex *mutex,
			   bool intr)
{
	int ret = 0;

	if (mutex) {
		if (intr)
			ret = mutex_lock_interruptible(mutex);
		else
			mutex_lock(mutex);
		if (ret)
			return -ERESTARTSYS;
	}

	ctx->res_mutex = mutex;
	ret = vmw_validation_res_reserve(ctx, intr);
	if (ret)
		goto out_no_res_reserve;

	ret = vmw_validation_bo_reserve(ctx, intr);
	if (ret)
		goto out_no_bo_reserve;

	ret = vmw_validation_bo_validate(ctx, intr);
	if (ret)
		goto out_no_validate;

	ret = vmw_validation_res_validate(ctx, intr);
	if (ret)
		goto out_no_validate;

	return 0;

out_no_validate:
	vmw_validation_bo_backoff(ctx);
out_no_bo_reserve:
	vmw_validation_res_unreserve(ctx, true);
out_no_res_reserve:
	if (mutex)
		mutex_unlock(mutex);

	return ret;
}

 
void vmw_validation_revert(struct vmw_validation_context *ctx)
{
	vmw_validation_bo_backoff(ctx);
	vmw_validation_res_unreserve(ctx, true);
	if (ctx->res_mutex)
		mutex_unlock(ctx->res_mutex);
	vmw_validation_unref_lists(ctx);
}

 
void vmw_validation_done(struct vmw_validation_context *ctx,
			 struct vmw_fence_obj *fence)
{
	vmw_validation_bo_fence(ctx, fence);
	vmw_validation_res_unreserve(ctx, false);
	if (ctx->res_mutex)
		mutex_unlock(ctx->res_mutex);
	vmw_validation_unref_lists(ctx);
}

 
int vmw_validation_preload_bo(struct vmw_validation_context *ctx)
{
	unsigned int size = sizeof(struct vmw_validation_bo_node);

	if (!vmw_validation_mem_alloc(ctx, size))
		return -ENOMEM;

	ctx->mem_size_left += size;
	return 0;
}

 
int vmw_validation_preload_res(struct vmw_validation_context *ctx,
			       unsigned int size)
{
	size = vmw_validation_align(sizeof(struct vmw_validation_res_node) +
				    size) +
		vmw_validation_align(sizeof(struct vmw_validation_bo_node));
	if (!vmw_validation_mem_alloc(ctx, size))
		return -ENOMEM;

	ctx->mem_size_left += size;
	return 0;
}

 
void vmw_validation_bo_backoff(struct vmw_validation_context *ctx)
{
	struct vmw_validation_bo_node *entry;

	 
	list_for_each_entry(entry, &ctx->bo_list, base.head) {
		if (entry->coherent_count) {
			unsigned int coherent_count = entry->coherent_count;
			struct vmw_bo *vbo = to_vmw_bo(&entry->base.bo->base);

			while (coherent_count--)
				vmw_bo_dirty_release(vbo);
		}
	}

	ttm_eu_backoff_reservation(&ctx->ticket, &ctx->bo_list);
}
