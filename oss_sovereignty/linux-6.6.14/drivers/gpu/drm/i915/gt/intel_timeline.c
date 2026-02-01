
 

#include <drm/drm_cache.h>

#include "gem/i915_gem_internal.h"

#include "i915_active.h"
#include "i915_drv.h"
#include "i915_syncmap.h"
#include "intel_gt.h"
#include "intel_ring.h"
#include "intel_timeline.h"

#define TIMELINE_SEQNO_BYTES 8

static struct i915_vma *hwsp_alloc(struct intel_gt *gt)
{
	struct drm_i915_private *i915 = gt->i915;
	struct drm_i915_gem_object *obj;
	struct i915_vma *vma;

	obj = i915_gem_object_create_internal(i915, PAGE_SIZE);
	if (IS_ERR(obj))
		return ERR_CAST(obj);

	i915_gem_object_set_cache_coherency(obj, I915_CACHE_LLC);

	vma = i915_vma_instance(obj, &gt->ggtt->vm, NULL);
	if (IS_ERR(vma))
		i915_gem_object_put(obj);

	return vma;
}

static void __timeline_retire(struct i915_active *active)
{
	struct intel_timeline *tl =
		container_of(active, typeof(*tl), active);

	i915_vma_unpin(tl->hwsp_ggtt);
	intel_timeline_put(tl);
}

static int __timeline_active(struct i915_active *active)
{
	struct intel_timeline *tl =
		container_of(active, typeof(*tl), active);

	__i915_vma_pin(tl->hwsp_ggtt);
	intel_timeline_get(tl);
	return 0;
}

I915_SELFTEST_EXPORT int
intel_timeline_pin_map(struct intel_timeline *timeline)
{
	struct drm_i915_gem_object *obj = timeline->hwsp_ggtt->obj;
	u32 ofs = offset_in_page(timeline->hwsp_offset);
	void *vaddr;

	vaddr = i915_gem_object_pin_map(obj, I915_MAP_WB);
	if (IS_ERR(vaddr))
		return PTR_ERR(vaddr);

	timeline->hwsp_map = vaddr;
	timeline->hwsp_seqno = memset(vaddr + ofs, 0, TIMELINE_SEQNO_BYTES);
	drm_clflush_virt_range(vaddr + ofs, TIMELINE_SEQNO_BYTES);

	return 0;
}

static int intel_timeline_init(struct intel_timeline *timeline,
			       struct intel_gt *gt,
			       struct i915_vma *hwsp,
			       unsigned int offset)
{
	kref_init(&timeline->kref);
	atomic_set(&timeline->pin_count, 0);

	timeline->gt = gt;

	if (hwsp) {
		timeline->hwsp_offset = offset;
		timeline->hwsp_ggtt = i915_vma_get(hwsp);
	} else {
		timeline->has_initial_breadcrumb = true;
		hwsp = hwsp_alloc(gt);
		if (IS_ERR(hwsp))
			return PTR_ERR(hwsp);
		timeline->hwsp_ggtt = hwsp;
	}

	timeline->hwsp_map = NULL;
	timeline->hwsp_seqno = (void *)(long)timeline->hwsp_offset;

	GEM_BUG_ON(timeline->hwsp_offset >= hwsp->size);

	timeline->fence_context = dma_fence_context_alloc(1);

	mutex_init(&timeline->mutex);

	INIT_ACTIVE_FENCE(&timeline->last_request);
	INIT_LIST_HEAD(&timeline->requests);

	i915_syncmap_init(&timeline->sync);
	i915_active_init(&timeline->active, __timeline_active,
			 __timeline_retire, 0);

	return 0;
}

void intel_gt_init_timelines(struct intel_gt *gt)
{
	struct intel_gt_timelines *timelines = &gt->timelines;

	spin_lock_init(&timelines->lock);
	INIT_LIST_HEAD(&timelines->active_list);
}

static void intel_timeline_fini(struct rcu_head *rcu)
{
	struct intel_timeline *timeline =
		container_of(rcu, struct intel_timeline, rcu);

	if (timeline->hwsp_map)
		i915_gem_object_unpin_map(timeline->hwsp_ggtt->obj);

	i915_vma_put(timeline->hwsp_ggtt);
	i915_active_fini(&timeline->active);

	 
	i915_syncmap_free(&timeline->sync);

	kfree(timeline);
}

struct intel_timeline *
__intel_timeline_create(struct intel_gt *gt,
			struct i915_vma *global_hwsp,
			unsigned int offset)
{
	struct intel_timeline *timeline;
	int err;

	timeline = kzalloc(sizeof(*timeline), GFP_KERNEL);
	if (!timeline)
		return ERR_PTR(-ENOMEM);

	err = intel_timeline_init(timeline, gt, global_hwsp, offset);
	if (err) {
		kfree(timeline);
		return ERR_PTR(err);
	}

	return timeline;
}

struct intel_timeline *
intel_timeline_create_from_engine(struct intel_engine_cs *engine,
				  unsigned int offset)
{
	struct i915_vma *hwsp = engine->status_page.vma;
	struct intel_timeline *tl;

	tl = __intel_timeline_create(engine->gt, hwsp, offset);
	if (IS_ERR(tl))
		return tl;

	 
	mutex_lock(&hwsp->vm->mutex);
	list_add_tail(&tl->engine_link, &engine->status_page.timelines);
	mutex_unlock(&hwsp->vm->mutex);

	return tl;
}

void __intel_timeline_pin(struct intel_timeline *tl)
{
	GEM_BUG_ON(!atomic_read(&tl->pin_count));
	atomic_inc(&tl->pin_count);
}

int intel_timeline_pin(struct intel_timeline *tl, struct i915_gem_ww_ctx *ww)
{
	int err;

	if (atomic_add_unless(&tl->pin_count, 1, 0))
		return 0;

	if (!tl->hwsp_map) {
		err = intel_timeline_pin_map(tl);
		if (err)
			return err;
	}

	err = i915_ggtt_pin(tl->hwsp_ggtt, ww, 0, PIN_HIGH);
	if (err)
		return err;

	tl->hwsp_offset =
		i915_ggtt_offset(tl->hwsp_ggtt) +
		offset_in_page(tl->hwsp_offset);
	GT_TRACE(tl->gt, "timeline:%llx using HWSP offset:%x\n",
		 tl->fence_context, tl->hwsp_offset);

	i915_active_acquire(&tl->active);
	if (atomic_fetch_inc(&tl->pin_count)) {
		i915_active_release(&tl->active);
		__i915_vma_unpin(tl->hwsp_ggtt);
	}

	return 0;
}

void intel_timeline_reset_seqno(const struct intel_timeline *tl)
{
	u32 *hwsp_seqno = (u32 *)tl->hwsp_seqno;
	 
	GEM_BUG_ON(!atomic_read(&tl->pin_count));

	memset(hwsp_seqno + 1, 0, TIMELINE_SEQNO_BYTES - sizeof(*hwsp_seqno));
	WRITE_ONCE(*hwsp_seqno, tl->seqno);
	drm_clflush_virt_range(hwsp_seqno, TIMELINE_SEQNO_BYTES);
}

void intel_timeline_enter(struct intel_timeline *tl)
{
	struct intel_gt_timelines *timelines = &tl->gt->timelines;

	 
	lockdep_assert_held(&tl->mutex);

	if (atomic_add_unless(&tl->active_count, 1, 0))
		return;

	spin_lock(&timelines->lock);
	if (!atomic_fetch_inc(&tl->active_count)) {
		 
		intel_timeline_reset_seqno(tl);
		list_add_tail(&tl->link, &timelines->active_list);
	}
	spin_unlock(&timelines->lock);
}

void intel_timeline_exit(struct intel_timeline *tl)
{
	struct intel_gt_timelines *timelines = &tl->gt->timelines;

	 
	lockdep_assert_held(&tl->mutex);

	GEM_BUG_ON(!atomic_read(&tl->active_count));
	if (atomic_add_unless(&tl->active_count, -1, 1))
		return;

	spin_lock(&timelines->lock);
	if (atomic_dec_and_test(&tl->active_count))
		list_del(&tl->link);
	spin_unlock(&timelines->lock);

	 
	i915_syncmap_free(&tl->sync);
}

static u32 timeline_advance(struct intel_timeline *tl)
{
	GEM_BUG_ON(!atomic_read(&tl->pin_count));
	GEM_BUG_ON(tl->seqno & tl->has_initial_breadcrumb);

	return tl->seqno += 1 + tl->has_initial_breadcrumb;
}

static noinline int
__intel_timeline_get_seqno(struct intel_timeline *tl,
			   u32 *seqno)
{
	u32 next_ofs = offset_in_page(tl->hwsp_offset + TIMELINE_SEQNO_BYTES);

	 
	if (TIMELINE_SEQNO_BYTES <= BIT(5) && (next_ofs & BIT(5)))
		next_ofs = offset_in_page(next_ofs + BIT(5));

	tl->hwsp_offset = i915_ggtt_offset(tl->hwsp_ggtt) + next_ofs;
	tl->hwsp_seqno = tl->hwsp_map + next_ofs;
	intel_timeline_reset_seqno(tl);

	*seqno = timeline_advance(tl);
	GEM_BUG_ON(i915_seqno_passed(*tl->hwsp_seqno, *seqno));
	return 0;
}

int intel_timeline_get_seqno(struct intel_timeline *tl,
			     struct i915_request *rq,
			     u32 *seqno)
{
	*seqno = timeline_advance(tl);

	 
	if (unlikely(!*seqno && tl->has_initial_breadcrumb))
		return __intel_timeline_get_seqno(tl, seqno);

	return 0;
}

int intel_timeline_read_hwsp(struct i915_request *from,
			     struct i915_request *to,
			     u32 *hwsp)
{
	struct intel_timeline *tl;
	int err;

	rcu_read_lock();
	tl = rcu_dereference(from->timeline);
	if (i915_request_signaled(from) ||
	    !i915_active_acquire_if_busy(&tl->active))
		tl = NULL;

	if (tl) {
		 
		*hwsp = i915_ggtt_offset(tl->hwsp_ggtt) +
			offset_in_page(from->hwsp_seqno);
	}

	 
	if (tl && __i915_request_is_complete(from)) {
		i915_active_release(&tl->active);
		tl = NULL;
	}
	rcu_read_unlock();

	if (!tl)
		return 1;

	 
	if (!tl->has_initial_breadcrumb) {
		err = -EINVAL;
		goto out;
	}

	err = i915_active_add_request(&tl->active, to);

out:
	i915_active_release(&tl->active);
	return err;
}

void intel_timeline_unpin(struct intel_timeline *tl)
{
	GEM_BUG_ON(!atomic_read(&tl->pin_count));
	if (!atomic_dec_and_test(&tl->pin_count))
		return;

	i915_active_release(&tl->active);
	__i915_vma_unpin(tl->hwsp_ggtt);
}

void __intel_timeline_free(struct kref *kref)
{
	struct intel_timeline *timeline =
		container_of(kref, typeof(*timeline), kref);

	GEM_BUG_ON(atomic_read(&timeline->pin_count));
	GEM_BUG_ON(!list_empty(&timeline->requests));
	GEM_BUG_ON(timeline->retire);

	call_rcu(&timeline->rcu, intel_timeline_fini);
}

void intel_gt_fini_timelines(struct intel_gt *gt)
{
	struct intel_gt_timelines *timelines = &gt->timelines;

	GEM_BUG_ON(!list_empty(&timelines->active_list));
}

void intel_gt_show_timelines(struct intel_gt *gt,
			     struct drm_printer *m,
			     void (*show_request)(struct drm_printer *m,
						  const struct i915_request *rq,
						  const char *prefix,
						  int indent))
{
	struct intel_gt_timelines *timelines = &gt->timelines;
	struct intel_timeline *tl, *tn;
	LIST_HEAD(free);

	spin_lock(&timelines->lock);
	list_for_each_entry_safe(tl, tn, &timelines->active_list, link) {
		unsigned long count, ready, inflight;
		struct i915_request *rq, *rn;
		struct dma_fence *fence;

		if (!mutex_trylock(&tl->mutex)) {
			drm_printf(m, "Timeline %llx: busy; skipping\n",
				   tl->fence_context);
			continue;
		}

		intel_timeline_get(tl);
		GEM_BUG_ON(!atomic_read(&tl->active_count));
		atomic_inc(&tl->active_count);  
		spin_unlock(&timelines->lock);

		count = 0;
		ready = 0;
		inflight = 0;
		list_for_each_entry_safe(rq, rn, &tl->requests, link) {
			if (i915_request_completed(rq))
				continue;

			count++;
			if (i915_request_is_ready(rq))
				ready++;
			if (i915_request_is_active(rq))
				inflight++;
		}

		drm_printf(m, "Timeline %llx: { ", tl->fence_context);
		drm_printf(m, "count: %lu, ready: %lu, inflight: %lu",
			   count, ready, inflight);
		drm_printf(m, ", seqno: { current: %d, last: %d }",
			   *tl->hwsp_seqno, tl->seqno);
		fence = i915_active_fence_get(&tl->last_request);
		if (fence) {
			drm_printf(m, ", engine: %s",
				   to_request(fence)->engine->name);
			dma_fence_put(fence);
		}
		drm_printf(m, " }\n");

		if (show_request) {
			list_for_each_entry_safe(rq, rn, &tl->requests, link)
				show_request(m, rq, "", 2);
		}

		mutex_unlock(&tl->mutex);
		spin_lock(&timelines->lock);

		 
		list_safe_reset_next(tl, tn, link);
		if (atomic_dec_and_test(&tl->active_count))
			list_del(&tl->link);

		 
		if (refcount_dec_and_test(&tl->kref.refcount)) {
			GEM_BUG_ON(atomic_read(&tl->active_count));
			list_add(&tl->link, &free);
		}
	}
	spin_unlock(&timelines->lock);

	list_for_each_entry_safe(tl, tn, &free, link)
		__intel_timeline_free(&tl->kref);
}

#if IS_ENABLED(CONFIG_DRM_I915_SELFTEST)
#include "gt/selftests/mock_timeline.c"
#include "gt/selftest_timeline.c"
#endif
