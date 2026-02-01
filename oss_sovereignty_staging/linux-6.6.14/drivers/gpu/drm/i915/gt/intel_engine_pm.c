
 

#include "i915_drv.h"

#include "intel_breadcrumbs.h"
#include "intel_context.h"
#include "intel_engine.h"
#include "intel_engine_heartbeat.h"
#include "intel_engine_pm.h"
#include "intel_gt.h"
#include "intel_gt_pm.h"
#include "intel_rc6.h"
#include "intel_ring.h"
#include "shmem_utils.h"
#include "intel_gt_regs.h"

static void intel_gsc_idle_msg_enable(struct intel_engine_cs *engine)
{
	struct drm_i915_private *i915 = engine->i915;

	if (IS_METEORLAKE(i915) && engine->id == GSC0) {
		intel_uncore_write(engine->gt->uncore,
				   RC_PSMI_CTRL_GSCCS,
				   _MASKED_BIT_DISABLE(IDLE_MSG_DISABLE));
		 
		intel_uncore_write(engine->gt->uncore,
				   PWRCTX_MAXCNT_GSCCS,
				   0xA);
	}
}

static void dbg_poison_ce(struct intel_context *ce)
{
	if (!IS_ENABLED(CONFIG_DRM_I915_DEBUG_GEM))
		return;

	if (ce->state) {
		struct drm_i915_gem_object *obj = ce->state->obj;
		int type = intel_gt_coherent_map_type(ce->engine->gt, obj, true);
		void *map;

		if (!i915_gem_object_trylock(obj, NULL))
			return;

		map = i915_gem_object_pin_map(obj, type);
		if (!IS_ERR(map)) {
			memset(map, CONTEXT_REDZONE, obj->base.size);
			i915_gem_object_flush_map(obj);
			i915_gem_object_unpin_map(obj);
		}
		i915_gem_object_unlock(obj);
	}
}

static int __engine_unpark(struct intel_wakeref *wf)
{
	struct intel_engine_cs *engine =
		container_of(wf, typeof(*engine), wakeref);
	struct intel_context *ce;

	ENGINE_TRACE(engine, "\n");

	intel_gt_pm_get(engine->gt);

	 
	ce = engine->kernel_context;
	if (ce) {
		GEM_BUG_ON(test_bit(CONTEXT_VALID_BIT, &ce->flags));

		 
		while (unlikely(intel_context_inflight(ce)))
			intel_engine_flush_submission(engine);

		 
		dbg_poison_ce(ce);

		 
		ce->ops->reset(ce);

		CE_TRACE(ce, "reset { seqno:%x, *hwsp:%x, ring:%x }\n",
			 ce->timeline->seqno,
			 READ_ONCE(*ce->timeline->hwsp_seqno),
			 ce->ring->emit);
		GEM_BUG_ON(ce->timeline->seqno !=
			   READ_ONCE(*ce->timeline->hwsp_seqno));
	}

	if (engine->unpark)
		engine->unpark(engine);

	intel_breadcrumbs_unpark(engine->breadcrumbs);
	intel_engine_unpark_heartbeat(engine);
	return 0;
}

static void duration(struct dma_fence *fence, struct dma_fence_cb *cb)
{
	struct i915_request *rq = to_request(fence);

	ewma__engine_latency_add(&rq->engine->latency,
				 ktime_us_delta(rq->fence.timestamp,
						rq->duration.emitted));
}

static void
__queue_and_release_pm(struct i915_request *rq,
		       struct intel_timeline *tl,
		       struct intel_engine_cs *engine)
{
	struct intel_gt_timelines *timelines = &engine->gt->timelines;

	ENGINE_TRACE(engine, "parking\n");

	 
	GEM_BUG_ON(rq->context->active_count != 1);
	__intel_gt_pm_get(engine->gt);

	 
	spin_lock(&timelines->lock);

	 
	if (!atomic_fetch_inc(&tl->active_count))
		list_add_tail(&tl->link, &timelines->active_list);

	 
	__i915_request_queue_bh(rq);

	 
	__intel_wakeref_defer_park(&engine->wakeref);

	spin_unlock(&timelines->lock);
}

static bool switch_to_kernel_context(struct intel_engine_cs *engine)
{
	struct intel_context *ce = engine->kernel_context;
	struct i915_request *rq;
	bool result = true;

	 
	if (intel_engine_uses_guc(engine))
		return true;

	 
	if (intel_gt_is_wedged(engine->gt))
		return true;

	GEM_BUG_ON(!intel_context_is_barrier(ce));
	GEM_BUG_ON(ce->timeline->hwsp_ggtt != engine->status_page.vma);

	 
	if (engine->wakeref_serial == engine->serial)
		return true;

	 
	set_bit(CONTEXT_IS_PARKING, &ce->flags);
	GEM_BUG_ON(atomic_read(&ce->timeline->active_count) < 0);

	rq = __i915_request_create(ce, GFP_NOWAIT);
	if (IS_ERR(rq))
		 
		goto out_unlock;

	 
	engine->wakeref_serial = engine->serial + 1;
	i915_request_add_active_barriers(rq);

	 
	rq->sched.attr.priority = I915_PRIORITY_BARRIER;
	if (likely(!__i915_request_commit(rq))) {  
		 
		BUILD_BUG_ON(sizeof(rq->duration) > sizeof(rq->submitq));
		dma_fence_add_callback(&rq->fence, &rq->duration.cb, duration);
		rq->duration.emitted = ktime_get();
	}

	 
	__queue_and_release_pm(rq, ce->timeline, engine);

	result = false;
out_unlock:
	clear_bit(CONTEXT_IS_PARKING, &ce->flags);
	return result;
}

static void call_idle_barriers(struct intel_engine_cs *engine)
{
	struct llist_node *node, *next;

	llist_for_each_safe(node, next, llist_del_all(&engine->barrier_tasks)) {
		struct dma_fence_cb *cb =
			container_of((struct list_head *)node,
				     typeof(*cb), node);

		cb->func(ERR_PTR(-EAGAIN), cb);
	}
}

static int __engine_park(struct intel_wakeref *wf)
{
	struct intel_engine_cs *engine =
		container_of(wf, typeof(*engine), wakeref);

	engine->saturated = 0;

	 
	if (!switch_to_kernel_context(engine))
		return -EBUSY;

	ENGINE_TRACE(engine, "parked\n");

	call_idle_barriers(engine);  

	intel_engine_park_heartbeat(engine);
	intel_breadcrumbs_park(engine->breadcrumbs);

	 
	GEM_BUG_ON(engine->sched_engine->queue_priority_hint != INT_MIN);

	if (engine->park)
		engine->park(engine);

	 
	intel_gt_pm_put_async(engine->gt);
	return 0;
}

static const struct intel_wakeref_ops wf_ops = {
	.get = __engine_unpark,
	.put = __engine_park,
};

void intel_engine_init__pm(struct intel_engine_cs *engine)
{
	intel_wakeref_init(&engine->wakeref, engine->i915, &wf_ops);
	intel_engine_init_heartbeat(engine);

	intel_gsc_idle_msg_enable(engine);
}

 
void intel_engine_reset_pinned_contexts(struct intel_engine_cs *engine)
{
	struct intel_context *ce;

	list_for_each_entry(ce, &engine->pinned_contexts_list,
			    pinned_contexts_link) {
		 
		if (ce == engine->kernel_context)
			continue;

		dbg_poison_ce(ce);
		ce->ops->reset(ce);
	}
}

#if IS_ENABLED(CONFIG_DRM_I915_SELFTEST)
#include "selftest_engine_pm.c"
#endif
