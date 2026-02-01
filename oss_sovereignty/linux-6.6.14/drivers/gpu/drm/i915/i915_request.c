 

#include <linux/dma-fence-array.h>
#include <linux/dma-fence-chain.h>
#include <linux/irq_work.h>
#include <linux/prefetch.h>
#include <linux/sched.h>
#include <linux/sched/clock.h>
#include <linux/sched/signal.h>
#include <linux/sched/mm.h>

#include "gem/i915_gem_context.h"
#include "gt/intel_breadcrumbs.h"
#include "gt/intel_context.h"
#include "gt/intel_engine.h"
#include "gt/intel_engine_heartbeat.h"
#include "gt/intel_engine_regs.h"
#include "gt/intel_gpu_commands.h"
#include "gt/intel_reset.h"
#include "gt/intel_ring.h"
#include "gt/intel_rps.h"

#include "i915_active.h"
#include "i915_config.h"
#include "i915_deps.h"
#include "i915_driver.h"
#include "i915_drv.h"
#include "i915_trace.h"

struct execute_cb {
	struct irq_work work;
	struct i915_sw_fence *fence;
	struct i915_request *signal;
};

static struct kmem_cache *slab_requests;
static struct kmem_cache *slab_execute_cbs;

static const char *i915_fence_get_driver_name(struct dma_fence *fence)
{
	return dev_name(to_request(fence)->i915->drm.dev);
}

static const char *i915_fence_get_timeline_name(struct dma_fence *fence)
{
	const struct i915_gem_context *ctx;

	 
	if (test_bit(DMA_FENCE_FLAG_SIGNALED_BIT, &fence->flags))
		return "signaled";

	ctx = i915_request_gem_context(to_request(fence));
	if (!ctx)
		return "[" DRIVER_NAME "]";

	return ctx->name;
}

static bool i915_fence_signaled(struct dma_fence *fence)
{
	return i915_request_completed(to_request(fence));
}

static bool i915_fence_enable_signaling(struct dma_fence *fence)
{
	return i915_request_enable_breadcrumb(to_request(fence));
}

static signed long i915_fence_wait(struct dma_fence *fence,
				   bool interruptible,
				   signed long timeout)
{
	return i915_request_wait_timeout(to_request(fence),
					 interruptible | I915_WAIT_PRIORITY,
					 timeout);
}

struct kmem_cache *i915_request_slab_cache(void)
{
	return slab_requests;
}

static void i915_fence_release(struct dma_fence *fence)
{
	struct i915_request *rq = to_request(fence);

	GEM_BUG_ON(rq->guc_prio != GUC_PRIO_INIT &&
		   rq->guc_prio != GUC_PRIO_FINI);

	i915_request_free_capture_list(fetch_and_zero(&rq->capture_list));
	if (rq->batch_res) {
		i915_vma_resource_put(rq->batch_res);
		rq->batch_res = NULL;
	}

	 
	i915_sw_fence_fini(&rq->submit);
	i915_sw_fence_fini(&rq->semaphore);

	 
	if (is_power_of_2(rq->execution_mask) &&
	    !cmpxchg(&rq->engine->request_pool, NULL, rq))
		return;

	kmem_cache_free(slab_requests, rq);
}

const struct dma_fence_ops i915_fence_ops = {
	.get_driver_name = i915_fence_get_driver_name,
	.get_timeline_name = i915_fence_get_timeline_name,
	.enable_signaling = i915_fence_enable_signaling,
	.signaled = i915_fence_signaled,
	.wait = i915_fence_wait,
	.release = i915_fence_release,
};

static void irq_execute_cb(struct irq_work *wrk)
{
	struct execute_cb *cb = container_of(wrk, typeof(*cb), work);

	i915_sw_fence_complete(cb->fence);
	kmem_cache_free(slab_execute_cbs, cb);
}

static __always_inline void
__notify_execute_cb(struct i915_request *rq, bool (*fn)(struct irq_work *wrk))
{
	struct execute_cb *cb, *cn;

	if (llist_empty(&rq->execute_cb))
		return;

	llist_for_each_entry_safe(cb, cn,
				  llist_del_all(&rq->execute_cb),
				  work.node.llist)
		fn(&cb->work);
}

static void __notify_execute_cb_irq(struct i915_request *rq)
{
	__notify_execute_cb(rq, irq_work_queue);
}

static bool irq_work_imm(struct irq_work *wrk)
{
	wrk->func(wrk);
	return false;
}

void i915_request_notify_execute_cb_imm(struct i915_request *rq)
{
	__notify_execute_cb(rq, irq_work_imm);
}

static void __i915_request_fill(struct i915_request *rq, u8 val)
{
	void *vaddr = rq->ring->vaddr;
	u32 head;

	head = rq->infix;
	if (rq->postfix < head) {
		memset(vaddr + head, val, rq->ring->size - head);
		head = 0;
	}
	memset(vaddr + head, val, rq->postfix - head);
}

 
bool
i915_request_active_engine(struct i915_request *rq,
			   struct intel_engine_cs **active)
{
	struct intel_engine_cs *engine, *locked;
	bool ret = false;

	 
	locked = READ_ONCE(rq->engine);
	spin_lock_irq(&locked->sched_engine->lock);
	while (unlikely(locked != (engine = READ_ONCE(rq->engine)))) {
		spin_unlock(&locked->sched_engine->lock);
		locked = engine;
		spin_lock(&locked->sched_engine->lock);
	}

	if (i915_request_is_active(rq)) {
		if (!__i915_request_is_complete(rq))
			*active = locked;
		ret = true;
	}

	spin_unlock_irq(&locked->sched_engine->lock);

	return ret;
}

static void __rq_init_watchdog(struct i915_request *rq)
{
	rq->watchdog.timer.function = NULL;
}

static enum hrtimer_restart __rq_watchdog_expired(struct hrtimer *hrtimer)
{
	struct i915_request *rq =
		container_of(hrtimer, struct i915_request, watchdog.timer);
	struct intel_gt *gt = rq->engine->gt;

	if (!i915_request_completed(rq)) {
		if (llist_add(&rq->watchdog.link, &gt->watchdog.list))
			queue_work(gt->i915->unordered_wq, &gt->watchdog.work);
	} else {
		i915_request_put(rq);
	}

	return HRTIMER_NORESTART;
}

static void __rq_arm_watchdog(struct i915_request *rq)
{
	struct i915_request_watchdog *wdg = &rq->watchdog;
	struct intel_context *ce = rq->context;

	if (!ce->watchdog.timeout_us)
		return;

	i915_request_get(rq);

	hrtimer_init(&wdg->timer, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
	wdg->timer.function = __rq_watchdog_expired;
	hrtimer_start_range_ns(&wdg->timer,
			       ns_to_ktime(ce->watchdog.timeout_us *
					   NSEC_PER_USEC),
			       NSEC_PER_MSEC,
			       HRTIMER_MODE_REL);
}

static void __rq_cancel_watchdog(struct i915_request *rq)
{
	struct i915_request_watchdog *wdg = &rq->watchdog;

	if (wdg->timer.function && hrtimer_try_to_cancel(&wdg->timer) > 0)
		i915_request_put(rq);
}

#if IS_ENABLED(CONFIG_DRM_I915_CAPTURE_ERROR)

 
void i915_request_free_capture_list(struct i915_capture_list *capture)
{
	while (capture) {
		struct i915_capture_list *next = capture->next;

		i915_vma_resource_put(capture->vma_res);
		kfree(capture);
		capture = next;
	}
}

#define assert_capture_list_is_null(_rq) GEM_BUG_ON((_rq)->capture_list)

#define clear_capture_list(_rq) ((_rq)->capture_list = NULL)

#else

#define i915_request_free_capture_list(_a) do {} while (0)

#define assert_capture_list_is_null(_a) do {} while (0)

#define clear_capture_list(_rq) do {} while (0)

#endif

bool i915_request_retire(struct i915_request *rq)
{
	if (!__i915_request_is_complete(rq))
		return false;

	RQ_TRACE(rq, "\n");

	GEM_BUG_ON(!i915_sw_fence_signaled(&rq->submit));
	trace_i915_request_retire(rq);
	i915_request_mark_complete(rq);

	__rq_cancel_watchdog(rq);

	 
	GEM_BUG_ON(!list_is_first(&rq->link,
				  &i915_request_timeline(rq)->requests));
	if (IS_ENABLED(CONFIG_DRM_I915_DEBUG_GEM))
		 
		__i915_request_fill(rq, POISON_FREE);
	rq->ring->head = rq->postfix;

	if (!i915_request_signaled(rq)) {
		spin_lock_irq(&rq->lock);
		dma_fence_signal_locked(&rq->fence);
		spin_unlock_irq(&rq->lock);
	}

	if (test_and_set_bit(I915_FENCE_FLAG_BOOST, &rq->fence.flags))
		intel_rps_dec_waiters(&rq->engine->gt->rps);

	 
	rq->engine->remove_active_request(rq);
	GEM_BUG_ON(!llist_empty(&rq->execute_cb));

	__list_del_entry(&rq->link);  

	intel_context_exit(rq->context);
	intel_context_unpin(rq->context);

	i915_sched_node_fini(&rq->sched);
	i915_request_put(rq);

	return true;
}

void i915_request_retire_upto(struct i915_request *rq)
{
	struct intel_timeline * const tl = i915_request_timeline(rq);
	struct i915_request *tmp;

	RQ_TRACE(rq, "\n");
	GEM_BUG_ON(!__i915_request_is_complete(rq));

	do {
		tmp = list_first_entry(&tl->requests, typeof(*tmp), link);
		GEM_BUG_ON(!i915_request_completed(tmp));
	} while (i915_request_retire(tmp) && tmp != rq);
}

static struct i915_request * const *
__engine_active(struct intel_engine_cs *engine)
{
	return READ_ONCE(engine->execlists.active);
}

static bool __request_in_flight(const struct i915_request *signal)
{
	struct i915_request * const *port, *rq;
	bool inflight = false;

	if (!i915_request_is_ready(signal))
		return false;

	 
	if (!intel_context_inflight(signal->context))
		return false;

	rcu_read_lock();
	for (port = __engine_active(signal->engine);
	     (rq = READ_ONCE(*port));  
	     port++) {
		if (rq->context == signal->context) {
			inflight = i915_seqno_passed(rq->fence.seqno,
						     signal->fence.seqno);
			break;
		}
	}
	rcu_read_unlock();

	return inflight;
}

static int
__await_execution(struct i915_request *rq,
		  struct i915_request *signal,
		  gfp_t gfp)
{
	struct execute_cb *cb;

	if (i915_request_is_active(signal))
		return 0;

	cb = kmem_cache_alloc(slab_execute_cbs, gfp);
	if (!cb)
		return -ENOMEM;

	cb->fence = &rq->submit;
	i915_sw_fence_await(cb->fence);
	init_irq_work(&cb->work, irq_execute_cb);

	 
	if (llist_add(&cb->work.node.llist, &signal->execute_cb)) {
		if (i915_request_is_active(signal) ||
		    __request_in_flight(signal))
			i915_request_notify_execute_cb_imm(signal);
	}

	return 0;
}

static bool fatal_error(int error)
{
	switch (error) {
	case 0:  
	case -EAGAIN:  
	case -ETIMEDOUT:  
		return false;
	default:
		return true;
	}
}

void __i915_request_skip(struct i915_request *rq)
{
	GEM_BUG_ON(!fatal_error(rq->fence.error));

	if (rq->infix == rq->postfix)
		return;

	RQ_TRACE(rq, "error: %d\n", rq->fence.error);

	 
	__i915_request_fill(rq, 0);
	rq->infix = rq->postfix;
}

bool i915_request_set_error_once(struct i915_request *rq, int error)
{
	int old;

	GEM_BUG_ON(!IS_ERR_VALUE((long)error));

	if (i915_request_signaled(rq))
		return false;

	old = READ_ONCE(rq->fence.error);
	do {
		if (fatal_error(old))
			return false;
	} while (!try_cmpxchg(&rq->fence.error, &old, error));

	return true;
}

struct i915_request *i915_request_mark_eio(struct i915_request *rq)
{
	if (__i915_request_is_complete(rq))
		return NULL;

	GEM_BUG_ON(i915_request_signaled(rq));

	 
	rq = i915_request_get(rq);

	i915_request_set_error_once(rq, -EIO);
	i915_request_mark_complete(rq);

	return rq;
}

bool __i915_request_submit(struct i915_request *request)
{
	struct intel_engine_cs *engine = request->engine;
	bool result = false;

	RQ_TRACE(request, "\n");

	GEM_BUG_ON(!irqs_disabled());
	lockdep_assert_held(&engine->sched_engine->lock);

	 
	if (__i915_request_is_complete(request)) {
		list_del_init(&request->sched.link);
		goto active;
	}

	if (unlikely(!intel_context_is_schedulable(request->context)))
		i915_request_set_error_once(request, -EIO);

	if (unlikely(fatal_error(request->fence.error)))
		__i915_request_skip(request);

	 
	if (request->sched.semaphores &&
	    i915_sw_fence_signaled(&request->semaphore))
		engine->saturated |= request->sched.semaphores;

	engine->emit_fini_breadcrumb(request,
				     request->ring->vaddr + request->postfix);

	trace_i915_request_execute(request);
	if (engine->bump_serial)
		engine->bump_serial(engine);
	else
		engine->serial++;

	result = true;

	GEM_BUG_ON(test_bit(I915_FENCE_FLAG_ACTIVE, &request->fence.flags));
	engine->add_active_request(request);
active:
	clear_bit(I915_FENCE_FLAG_PQUEUE, &request->fence.flags);
	set_bit(I915_FENCE_FLAG_ACTIVE, &request->fence.flags);

	 
	__notify_execute_cb_irq(request);

	 
	if (test_bit(DMA_FENCE_FLAG_ENABLE_SIGNAL_BIT, &request->fence.flags))
		i915_request_enable_breadcrumb(request);

	return result;
}

void i915_request_submit(struct i915_request *request)
{
	struct intel_engine_cs *engine = request->engine;
	unsigned long flags;

	 
	spin_lock_irqsave(&engine->sched_engine->lock, flags);

	__i915_request_submit(request);

	spin_unlock_irqrestore(&engine->sched_engine->lock, flags);
}

void __i915_request_unsubmit(struct i915_request *request)
{
	struct intel_engine_cs *engine = request->engine;

	 
	RQ_TRACE(request, "\n");

	GEM_BUG_ON(!irqs_disabled());
	lockdep_assert_held(&engine->sched_engine->lock);

	 
	GEM_BUG_ON(!test_bit(I915_FENCE_FLAG_ACTIVE, &request->fence.flags));
	clear_bit_unlock(I915_FENCE_FLAG_ACTIVE, &request->fence.flags);
	if (test_bit(DMA_FENCE_FLAG_ENABLE_SIGNAL_BIT, &request->fence.flags))
		i915_request_cancel_breadcrumb(request);

	 
	if (request->sched.semaphores && __i915_request_has_started(request))
		request->sched.semaphores = 0;

	 
}

void i915_request_unsubmit(struct i915_request *request)
{
	struct intel_engine_cs *engine = request->engine;
	unsigned long flags;

	 
	spin_lock_irqsave(&engine->sched_engine->lock, flags);

	__i915_request_unsubmit(request);

	spin_unlock_irqrestore(&engine->sched_engine->lock, flags);
}

void i915_request_cancel(struct i915_request *rq, int error)
{
	if (!i915_request_set_error_once(rq, error))
		return;

	set_bit(I915_FENCE_FLAG_SENTINEL, &rq->fence.flags);

	intel_context_cancel_request(rq->context, rq);
}

static int
submit_notify(struct i915_sw_fence *fence, enum i915_sw_fence_notify state)
{
	struct i915_request *request =
		container_of(fence, typeof(*request), submit);

	switch (state) {
	case FENCE_COMPLETE:
		trace_i915_request_submit(request);

		if (unlikely(fence->error))
			i915_request_set_error_once(request, fence->error);
		else
			__rq_arm_watchdog(request);

		 
		rcu_read_lock();
		request->engine->submit_request(request);
		rcu_read_unlock();
		break;

	case FENCE_FREE:
		i915_request_put(request);
		break;
	}

	return NOTIFY_DONE;
}

static int
semaphore_notify(struct i915_sw_fence *fence, enum i915_sw_fence_notify state)
{
	struct i915_request *rq = container_of(fence, typeof(*rq), semaphore);

	switch (state) {
	case FENCE_COMPLETE:
		break;

	case FENCE_FREE:
		i915_request_put(rq);
		break;
	}

	return NOTIFY_DONE;
}

static void retire_requests(struct intel_timeline *tl)
{
	struct i915_request *rq, *rn;

	list_for_each_entry_safe(rq, rn, &tl->requests, link)
		if (!i915_request_retire(rq))
			break;
}

static noinline struct i915_request *
request_alloc_slow(struct intel_timeline *tl,
		   struct i915_request **rsvd,
		   gfp_t gfp)
{
	struct i915_request *rq;

	 
	if (!gfpflags_allow_blocking(gfp)) {
		rq = xchg(rsvd, NULL);
		if (!rq)  
			goto out;

		return rq;
	}

	if (list_empty(&tl->requests))
		goto out;

	 
	rq = list_first_entry(&tl->requests, typeof(*rq), link);
	i915_request_retire(rq);

	rq = kmem_cache_alloc(slab_requests,
			      gfp | __GFP_RETRY_MAYFAIL | __GFP_NOWARN);
	if (rq)
		return rq;

	 
	rq = list_last_entry(&tl->requests, typeof(*rq), link);
	cond_synchronize_rcu(rq->rcustate);

	 
	retire_requests(tl);

out:
	return kmem_cache_alloc(slab_requests, gfp);
}

static void __i915_request_ctor(void *arg)
{
	struct i915_request *rq = arg;

	spin_lock_init(&rq->lock);
	i915_sched_node_init(&rq->sched);
	i915_sw_fence_init(&rq->submit, submit_notify);
	i915_sw_fence_init(&rq->semaphore, semaphore_notify);

	clear_capture_list(rq);
	rq->batch_res = NULL;

	init_llist_head(&rq->execute_cb);
}

#if IS_ENABLED(CONFIG_DRM_I915_SELFTEST)
#define clear_batch_ptr(_rq) ((_rq)->batch = NULL)
#else
#define clear_batch_ptr(_a) do {} while (0)
#endif

struct i915_request *
__i915_request_create(struct intel_context *ce, gfp_t gfp)
{
	struct intel_timeline *tl = ce->timeline;
	struct i915_request *rq;
	u32 seqno;
	int ret;

	might_alloc(gfp);

	 
	__intel_context_pin(ce);

	 
	rq = kmem_cache_alloc(slab_requests,
			      gfp | __GFP_RETRY_MAYFAIL | __GFP_NOWARN);
	if (unlikely(!rq)) {
		rq = request_alloc_slow(tl, &ce->engine->request_pool, gfp);
		if (!rq) {
			ret = -ENOMEM;
			goto err_unreserve;
		}
	}

	rq->context = ce;
	rq->engine = ce->engine;
	rq->ring = ce->ring;
	rq->execution_mask = ce->engine->mask;
	rq->i915 = ce->engine->i915;

	ret = intel_timeline_get_seqno(tl, rq, &seqno);
	if (ret)
		goto err_free;

	dma_fence_init(&rq->fence, &i915_fence_ops, &rq->lock,
		       tl->fence_context, seqno);

	RCU_INIT_POINTER(rq->timeline, tl);
	rq->hwsp_seqno = tl->hwsp_seqno;
	GEM_BUG_ON(__i915_request_is_complete(rq));

	rq->rcustate = get_state_synchronize_rcu();  

	rq->guc_prio = GUC_PRIO_INIT;

	 
	i915_sw_fence_reinit(&i915_request_get(rq)->submit);
	i915_sw_fence_reinit(&i915_request_get(rq)->semaphore);

	i915_sched_node_reinit(&rq->sched);

	 
	clear_batch_ptr(rq);
	__rq_init_watchdog(rq);
	assert_capture_list_is_null(rq);
	GEM_BUG_ON(!llist_empty(&rq->execute_cb));
	GEM_BUG_ON(rq->batch_res);

	 
	rq->reserved_space =
		2 * rq->engine->emit_fini_breadcrumb_dw * sizeof(u32);

	 
	rq->head = rq->ring->emit;

	ret = rq->engine->request_alloc(rq);
	if (ret)
		goto err_unwind;

	rq->infix = rq->ring->emit;  

	intel_context_mark_active(ce);
	list_add_tail_rcu(&rq->link, &tl->requests);

	return rq;

err_unwind:
	ce->ring->emit = rq->head;

	 
	GEM_BUG_ON(!list_empty(&rq->sched.signalers_list));
	GEM_BUG_ON(!list_empty(&rq->sched.waiters_list));

err_free:
	kmem_cache_free(slab_requests, rq);
err_unreserve:
	intel_context_unpin(ce);
	return ERR_PTR(ret);
}

struct i915_request *
i915_request_create(struct intel_context *ce)
{
	struct i915_request *rq;
	struct intel_timeline *tl;

	tl = intel_context_timeline_lock(ce);
	if (IS_ERR(tl))
		return ERR_CAST(tl);

	 
	rq = list_first_entry(&tl->requests, typeof(*rq), link);
	if (!list_is_last(&rq->link, &tl->requests))
		i915_request_retire(rq);

	intel_context_enter(ce);
	rq = __i915_request_create(ce, GFP_KERNEL);
	intel_context_exit(ce);  
	if (IS_ERR(rq))
		goto err_unlock;

	 
	rq->cookie = lockdep_pin_lock(&tl->mutex);

	return rq;

err_unlock:
	intel_context_timeline_unlock(tl);
	return rq;
}

static int
i915_request_await_start(struct i915_request *rq, struct i915_request *signal)
{
	struct dma_fence *fence;
	int err;

	if (i915_request_timeline(rq) == rcu_access_pointer(signal->timeline))
		return 0;

	if (i915_request_started(signal))
		return 0;

	 
	fence = NULL;
	rcu_read_lock();
	do {
		struct list_head *pos = READ_ONCE(signal->link.prev);
		struct i915_request *prev;

		 
		if (unlikely(__i915_request_has_started(signal)))
			break;

		 
		if (pos == &rcu_dereference(signal->timeline)->requests)
			break;

		 
		prev = list_entry(pos, typeof(*prev), link);
		if (!i915_request_get_rcu(prev))
			break;

		 
		if (unlikely(READ_ONCE(prev->link.next) != &signal->link)) {
			i915_request_put(prev);
			break;
		}

		fence = &prev->fence;
	} while (0);
	rcu_read_unlock();
	if (!fence)
		return 0;

	err = 0;
	if (!intel_timeline_sync_is_later(i915_request_timeline(rq), fence))
		err = i915_sw_fence_await_dma_fence(&rq->submit,
						    fence, 0,
						    I915_FENCE_GFP);
	dma_fence_put(fence);

	return err;
}

static intel_engine_mask_t
already_busywaiting(struct i915_request *rq)
{
	 
	return rq->sched.semaphores | READ_ONCE(rq->engine->saturated);
}

static int
__emit_semaphore_wait(struct i915_request *to,
		      struct i915_request *from,
		      u32 seqno)
{
	const int has_token = GRAPHICS_VER(to->engine->i915) >= 12;
	u32 hwsp_offset;
	int len, err;
	u32 *cs;

	GEM_BUG_ON(GRAPHICS_VER(to->engine->i915) < 8);
	GEM_BUG_ON(i915_request_has_initial_breadcrumb(to));

	 
	err = intel_timeline_read_hwsp(from, to, &hwsp_offset);
	if (err)
		return err;

	len = 4;
	if (has_token)
		len += 2;

	cs = intel_ring_begin(to, len);
	if (IS_ERR(cs))
		return PTR_ERR(cs);

	 
	*cs++ = (MI_SEMAPHORE_WAIT |
		 MI_SEMAPHORE_GLOBAL_GTT |
		 MI_SEMAPHORE_POLL |
		 MI_SEMAPHORE_SAD_GTE_SDD) +
		has_token;
	*cs++ = seqno;
	*cs++ = hwsp_offset;
	*cs++ = 0;
	if (has_token) {
		*cs++ = 0;
		*cs++ = MI_NOOP;
	}

	intel_ring_advance(to, cs);
	return 0;
}

static bool
can_use_semaphore_wait(struct i915_request *to, struct i915_request *from)
{
	return to->engine->gt->ggtt == from->engine->gt->ggtt;
}

static int
emit_semaphore_wait(struct i915_request *to,
		    struct i915_request *from,
		    gfp_t gfp)
{
	const intel_engine_mask_t mask = READ_ONCE(from->engine)->mask;
	struct i915_sw_fence *wait = &to->submit;

	if (!can_use_semaphore_wait(to, from))
		goto await_fence;

	if (!intel_context_use_semaphores(to->context))
		goto await_fence;

	if (i915_request_has_initial_breadcrumb(to))
		goto await_fence;

	 
	if (from->sched.flags & I915_SCHED_HAS_EXTERNAL_CHAIN)
		goto await_fence;

	 
	if (already_busywaiting(to) & mask)
		goto await_fence;

	if (i915_request_await_start(to, from) < 0)
		goto await_fence;

	 
	if (__await_execution(to, from, gfp))
		goto await_fence;

	if (__emit_semaphore_wait(to, from, from->fence.seqno))
		goto await_fence;

	to->sched.semaphores |= mask;
	wait = &to->semaphore;

await_fence:
	return i915_sw_fence_await_dma_fence(wait,
					     &from->fence, 0,
					     I915_FENCE_GFP);
}

static bool intel_timeline_sync_has_start(struct intel_timeline *tl,
					  struct dma_fence *fence)
{
	return __intel_timeline_sync_is_later(tl,
					      fence->context,
					      fence->seqno - 1);
}

static int intel_timeline_sync_set_start(struct intel_timeline *tl,
					 const struct dma_fence *fence)
{
	return __intel_timeline_sync_set(tl, fence->context, fence->seqno - 1);
}

static int
__i915_request_await_execution(struct i915_request *to,
			       struct i915_request *from)
{
	int err;

	GEM_BUG_ON(intel_context_is_barrier(from->context));

	 
	err = __await_execution(to, from, I915_FENCE_GFP);
	if (err)
		return err;

	 
	if (intel_timeline_sync_has_start(i915_request_timeline(to),
					  &from->fence))
		return 0;

	 
	err = i915_request_await_start(to, from);
	if (err < 0)
		return err;

	 
	if (can_use_semaphore_wait(to, from) &&
	    intel_engine_has_semaphores(to->engine) &&
	    !i915_request_has_initial_breadcrumb(to)) {
		err = __emit_semaphore_wait(to, from, from->fence.seqno - 1);
		if (err < 0)
			return err;
	}

	 
	if (to->engine->sched_engine->schedule) {
		err = i915_sched_node_add_dependency(&to->sched,
						     &from->sched,
						     I915_DEPENDENCY_WEAK);
		if (err < 0)
			return err;
	}

	return intel_timeline_sync_set_start(i915_request_timeline(to),
					     &from->fence);
}

static void mark_external(struct i915_request *rq)
{
	 
	rq->sched.flags |= I915_SCHED_HAS_EXTERNAL_CHAIN;
}

static int
__i915_request_await_external(struct i915_request *rq, struct dma_fence *fence)
{
	mark_external(rq);
	return i915_sw_fence_await_dma_fence(&rq->submit, fence,
					     i915_fence_context_timeout(rq->i915,
									fence->context),
					     I915_FENCE_GFP);
}

static int
i915_request_await_external(struct i915_request *rq, struct dma_fence *fence)
{
	struct dma_fence *iter;
	int err = 0;

	if (!to_dma_fence_chain(fence))
		return __i915_request_await_external(rq, fence);

	dma_fence_chain_for_each(iter, fence) {
		struct dma_fence_chain *chain = to_dma_fence_chain(iter);

		if (!dma_fence_is_i915(chain->fence)) {
			err = __i915_request_await_external(rq, iter);
			break;
		}

		err = i915_request_await_dma_fence(rq, chain->fence);
		if (err < 0)
			break;
	}

	dma_fence_put(iter);
	return err;
}

static inline bool is_parallel_rq(struct i915_request *rq)
{
	return intel_context_is_parallel(rq->context);
}

static inline struct intel_context *request_to_parent(struct i915_request *rq)
{
	return intel_context_to_parent(rq->context);
}

static bool is_same_parallel_context(struct i915_request *to,
				     struct i915_request *from)
{
	if (is_parallel_rq(to))
		return request_to_parent(to) == request_to_parent(from);

	return false;
}

int
i915_request_await_execution(struct i915_request *rq,
			     struct dma_fence *fence)
{
	struct dma_fence **child = &fence;
	unsigned int nchild = 1;
	int ret;

	if (dma_fence_is_array(fence)) {
		struct dma_fence_array *array = to_dma_fence_array(fence);

		 

		child = array->fences;
		nchild = array->num_fences;
		GEM_BUG_ON(!nchild);
	}

	do {
		fence = *child++;
		if (test_bit(DMA_FENCE_FLAG_SIGNALED_BIT, &fence->flags))
			continue;

		if (fence->context == rq->fence.context)
			continue;

		 

		if (dma_fence_is_i915(fence)) {
			if (is_same_parallel_context(rq, to_request(fence)))
				continue;
			ret = __i915_request_await_execution(rq,
							     to_request(fence));
		} else {
			ret = i915_request_await_external(rq, fence);
		}
		if (ret < 0)
			return ret;
	} while (--nchild);

	return 0;
}

static int
await_request_submit(struct i915_request *to, struct i915_request *from)
{
	 
	if (to->engine == READ_ONCE(from->engine))
		return i915_sw_fence_await_sw_fence_gfp(&to->submit,
							&from->submit,
							I915_FENCE_GFP);
	else
		return __i915_request_await_execution(to, from);
}

static int
i915_request_await_request(struct i915_request *to, struct i915_request *from)
{
	int ret;

	GEM_BUG_ON(to == from);
	GEM_BUG_ON(to->timeline == from->timeline);

	if (i915_request_completed(from)) {
		i915_sw_fence_set_error_once(&to->submit, from->fence.error);
		return 0;
	}

	if (to->engine->sched_engine->schedule) {
		ret = i915_sched_node_add_dependency(&to->sched,
						     &from->sched,
						     I915_DEPENDENCY_EXTERNAL);
		if (ret < 0)
			return ret;
	}

	if (!intel_engine_uses_guc(to->engine) &&
	    is_power_of_2(to->execution_mask | READ_ONCE(from->execution_mask)))
		ret = await_request_submit(to, from);
	else
		ret = emit_semaphore_wait(to, from, I915_FENCE_GFP);
	if (ret < 0)
		return ret;

	return 0;
}

int
i915_request_await_dma_fence(struct i915_request *rq, struct dma_fence *fence)
{
	struct dma_fence **child = &fence;
	unsigned int nchild = 1;
	int ret;

	 
	if (dma_fence_is_array(fence)) {
		struct dma_fence_array *array = to_dma_fence_array(fence);

		child = array->fences;
		nchild = array->num_fences;
		GEM_BUG_ON(!nchild);
	}

	do {
		fence = *child++;
		if (test_bit(DMA_FENCE_FLAG_SIGNALED_BIT, &fence->flags))
			continue;

		 
		if (fence->context == rq->fence.context)
			continue;

		 
		if (fence->context &&
		    intel_timeline_sync_is_later(i915_request_timeline(rq),
						 fence))
			continue;

		if (dma_fence_is_i915(fence)) {
			if (is_same_parallel_context(rq, to_request(fence)))
				continue;
			ret = i915_request_await_request(rq, to_request(fence));
		} else {
			ret = i915_request_await_external(rq, fence);
		}
		if (ret < 0)
			return ret;

		 
		if (fence->context)
			intel_timeline_sync_set(i915_request_timeline(rq),
						fence);
	} while (--nchild);

	return 0;
}

 
int i915_request_await_deps(struct i915_request *rq, const struct i915_deps *deps)
{
	int i, err;

	for (i = 0; i < deps->num_deps; ++i) {
		err = i915_request_await_dma_fence(rq, deps->fences[i]);
		if (err)
			return err;
	}

	return 0;
}

 
int
i915_request_await_object(struct i915_request *to,
			  struct drm_i915_gem_object *obj,
			  bool write)
{
	struct dma_resv_iter cursor;
	struct dma_fence *fence;
	int ret = 0;

	dma_resv_for_each_fence(&cursor, obj->base.resv,
				dma_resv_usage_rw(write), fence) {
		ret = i915_request_await_dma_fence(to, fence);
		if (ret)
			break;
	}

	return ret;
}

static void i915_request_await_huc(struct i915_request *rq)
{
	struct intel_huc *huc = &rq->context->engine->gt->uc.huc;

	 
	if (!rcu_access_pointer(rq->context->gem_context))
		return;

	if (intel_huc_wait_required(huc))
		i915_sw_fence_await_sw_fence(&rq->submit,
					     &huc->delayed_load.fence,
					     &rq->hucq);
}

static struct i915_request *
__i915_request_ensure_parallel_ordering(struct i915_request *rq,
					struct intel_timeline *timeline)
{
	struct i915_request *prev;

	GEM_BUG_ON(!is_parallel_rq(rq));

	prev = request_to_parent(rq)->parallel.last_rq;
	if (prev) {
		if (!__i915_request_is_complete(prev)) {
			i915_sw_fence_await_sw_fence(&rq->submit,
						     &prev->submit,
						     &rq->submitq);

			if (rq->engine->sched_engine->schedule)
				__i915_sched_node_add_dependency(&rq->sched,
								 &prev->sched,
								 &rq->dep,
								 0);
		}
		i915_request_put(prev);
	}

	request_to_parent(rq)->parallel.last_rq = i915_request_get(rq);

	 
	return to_request(__i915_active_fence_set(&timeline->last_request,
						  &rq->fence));
}

static struct i915_request *
__i915_request_ensure_ordering(struct i915_request *rq,
			       struct intel_timeline *timeline)
{
	struct i915_request *prev;

	GEM_BUG_ON(is_parallel_rq(rq));

	prev = to_request(__i915_active_fence_set(&timeline->last_request,
						  &rq->fence));

	if (prev && !__i915_request_is_complete(prev)) {
		bool uses_guc = intel_engine_uses_guc(rq->engine);
		bool pow2 = is_power_of_2(READ_ONCE(prev->engine)->mask |
					  rq->engine->mask);
		bool same_context = prev->context == rq->context;

		 
		GEM_BUG_ON(same_context &&
			   i915_seqno_passed(prev->fence.seqno,
					     rq->fence.seqno));

		if ((same_context && uses_guc) || (!uses_guc && pow2))
			i915_sw_fence_await_sw_fence(&rq->submit,
						     &prev->submit,
						     &rq->submitq);
		else
			__i915_sw_fence_await_dma_fence(&rq->submit,
							&prev->fence,
							&rq->dmaq);
		if (rq->engine->sched_engine->schedule)
			__i915_sched_node_add_dependency(&rq->sched,
							 &prev->sched,
							 &rq->dep,
							 0);
	}

	 
	return prev;
}

static struct i915_request *
__i915_request_add_to_timeline(struct i915_request *rq)
{
	struct intel_timeline *timeline = i915_request_timeline(rq);
	struct i915_request *prev;

	 
	if (rq->engine->class == VIDEO_DECODE_CLASS)
		i915_request_await_huc(rq);

	 
	if (likely(!is_parallel_rq(rq)))
		prev = __i915_request_ensure_ordering(rq, timeline);
	else
		prev = __i915_request_ensure_parallel_ordering(rq, timeline);
	if (prev)
		i915_request_put(prev);

	 
	GEM_BUG_ON(timeline->seqno != rq->fence.seqno);

	return prev;
}

 
struct i915_request *__i915_request_commit(struct i915_request *rq)
{
	struct intel_engine_cs *engine = rq->engine;
	struct intel_ring *ring = rq->ring;
	u32 *cs;

	RQ_TRACE(rq, "\n");

	 
	GEM_BUG_ON(rq->reserved_space > ring->space);
	rq->reserved_space = 0;
	rq->emitted_jiffies = jiffies;

	 
	cs = intel_ring_begin(rq, engine->emit_fini_breadcrumb_dw);
	GEM_BUG_ON(IS_ERR(cs));
	rq->postfix = intel_ring_offset(rq, cs);

	return __i915_request_add_to_timeline(rq);
}

void __i915_request_queue_bh(struct i915_request *rq)
{
	i915_sw_fence_commit(&rq->semaphore);
	i915_sw_fence_commit(&rq->submit);
}

void __i915_request_queue(struct i915_request *rq,
			  const struct i915_sched_attr *attr)
{
	 
	if (attr && rq->engine->sched_engine->schedule)
		rq->engine->sched_engine->schedule(rq, attr);

	local_bh_disable();
	__i915_request_queue_bh(rq);
	local_bh_enable();  
}

void i915_request_add(struct i915_request *rq)
{
	struct intel_timeline * const tl = i915_request_timeline(rq);
	struct i915_sched_attr attr = {};
	struct i915_gem_context *ctx;

	lockdep_assert_held(&tl->mutex);
	lockdep_unpin_lock(&tl->mutex, rq->cookie);

	trace_i915_request_add(rq);
	__i915_request_commit(rq);

	 
	rcu_read_lock();
	ctx = rcu_dereference(rq->context->gem_context);
	if (ctx)
		attr = ctx->sched;
	rcu_read_unlock();

	__i915_request_queue(rq, &attr);

	mutex_unlock(&tl->mutex);
}

static unsigned long local_clock_ns(unsigned int *cpu)
{
	unsigned long t;

	 
	*cpu = get_cpu();
	t = local_clock();
	put_cpu();

	return t;
}

static bool busywait_stop(unsigned long timeout, unsigned int cpu)
{
	unsigned int this_cpu;

	if (time_after(local_clock_ns(&this_cpu), timeout))
		return true;

	return this_cpu != cpu;
}

static bool __i915_spin_request(struct i915_request * const rq, int state)
{
	unsigned long timeout_ns;
	unsigned int cpu;

	 
	if (!i915_request_is_running(rq))
		return false;

	 

	timeout_ns = READ_ONCE(rq->engine->props.max_busywait_duration_ns);
	timeout_ns += local_clock_ns(&cpu);
	do {
		if (dma_fence_is_signaled(&rq->fence))
			return true;

		if (signal_pending_state(state, current))
			break;

		if (busywait_stop(timeout_ns, cpu))
			break;

		cpu_relax();
	} while (!need_resched());

	return false;
}

struct request_wait {
	struct dma_fence_cb cb;
	struct task_struct *tsk;
};

static void request_wait_wake(struct dma_fence *fence, struct dma_fence_cb *cb)
{
	struct request_wait *wait = container_of(cb, typeof(*wait), cb);

	wake_up_process(fetch_and_zero(&wait->tsk));
}

 
long i915_request_wait_timeout(struct i915_request *rq,
			       unsigned int flags,
			       long timeout)
{
	const int state = flags & I915_WAIT_INTERRUPTIBLE ?
		TASK_INTERRUPTIBLE : TASK_UNINTERRUPTIBLE;
	struct request_wait wait;

	might_sleep();
	GEM_BUG_ON(timeout < 0);

	if (dma_fence_is_signaled(&rq->fence))
		return timeout ?: 1;

	if (!timeout)
		return -ETIME;

	trace_i915_request_wait_begin(rq, flags);

	 
	mutex_acquire(&rq->engine->gt->reset.mutex.dep_map, 0, 0, _THIS_IP_);

	 
	if (CONFIG_DRM_I915_MAX_REQUEST_BUSYWAIT &&
	    __i915_spin_request(rq, state))
		goto out;

	 
	if (flags & I915_WAIT_PRIORITY && !i915_request_started(rq))
		intel_rps_boost(rq);

	wait.tsk = current;
	if (dma_fence_add_callback(&rq->fence, &wait.cb, request_wait_wake))
		goto out;

	 
	if (i915_request_is_ready(rq))
		__intel_engine_flush_submission(rq->engine, false);

	for (;;) {
		set_current_state(state);

		if (dma_fence_is_signaled(&rq->fence))
			break;

		if (signal_pending_state(state, current)) {
			timeout = -ERESTARTSYS;
			break;
		}

		if (!timeout) {
			timeout = -ETIME;
			break;
		}

		timeout = io_schedule_timeout(timeout);
	}
	__set_current_state(TASK_RUNNING);

	if (READ_ONCE(wait.tsk))
		dma_fence_remove_callback(&rq->fence, &wait.cb);
	GEM_BUG_ON(!list_empty(&wait.cb.node));

out:
	mutex_release(&rq->engine->gt->reset.mutex.dep_map, _THIS_IP_);
	trace_i915_request_wait_end(rq);
	return timeout;
}

 
long i915_request_wait(struct i915_request *rq,
		       unsigned int flags,
		       long timeout)
{
	long ret = i915_request_wait_timeout(rq, flags, timeout);

	if (!ret)
		return -ETIME;

	if (ret > 0 && !timeout)
		return 0;

	return ret;
}

static int print_sched_attr(const struct i915_sched_attr *attr,
			    char *buf, int x, int len)
{
	if (attr->priority == I915_PRIORITY_INVALID)
		return x;

	x += snprintf(buf + x, len - x,
		      " prio=%d", attr->priority);

	return x;
}

static char queue_status(const struct i915_request *rq)
{
	if (i915_request_is_active(rq))
		return 'E';

	if (i915_request_is_ready(rq))
		return intel_engine_is_virtual(rq->engine) ? 'V' : 'R';

	return 'U';
}

static const char *run_status(const struct i915_request *rq)
{
	if (__i915_request_is_complete(rq))
		return "!";

	if (__i915_request_has_started(rq))
		return "*";

	if (!i915_sw_fence_signaled(&rq->semaphore))
		return "&";

	return "";
}

static const char *fence_status(const struct i915_request *rq)
{
	if (test_bit(DMA_FENCE_FLAG_SIGNALED_BIT, &rq->fence.flags))
		return "+";

	if (test_bit(DMA_FENCE_FLAG_ENABLE_SIGNAL_BIT, &rq->fence.flags))
		return "-";

	return "";
}

void i915_request_show(struct drm_printer *m,
		       const struct i915_request *rq,
		       const char *prefix,
		       int indent)
{
	const char *name = rq->fence.ops->get_timeline_name((struct dma_fence *)&rq->fence);
	char buf[80] = "";
	int x = 0;

	 

	x = print_sched_attr(&rq->sched.attr, buf, x, sizeof(buf));

	drm_printf(m, "%s%.*s%c %llx:%lld%s%s %s @ %dms: %s\n",
		   prefix, indent, "                ",
		   queue_status(rq),
		   rq->fence.context, rq->fence.seqno,
		   run_status(rq),
		   fence_status(rq),
		   buf,
		   jiffies_to_msecs(jiffies - rq->emitted_jiffies),
		   name);
}

static bool engine_match_ring(struct intel_engine_cs *engine, struct i915_request *rq)
{
	u32 ring = ENGINE_READ(engine, RING_START);

	return ring == i915_ggtt_offset(rq->ring->vma);
}

static bool match_ring(struct i915_request *rq)
{
	struct intel_engine_cs *engine;
	bool found;
	int i;

	if (!intel_engine_is_virtual(rq->engine))
		return engine_match_ring(rq->engine, rq);

	found = false;
	i = 0;
	while ((engine = intel_engine_get_sibling(rq->engine, i++))) {
		found = engine_match_ring(engine, rq);
		if (found)
			break;
	}

	return found;
}

enum i915_request_state i915_test_request_state(struct i915_request *rq)
{
	if (i915_request_completed(rq))
		return I915_REQUEST_COMPLETE;

	if (!i915_request_started(rq))
		return I915_REQUEST_PENDING;

	if (match_ring(rq))
		return I915_REQUEST_ACTIVE;

	return I915_REQUEST_QUEUED;
}

#if IS_ENABLED(CONFIG_DRM_I915_SELFTEST)
#include "selftests/mock_request.c"
#include "selftests/i915_request.c"
#endif

void i915_request_module_exit(void)
{
	kmem_cache_destroy(slab_execute_cbs);
	kmem_cache_destroy(slab_requests);
}

int __init i915_request_module_init(void)
{
	slab_requests =
		kmem_cache_create("i915_request",
				  sizeof(struct i915_request),
				  __alignof__(struct i915_request),
				  SLAB_HWCACHE_ALIGN |
				  SLAB_RECLAIM_ACCOUNT |
				  SLAB_TYPESAFE_BY_RCU,
				  __i915_request_ctor);
	if (!slab_requests)
		return -ENOMEM;

	slab_execute_cbs = KMEM_CACHE(execute_cb,
					     SLAB_HWCACHE_ALIGN |
					     SLAB_RECLAIM_ACCOUNT |
					     SLAB_TYPESAFE_BY_RCU);
	if (!slab_execute_cbs)
		goto err_requests;

	return 0;

err_requests:
	kmem_cache_destroy(slab_requests);
	return -ENOMEM;
}
