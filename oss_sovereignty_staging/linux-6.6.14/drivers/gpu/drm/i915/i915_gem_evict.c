 

#include "gem/i915_gem_context.h"
#include "gt/intel_gt.h"
#include "gt/intel_gt_requests.h"

#include "i915_drv.h"
#include "i915_gem_evict.h"
#include "i915_trace.h"

I915_SELFTEST_DECLARE(static struct igt_evict_ctl {
	bool fail_if_busy:1;
} igt_evict_ctl;)

static bool dying_vma(struct i915_vma *vma)
{
	return !kref_read(&vma->obj->base.refcount);
}

static int ggtt_flush(struct i915_address_space *vm)
{
	struct i915_ggtt *ggtt = i915_vm_to_ggtt(vm);
	struct intel_gt *gt;
	int ret = 0;

	list_for_each_entry(gt, &ggtt->gt_list, ggtt_link) {
		 
		ret = intel_gt_wait_for_idle(gt, MAX_SCHEDULE_TIMEOUT);
		if (ret)
			return ret;
	}
	return ret;
}

static bool grab_vma(struct i915_vma *vma, struct i915_gem_ww_ctx *ww)
{
	 
	if (i915_gem_object_get_rcu(vma->obj)) {
		if (!i915_gem_object_trylock(vma->obj, ww)) {
			i915_gem_object_put(vma->obj);
			return false;
		}
	} else {
		 
		atomic_and(~I915_VMA_PIN_MASK, &vma->flags);
	}

	return true;
}

static void ungrab_vma(struct i915_vma *vma)
{
	if (dying_vma(vma))
		return;

	i915_gem_object_unlock(vma->obj);
	i915_gem_object_put(vma->obj);
}

static bool
mark_free(struct drm_mm_scan *scan,
	  struct i915_gem_ww_ctx *ww,
	  struct i915_vma *vma,
	  unsigned int flags,
	  struct list_head *unwind)
{
	if (i915_vma_is_pinned(vma))
		return false;

	if (!grab_vma(vma, ww))
		return false;

	list_add(&vma->evict_link, unwind);
	return drm_mm_scan_add_block(scan, &vma->node);
}

static bool defer_evict(struct i915_vma *vma)
{
	if (i915_vma_is_active(vma))
		return true;

	if (i915_vma_is_scanout(vma))
		return true;

	return false;
}

 
int
i915_gem_evict_something(struct i915_address_space *vm,
			 struct i915_gem_ww_ctx *ww,
			 u64 min_size, u64 alignment,
			 unsigned long color,
			 u64 start, u64 end,
			 unsigned flags)
{
	struct drm_mm_scan scan;
	struct list_head eviction_list;
	struct i915_vma *vma, *next;
	struct drm_mm_node *node;
	enum drm_mm_insert_mode mode;
	struct i915_vma *active;
	struct intel_gt *gt;
	int ret;

	lockdep_assert_held(&vm->mutex);
	trace_i915_gem_evict(vm, min_size, alignment, flags);

	 
	mode = DRM_MM_INSERT_BEST;
	if (flags & PIN_HIGH)
		mode = DRM_MM_INSERT_HIGH;
	if (flags & PIN_MAPPABLE)
		mode = DRM_MM_INSERT_LOW;
	drm_mm_scan_init_with_range(&scan, &vm->mm,
				    min_size, alignment, color,
				    start, end, mode);

	if (i915_is_ggtt(vm)) {
		struct i915_ggtt *ggtt = i915_vm_to_ggtt(vm);

		list_for_each_entry(gt, &ggtt->gt_list, ggtt_link)
			intel_gt_retire_requests(gt);
	} else {
		intel_gt_retire_requests(vm->gt);
	}

search_again:
	active = NULL;
	INIT_LIST_HEAD(&eviction_list);
	list_for_each_entry_safe(vma, next, &vm->bound_list, vm_link) {
		if (vma == active) {  
			if (flags & PIN_NONBLOCK)
				break;

			active = ERR_PTR(-EAGAIN);
		}

		 
		if (active != ERR_PTR(-EAGAIN) && defer_evict(vma)) {
			if (!active)
				active = vma;

			list_move_tail(&vma->vm_link, &vm->bound_list);
			continue;
		}

		if (mark_free(&scan, ww, vma, flags, &eviction_list))
			goto found;
	}

	 
	list_for_each_entry_safe(vma, next, &eviction_list, evict_link) {
		ret = drm_mm_scan_remove_block(&scan, &vma->node);
		BUG_ON(ret);
		ungrab_vma(vma);
	}

	 
	if (!i915_is_ggtt(vm) || flags & PIN_NONBLOCK)
		return -ENOSPC;

	 
	if (I915_SELFTEST_ONLY(igt_evict_ctl.fail_if_busy))
		return -EBUSY;

	ret = ggtt_flush(vm);
	if (ret)
		return ret;

	cond_resched();

	flags |= PIN_NONBLOCK;
	goto search_again;

found:
	 
	list_for_each_entry_safe(vma, next, &eviction_list, evict_link) {
		if (drm_mm_scan_remove_block(&scan, &vma->node)) {
			__i915_vma_pin(vma);
		} else {
			list_del(&vma->evict_link);
			ungrab_vma(vma);
		}
	}

	 
	ret = 0;
	list_for_each_entry_safe(vma, next, &eviction_list, evict_link) {
		__i915_vma_unpin(vma);
		if (ret == 0)
			ret = __i915_vma_unbind(vma);
		ungrab_vma(vma);
	}

	while (ret == 0 && (node = drm_mm_scan_color_evict(&scan))) {
		vma = container_of(node, struct i915_vma, node);

		 
		if (vma->node.color != I915_COLOR_UNEVICTABLE &&
		    grab_vma(vma, ww)) {
			ret = __i915_vma_unbind(vma);
			ungrab_vma(vma);
		} else {
			ret = -ENOSPC;
		}
	}

	return ret;
}

 
int i915_gem_evict_for_node(struct i915_address_space *vm,
			    struct i915_gem_ww_ctx *ww,
			    struct drm_mm_node *target,
			    unsigned int flags)
{
	LIST_HEAD(eviction_list);
	struct drm_mm_node *node;
	u64 start = target->start;
	u64 end = start + target->size;
	struct i915_vma *vma, *next;
	int ret = 0;

	lockdep_assert_held(&vm->mutex);
	GEM_BUG_ON(!IS_ALIGNED(start, I915_GTT_PAGE_SIZE));
	GEM_BUG_ON(!IS_ALIGNED(end, I915_GTT_PAGE_SIZE));

	trace_i915_gem_evict_node(vm, target, flags);

	 
	if (i915_is_ggtt(vm)) {
		struct i915_ggtt *ggtt = i915_vm_to_ggtt(vm);
		struct intel_gt *gt;

		list_for_each_entry(gt, &ggtt->gt_list, ggtt_link)
			intel_gt_retire_requests(gt);
	} else {
		intel_gt_retire_requests(vm->gt);
	}

	if (i915_vm_has_cache_coloring(vm)) {
		 
		if (start)
			start -= I915_GTT_PAGE_SIZE;

		 
		end += I915_GTT_PAGE_SIZE;
	}
	GEM_BUG_ON(start >= end);

	drm_mm_for_each_node_in_range(node, &vm->mm, start, end) {
		 
		if (node->color == I915_COLOR_UNEVICTABLE) {
			ret = -ENOSPC;
			break;
		}

		GEM_BUG_ON(!drm_mm_node_allocated(node));
		vma = container_of(node, typeof(*vma), node);

		 
		if (i915_vm_has_cache_coloring(vm)) {
			if (node->start + node->size == target->start) {
				if (node->color == target->color)
					continue;
			}
			if (node->start == target->start + target->size) {
				if (node->color == target->color)
					continue;
			}
		}

		if (i915_vma_is_pinned(vma)) {
			ret = -ENOSPC;
			break;
		}

		if (flags & PIN_NONBLOCK && i915_vma_is_active(vma)) {
			ret = -ENOSPC;
			break;
		}

		if (!grab_vma(vma, ww)) {
			ret = -ENOSPC;
			break;
		}

		 
		__i915_vma_pin(vma);
		list_add(&vma->evict_link, &eviction_list);
	}

	list_for_each_entry_safe(vma, next, &eviction_list, evict_link) {
		__i915_vma_unpin(vma);
		if (ret == 0)
			ret = __i915_vma_unbind(vma);

		ungrab_vma(vma);
	}

	return ret;
}

 
int i915_gem_evict_vm(struct i915_address_space *vm, struct i915_gem_ww_ctx *ww,
		      struct drm_i915_gem_object **busy_bo)
{
	int ret = 0;

	lockdep_assert_held(&vm->mutex);
	trace_i915_gem_evict_vm(vm);

	 
	if (i915_is_ggtt(vm)) {
		ret = ggtt_flush(vm);
		if (ret)
			return ret;
	}

	do {
		struct i915_vma *vma, *vn;
		LIST_HEAD(eviction_list);
		LIST_HEAD(locked_eviction_list);

		list_for_each_entry(vma, &vm->bound_list, vm_link) {
			if (i915_vma_is_pinned(vma))
				continue;

			 
			if (!i915_gem_object_get_rcu(vma->obj) ||
			    (ww && (dma_resv_locking_ctx(vma->obj->base.resv) == &ww->ctx))) {
				__i915_vma_pin(vma);
				list_add(&vma->evict_link, &locked_eviction_list);
				continue;
			}

			if (!i915_gem_object_trylock(vma->obj, ww)) {
				if (busy_bo) {
					*busy_bo = vma->obj;  
					ret = -EBUSY;
					break;
				}
				i915_gem_object_put(vma->obj);
				continue;
			}

			__i915_vma_pin(vma);
			list_add(&vma->evict_link, &eviction_list);
		}
		if (list_empty(&eviction_list) && list_empty(&locked_eviction_list))
			break;

		 
		list_for_each_entry_safe(vma, vn, &locked_eviction_list, evict_link) {
			__i915_vma_unpin(vma);

			if (ret == 0) {
				ret = __i915_vma_unbind(vma);
				if (ret != -EINTR)  
					ret = 0;
			}
			if (!dying_vma(vma))
				i915_gem_object_put(vma->obj);
		}

		list_for_each_entry_safe(vma, vn, &eviction_list, evict_link) {
			__i915_vma_unpin(vma);
			if (ret == 0) {
				ret = __i915_vma_unbind(vma);
				if (ret != -EINTR)  
					ret = 0;
			}

			i915_gem_object_unlock(vma->obj);
			i915_gem_object_put(vma->obj);
		}
	} while (ret == 0);

	return ret;
}

#if IS_ENABLED(CONFIG_DRM_I915_SELFTEST)
#include "selftests/i915_gem_evict.c"
#endif
