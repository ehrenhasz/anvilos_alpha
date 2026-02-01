
 

 
#include <linux/interrupt.h>
#include <linux/string_helpers.h>

#include "i915_drv.h"
#include "i915_reg.h"
#include "i915_trace.h"
#include "i915_vgpu.h"
#include "gen8_engine_cs.h"
#include "intel_breadcrumbs.h"
#include "intel_context.h"
#include "intel_engine_heartbeat.h"
#include "intel_engine_pm.h"
#include "intel_engine_regs.h"
#include "intel_engine_stats.h"
#include "intel_execlists_submission.h"
#include "intel_gt.h"
#include "intel_gt_irq.h"
#include "intel_gt_pm.h"
#include "intel_gt_regs.h"
#include "intel_gt_requests.h"
#include "intel_lrc.h"
#include "intel_lrc_reg.h"
#include "intel_mocs.h"
#include "intel_reset.h"
#include "intel_ring.h"
#include "intel_workarounds.h"
#include "shmem_utils.h"

#define RING_EXECLIST_QFULL		(1 << 0x2)
#define RING_EXECLIST1_VALID		(1 << 0x3)
#define RING_EXECLIST0_VALID		(1 << 0x4)
#define RING_EXECLIST_ACTIVE_STATUS	(3 << 0xE)
#define RING_EXECLIST1_ACTIVE		(1 << 0x11)
#define RING_EXECLIST0_ACTIVE		(1 << 0x12)

#define GEN8_CTX_STATUS_IDLE_ACTIVE	(1 << 0)
#define GEN8_CTX_STATUS_PREEMPTED	(1 << 1)
#define GEN8_CTX_STATUS_ELEMENT_SWITCH	(1 << 2)
#define GEN8_CTX_STATUS_ACTIVE_IDLE	(1 << 3)
#define GEN8_CTX_STATUS_COMPLETE	(1 << 4)
#define GEN8_CTX_STATUS_LITE_RESTORE	(1 << 15)

#define GEN8_CTX_STATUS_COMPLETED_MASK \
	 (GEN8_CTX_STATUS_COMPLETE | GEN8_CTX_STATUS_PREEMPTED)

#define GEN12_CTX_STATUS_SWITCHED_TO_NEW_QUEUE	(0x1)  
#define GEN12_CTX_SWITCH_DETAIL(csb_dw)	((csb_dw) & 0xF)  
#define GEN12_CSB_SW_CTX_ID_MASK		GENMASK(25, 15)
#define GEN12_IDLE_CTX_ID		0x7FF
#define GEN12_CSB_CTX_VALID(csb_dw) \
	(FIELD_GET(GEN12_CSB_SW_CTX_ID_MASK, csb_dw) != GEN12_IDLE_CTX_ID)

#define XEHP_CTX_STATUS_SWITCHED_TO_NEW_QUEUE	BIT(1)  
#define XEHP_CSB_SW_CTX_ID_MASK			GENMASK(31, 10)
#define XEHP_IDLE_CTX_ID			0xFFFF
#define XEHP_CSB_CTX_VALID(csb_dw) \
	(FIELD_GET(XEHP_CSB_SW_CTX_ID_MASK, csb_dw) != XEHP_IDLE_CTX_ID)

 
#define EXECLISTS_REQUEST_SIZE 64  

struct virtual_engine {
	struct intel_engine_cs base;
	struct intel_context context;
	struct rcu_work rcu;

	 
	struct i915_request *request;

	 
	struct ve_node {
		struct rb_node rb;
		int prio;
	} nodes[I915_NUM_ENGINES];

	 
	unsigned int num_siblings;
	struct intel_engine_cs *siblings[];
};

static struct virtual_engine *to_virtual_engine(struct intel_engine_cs *engine)
{
	GEM_BUG_ON(!intel_engine_is_virtual(engine));
	return container_of(engine, struct virtual_engine, base);
}

static struct intel_context *
execlists_create_virtual(struct intel_engine_cs **siblings, unsigned int count,
			 unsigned long flags);

static struct i915_request *
__active_request(const struct intel_timeline * const tl,
		 struct i915_request *rq,
		 int error)
{
	struct i915_request *active = rq;

	list_for_each_entry_from_reverse(rq, &tl->requests, link) {
		if (__i915_request_is_complete(rq))
			break;

		if (error) {
			i915_request_set_error_once(rq, error);
			__i915_request_skip(rq);
		}
		active = rq;
	}

	return active;
}

static struct i915_request *
active_request(const struct intel_timeline * const tl, struct i915_request *rq)
{
	return __active_request(tl, rq, 0);
}

static void ring_set_paused(const struct intel_engine_cs *engine, int state)
{
	 
	engine->status_page.addr[I915_GEM_HWS_PREEMPT] = state;
	if (state)
		wmb();
}

static struct i915_priolist *to_priolist(struct rb_node *rb)
{
	return rb_entry(rb, struct i915_priolist, node);
}

static int rq_prio(const struct i915_request *rq)
{
	return READ_ONCE(rq->sched.attr.priority);
}

static int effective_prio(const struct i915_request *rq)
{
	int prio = rq_prio(rq);

	 
	if (i915_request_has_nopreempt(rq))
		prio = I915_PRIORITY_UNPREEMPTABLE;

	return prio;
}

static int queue_prio(const struct i915_sched_engine *sched_engine)
{
	struct rb_node *rb;

	rb = rb_first_cached(&sched_engine->queue);
	if (!rb)
		return INT_MIN;

	return to_priolist(rb)->priority;
}

static int virtual_prio(const struct intel_engine_execlists *el)
{
	struct rb_node *rb = rb_first_cached(&el->virtual);

	return rb ? rb_entry(rb, struct ve_node, rb)->prio : INT_MIN;
}

static bool need_preempt(const struct intel_engine_cs *engine,
			 const struct i915_request *rq)
{
	int last_prio;

	if (!intel_engine_has_semaphores(engine))
		return false;

	 
	last_prio = max(effective_prio(rq), I915_PRIORITY_NORMAL - 1);
	if (engine->sched_engine->queue_priority_hint <= last_prio)
		return false;

	 
	if (!list_is_last(&rq->sched.link, &engine->sched_engine->requests) &&
	    rq_prio(list_next_entry(rq, sched.link)) > last_prio)
		return true;

	 
	return max(virtual_prio(&engine->execlists),
		   queue_prio(engine->sched_engine)) > last_prio;
}

__maybe_unused static bool
assert_priority_queue(const struct i915_request *prev,
		      const struct i915_request *next)
{
	 
	if (i915_request_is_active(prev))
		return true;

	return rq_prio(prev) >= rq_prio(next);
}

static struct i915_request *
__unwind_incomplete_requests(struct intel_engine_cs *engine)
{
	struct i915_request *rq, *rn, *active = NULL;
	struct list_head *pl;
	int prio = I915_PRIORITY_INVALID;

	lockdep_assert_held(&engine->sched_engine->lock);

	list_for_each_entry_safe_reverse(rq, rn,
					 &engine->sched_engine->requests,
					 sched.link) {
		if (__i915_request_is_complete(rq)) {
			list_del_init(&rq->sched.link);
			continue;
		}

		__i915_request_unsubmit(rq);

		GEM_BUG_ON(rq_prio(rq) == I915_PRIORITY_INVALID);
		if (rq_prio(rq) != prio) {
			prio = rq_prio(rq);
			pl = i915_sched_lookup_priolist(engine->sched_engine,
							prio);
		}
		GEM_BUG_ON(i915_sched_engine_is_empty(engine->sched_engine));

		list_move(&rq->sched.link, pl);
		set_bit(I915_FENCE_FLAG_PQUEUE, &rq->fence.flags);

		 
		if (intel_ring_direction(rq->ring,
					 rq->tail,
					 rq->ring->tail + 8) > 0)
			rq->context->lrc.desc |= CTX_DESC_FORCE_RESTORE;

		active = rq;
	}

	return active;
}

struct i915_request *
execlists_unwind_incomplete_requests(struct intel_engine_execlists *execlists)
{
	struct intel_engine_cs *engine =
		container_of(execlists, typeof(*engine), execlists);

	return __unwind_incomplete_requests(engine);
}

static void
execlists_context_status_change(struct i915_request *rq, unsigned long status)
{
	 
	if (!IS_ENABLED(CONFIG_DRM_I915_GVT))
		return;

	atomic_notifier_call_chain(&rq->engine->context_status_notifier,
				   status, rq);
}

static void reset_active(struct i915_request *rq,
			 struct intel_engine_cs *engine)
{
	struct intel_context * const ce = rq->context;
	u32 head;

	 
	ENGINE_TRACE(engine, "{ reset rq=%llx:%lld }\n",
		     rq->fence.context, rq->fence.seqno);

	 
	if (__i915_request_is_complete(rq))
		head = rq->tail;
	else
		head = __active_request(ce->timeline, rq, -EIO)->head;
	head = intel_ring_wrap(ce->ring, head);

	 
	lrc_init_regs(ce, engine, true);

	 
	ce->lrc.lrca = lrc_update_regs(ce, engine, head);
}

static bool bad_request(const struct i915_request *rq)
{
	return rq->fence.error && i915_request_started(rq);
}

static struct intel_engine_cs *
__execlists_schedule_in(struct i915_request *rq)
{
	struct intel_engine_cs * const engine = rq->engine;
	struct intel_context * const ce = rq->context;

	intel_context_get(ce);

	if (unlikely(intel_context_is_closed(ce) &&
		     !intel_engine_has_heartbeat(engine)))
		intel_context_set_exiting(ce);

	if (unlikely(!intel_context_is_schedulable(ce) || bad_request(rq)))
		reset_active(rq, engine);

	if (IS_ENABLED(CONFIG_DRM_I915_DEBUG_GEM))
		lrc_check_regs(ce, engine, "before");

	if (ce->tag) {
		 
		GEM_BUG_ON(ce->tag <= BITS_PER_LONG);
		ce->lrc.ccid = ce->tag;
	} else if (GRAPHICS_VER_FULL(engine->i915) >= IP_VER(12, 50)) {
		 
		unsigned int tag = ffs(READ_ONCE(engine->context_tag));

		GEM_BUG_ON(tag == 0 || tag >= BITS_PER_LONG);
		clear_bit(tag - 1, &engine->context_tag);
		ce->lrc.ccid = tag << (XEHP_SW_CTX_ID_SHIFT - 32);

		BUILD_BUG_ON(BITS_PER_LONG > GEN12_MAX_CONTEXT_HW_ID);

	} else {
		 
		unsigned int tag = __ffs(engine->context_tag);

		GEM_BUG_ON(tag >= BITS_PER_LONG);
		__clear_bit(tag, &engine->context_tag);
		ce->lrc.ccid = (1 + tag) << (GEN11_SW_CTX_ID_SHIFT - 32);

		BUILD_BUG_ON(BITS_PER_LONG > GEN12_MAX_CONTEXT_HW_ID);
	}

	ce->lrc.ccid |= engine->execlists.ccid;

	__intel_gt_pm_get(engine->gt);
	if (engine->fw_domain && !engine->fw_active++)
		intel_uncore_forcewake_get(engine->uncore, engine->fw_domain);
	execlists_context_status_change(rq, INTEL_CONTEXT_SCHEDULE_IN);
	intel_engine_context_in(engine);

	CE_TRACE(ce, "schedule-in, ccid:%x\n", ce->lrc.ccid);

	return engine;
}

static void execlists_schedule_in(struct i915_request *rq, int idx)
{
	struct intel_context * const ce = rq->context;
	struct intel_engine_cs *old;

	GEM_BUG_ON(!intel_engine_pm_is_awake(rq->engine));
	trace_i915_request_in(rq, idx);

	old = ce->inflight;
	if (!old)
		old = __execlists_schedule_in(rq);
	WRITE_ONCE(ce->inflight, ptr_inc(old));

	GEM_BUG_ON(intel_context_inflight(ce) != rq->engine);
}

static void
resubmit_virtual_request(struct i915_request *rq, struct virtual_engine *ve)
{
	struct intel_engine_cs *engine = rq->engine;

	spin_lock_irq(&engine->sched_engine->lock);

	clear_bit(I915_FENCE_FLAG_PQUEUE, &rq->fence.flags);
	WRITE_ONCE(rq->engine, &ve->base);
	ve->base.submit_request(rq);

	spin_unlock_irq(&engine->sched_engine->lock);
}

static void kick_siblings(struct i915_request *rq, struct intel_context *ce)
{
	struct virtual_engine *ve = container_of(ce, typeof(*ve), context);
	struct intel_engine_cs *engine = rq->engine;

	 
	if (!list_empty(&ce->signals))
		intel_context_remove_breadcrumbs(ce, engine->breadcrumbs);

	 
	if (i915_request_in_priority_queue(rq) &&
	    rq->execution_mask != engine->mask)
		resubmit_virtual_request(rq, ve);

	if (READ_ONCE(ve->request))
		tasklet_hi_schedule(&ve->base.sched_engine->tasklet);
}

static void __execlists_schedule_out(struct i915_request * const rq,
				     struct intel_context * const ce)
{
	struct intel_engine_cs * const engine = rq->engine;
	unsigned int ccid;

	 

	CE_TRACE(ce, "schedule-out, ccid:%x\n", ce->lrc.ccid);
	GEM_BUG_ON(ce->inflight != engine);

	if (IS_ENABLED(CONFIG_DRM_I915_DEBUG_GEM))
		lrc_check_regs(ce, engine, "after");

	 
	if (intel_timeline_is_last(ce->timeline, rq) &&
	    __i915_request_is_complete(rq))
		intel_engine_add_retire(engine, ce->timeline);

	ccid = ce->lrc.ccid;
	if (GRAPHICS_VER_FULL(engine->i915) >= IP_VER(12, 50)) {
		ccid >>= XEHP_SW_CTX_ID_SHIFT - 32;
		ccid &= XEHP_MAX_CONTEXT_HW_ID;
	} else {
		ccid >>= GEN11_SW_CTX_ID_SHIFT - 32;
		ccid &= GEN12_MAX_CONTEXT_HW_ID;
	}

	if (ccid < BITS_PER_LONG) {
		GEM_BUG_ON(ccid == 0);
		GEM_BUG_ON(test_bit(ccid - 1, &engine->context_tag));
		__set_bit(ccid - 1, &engine->context_tag);
	}
	intel_engine_context_out(engine);
	execlists_context_status_change(rq, INTEL_CONTEXT_SCHEDULE_OUT);
	if (engine->fw_domain && !--engine->fw_active)
		intel_uncore_forcewake_put(engine->uncore, engine->fw_domain);
	intel_gt_pm_put_async(engine->gt);

	 
	if (ce->engine != engine)
		kick_siblings(rq, ce);

	WRITE_ONCE(ce->inflight, NULL);
	intel_context_put(ce);
}

static inline void execlists_schedule_out(struct i915_request *rq)
{
	struct intel_context * const ce = rq->context;

	trace_i915_request_out(rq);

	GEM_BUG_ON(!ce->inflight);
	ce->inflight = ptr_dec(ce->inflight);
	if (!__intel_context_inflight_count(ce->inflight))
		__execlists_schedule_out(rq, ce);

	i915_request_put(rq);
}

static u32 map_i915_prio_to_lrc_desc_prio(int prio)
{
	if (prio > I915_PRIORITY_NORMAL)
		return GEN12_CTX_PRIORITY_HIGH;
	else if (prio < I915_PRIORITY_NORMAL)
		return GEN12_CTX_PRIORITY_LOW;
	else
		return GEN12_CTX_PRIORITY_NORMAL;
}

static u64 execlists_update_context(struct i915_request *rq)
{
	struct intel_context *ce = rq->context;
	u64 desc;
	u32 tail, prev;

	desc = ce->lrc.desc;
	if (rq->engine->flags & I915_ENGINE_HAS_EU_PRIORITY)
		desc |= map_i915_prio_to_lrc_desc_prio(rq_prio(rq));

	 
	GEM_BUG_ON(ce->lrc_reg_state[CTX_RING_TAIL] != rq->ring->tail);
	prev = rq->ring->tail;
	tail = intel_ring_set_tail(rq->ring, rq->tail);
	if (unlikely(intel_ring_direction(rq->ring, tail, prev) <= 0))
		desc |= CTX_DESC_FORCE_RESTORE;
	ce->lrc_reg_state[CTX_RING_TAIL] = tail;
	rq->tail = rq->wa_tail;

	 
	wmb();

	ce->lrc.desc &= ~CTX_DESC_FORCE_RESTORE;
	return desc;
}

static void write_desc(struct intel_engine_execlists *execlists, u64 desc, u32 port)
{
	if (execlists->ctrl_reg) {
		writel(lower_32_bits(desc), execlists->submit_reg + port * 2);
		writel(upper_32_bits(desc), execlists->submit_reg + port * 2 + 1);
	} else {
		writel(upper_32_bits(desc), execlists->submit_reg);
		writel(lower_32_bits(desc), execlists->submit_reg);
	}
}

static __maybe_unused char *
dump_port(char *buf, int buflen, const char *prefix, struct i915_request *rq)
{
	if (!rq)
		return "";

	snprintf(buf, buflen, "%sccid:%x %llx:%lld%s prio %d",
		 prefix,
		 rq->context->lrc.ccid,
		 rq->fence.context, rq->fence.seqno,
		 __i915_request_is_complete(rq) ? "!" :
		 __i915_request_has_started(rq) ? "*" :
		 "",
		 rq_prio(rq));

	return buf;
}

static __maybe_unused noinline void
trace_ports(const struct intel_engine_execlists *execlists,
	    const char *msg,
	    struct i915_request * const *ports)
{
	const struct intel_engine_cs *engine =
		container_of(execlists, typeof(*engine), execlists);
	char __maybe_unused p0[40], p1[40];

	if (!ports[0])
		return;

	ENGINE_TRACE(engine, "%s { %s%s }\n", msg,
		     dump_port(p0, sizeof(p0), "", ports[0]),
		     dump_port(p1, sizeof(p1), ", ", ports[1]));
}

static bool
reset_in_progress(const struct intel_engine_cs *engine)
{
	return unlikely(!__tasklet_is_enabled(&engine->sched_engine->tasklet));
}

static __maybe_unused noinline bool
assert_pending_valid(const struct intel_engine_execlists *execlists,
		     const char *msg)
{
	struct intel_engine_cs *engine =
		container_of(execlists, typeof(*engine), execlists);
	struct i915_request * const *port, *rq, *prev = NULL;
	struct intel_context *ce = NULL;
	u32 ccid = -1;

	trace_ports(execlists, msg, execlists->pending);

	 
	if (reset_in_progress(engine))
		return true;

	if (!execlists->pending[0]) {
		GEM_TRACE_ERR("%s: Nothing pending for promotion!\n",
			      engine->name);
		return false;
	}

	if (execlists->pending[execlists_num_ports(execlists)]) {
		GEM_TRACE_ERR("%s: Excess pending[%d] for promotion!\n",
			      engine->name, execlists_num_ports(execlists));
		return false;
	}

	for (port = execlists->pending; (rq = *port); port++) {
		unsigned long flags;
		bool ok = true;

		GEM_BUG_ON(!kref_read(&rq->fence.refcount));
		GEM_BUG_ON(!i915_request_is_active(rq));

		if (ce == rq->context) {
			GEM_TRACE_ERR("%s: Dup context:%llx in pending[%zd]\n",
				      engine->name,
				      ce->timeline->fence_context,
				      port - execlists->pending);
			return false;
		}
		ce = rq->context;

		if (ccid == ce->lrc.ccid) {
			GEM_TRACE_ERR("%s: Dup ccid:%x context:%llx in pending[%zd]\n",
				      engine->name,
				      ccid, ce->timeline->fence_context,
				      port - execlists->pending);
			return false;
		}
		ccid = ce->lrc.ccid;

		 
		if (prev && i915_request_has_sentinel(prev) &&
		    !READ_ONCE(prev->fence.error)) {
			GEM_TRACE_ERR("%s: context:%llx after sentinel in pending[%zd]\n",
				      engine->name,
				      ce->timeline->fence_context,
				      port - execlists->pending);
			return false;
		}
		prev = rq;

		 
		if (rq->execution_mask != engine->mask &&
		    port != execlists->pending) {
			GEM_TRACE_ERR("%s: virtual engine:%llx not in prime position[%zd]\n",
				      engine->name,
				      ce->timeline->fence_context,
				      port - execlists->pending);
			return false;
		}

		 
		if (!spin_trylock_irqsave(&rq->lock, flags))
			continue;

		if (__i915_request_is_complete(rq))
			goto unlock;

		if (i915_active_is_idle(&ce->active) &&
		    !intel_context_is_barrier(ce)) {
			GEM_TRACE_ERR("%s: Inactive context:%llx in pending[%zd]\n",
				      engine->name,
				      ce->timeline->fence_context,
				      port - execlists->pending);
			ok = false;
			goto unlock;
		}

		if (!i915_vma_is_pinned(ce->state)) {
			GEM_TRACE_ERR("%s: Unpinned context:%llx in pending[%zd]\n",
				      engine->name,
				      ce->timeline->fence_context,
				      port - execlists->pending);
			ok = false;
			goto unlock;
		}

		if (!i915_vma_is_pinned(ce->ring->vma)) {
			GEM_TRACE_ERR("%s: Unpinned ring:%llx in pending[%zd]\n",
				      engine->name,
				      ce->timeline->fence_context,
				      port - execlists->pending);
			ok = false;
			goto unlock;
		}

unlock:
		spin_unlock_irqrestore(&rq->lock, flags);
		if (!ok)
			return false;
	}

	return ce;
}

static void execlists_submit_ports(struct intel_engine_cs *engine)
{
	struct intel_engine_execlists *execlists = &engine->execlists;
	unsigned int n;

	GEM_BUG_ON(!assert_pending_valid(execlists, "submit"));

	 
	GEM_BUG_ON(!intel_engine_pm_is_awake(engine));

	 
	for (n = execlists_num_ports(execlists); n--; ) {
		struct i915_request *rq = execlists->pending[n];

		write_desc(execlists,
			   rq ? execlists_update_context(rq) : 0,
			   n);
	}

	 
	if (execlists->ctrl_reg)
		writel(EL_CTRL_LOAD, execlists->ctrl_reg);
}

static bool ctx_single_port_submission(const struct intel_context *ce)
{
	return (IS_ENABLED(CONFIG_DRM_I915_GVT) &&
		intel_context_force_single_submission(ce));
}

static bool can_merge_ctx(const struct intel_context *prev,
			  const struct intel_context *next)
{
	if (prev != next)
		return false;

	if (ctx_single_port_submission(prev))
		return false;

	return true;
}

static unsigned long i915_request_flags(const struct i915_request *rq)
{
	return READ_ONCE(rq->fence.flags);
}

static bool can_merge_rq(const struct i915_request *prev,
			 const struct i915_request *next)
{
	GEM_BUG_ON(prev == next);
	GEM_BUG_ON(!assert_priority_queue(prev, next));

	 
	if (__i915_request_is_complete(next))
		return true;

	if (unlikely((i915_request_flags(prev) | i915_request_flags(next)) &
		     (BIT(I915_FENCE_FLAG_NOPREEMPT) |
		      BIT(I915_FENCE_FLAG_SENTINEL))))
		return false;

	if (!can_merge_ctx(prev->context, next->context))
		return false;

	GEM_BUG_ON(i915_seqno_passed(prev->fence.seqno, next->fence.seqno));
	return true;
}

static bool virtual_matches(const struct virtual_engine *ve,
			    const struct i915_request *rq,
			    const struct intel_engine_cs *engine)
{
	const struct intel_engine_cs *inflight;

	if (!rq)
		return false;

	if (!(rq->execution_mask & engine->mask))  
		return false;

	 
	inflight = intel_context_inflight(&ve->context);
	if (inflight && inflight != engine)
		return false;

	return true;
}

static struct virtual_engine *
first_virtual_engine(struct intel_engine_cs *engine)
{
	struct intel_engine_execlists *el = &engine->execlists;
	struct rb_node *rb = rb_first_cached(&el->virtual);

	while (rb) {
		struct virtual_engine *ve =
			rb_entry(rb, typeof(*ve), nodes[engine->id].rb);
		struct i915_request *rq = READ_ONCE(ve->request);

		 
		if (!rq || !virtual_matches(ve, rq, engine)) {
			rb_erase_cached(rb, &el->virtual);
			RB_CLEAR_NODE(rb);
			rb = rb_first_cached(&el->virtual);
			continue;
		}

		return ve;
	}

	return NULL;
}

static void virtual_xfer_context(struct virtual_engine *ve,
				 struct intel_engine_cs *engine)
{
	unsigned int n;

	if (likely(engine == ve->siblings[0]))
		return;

	GEM_BUG_ON(READ_ONCE(ve->context.inflight));
	if (!intel_engine_has_relative_mmio(engine))
		lrc_update_offsets(&ve->context, engine);

	 
	for (n = 1; n < ve->num_siblings; n++) {
		if (ve->siblings[n] == engine) {
			swap(ve->siblings[n], ve->siblings[0]);
			break;
		}
	}
}

static void defer_request(struct i915_request *rq, struct list_head * const pl)
{
	LIST_HEAD(list);

	 
	do {
		struct i915_dependency *p;

		GEM_BUG_ON(i915_request_is_active(rq));
		list_move_tail(&rq->sched.link, pl);

		for_each_waiter(p, rq) {
			struct i915_request *w =
				container_of(p->waiter, typeof(*w), sched);

			if (p->flags & I915_DEPENDENCY_WEAK)
				continue;

			 
			if (w->engine != rq->engine)
				continue;

			 
			GEM_BUG_ON(i915_request_has_initial_breadcrumb(w) &&
				   __i915_request_has_started(w) &&
				   !__i915_request_is_complete(rq));

			if (!i915_request_is_ready(w))
				continue;

			if (rq_prio(w) < rq_prio(rq))
				continue;

			GEM_BUG_ON(rq_prio(w) > rq_prio(rq));
			GEM_BUG_ON(i915_request_is_active(w));
			list_move_tail(&w->sched.link, &list);
		}

		rq = list_first_entry_or_null(&list, typeof(*rq), sched.link);
	} while (rq);
}

static void defer_active(struct intel_engine_cs *engine)
{
	struct i915_request *rq;

	rq = __unwind_incomplete_requests(engine);
	if (!rq)
		return;

	defer_request(rq, i915_sched_lookup_priolist(engine->sched_engine,
						     rq_prio(rq)));
}

static bool
timeslice_yield(const struct intel_engine_execlists *el,
		const struct i915_request *rq)
{
	 
	return rq->context->lrc.ccid == READ_ONCE(el->yield);
}

static bool needs_timeslice(const struct intel_engine_cs *engine,
			    const struct i915_request *rq)
{
	if (!intel_engine_has_timeslices(engine))
		return false;

	 
	if (!rq || __i915_request_is_complete(rq))
		return false;

	 
	if (READ_ONCE(engine->execlists.pending[0]))
		return false;

	 
	if (!list_is_last_rcu(&rq->sched.link,
			      &engine->sched_engine->requests)) {
		ENGINE_TRACE(engine, "timeslice required for second inflight context\n");
		return true;
	}

	 
	if (!i915_sched_engine_is_empty(engine->sched_engine)) {
		ENGINE_TRACE(engine, "timeslice required for queue\n");
		return true;
	}

	if (!RB_EMPTY_ROOT(&engine->execlists.virtual.rb_root)) {
		ENGINE_TRACE(engine, "timeslice required for virtual\n");
		return true;
	}

	return false;
}

static bool
timeslice_expired(struct intel_engine_cs *engine, const struct i915_request *rq)
{
	const struct intel_engine_execlists *el = &engine->execlists;

	if (i915_request_has_nopreempt(rq) && __i915_request_has_started(rq))
		return false;

	if (!needs_timeslice(engine, rq))
		return false;

	return timer_expired(&el->timer) || timeslice_yield(el, rq);
}

static unsigned long timeslice(const struct intel_engine_cs *engine)
{
	return READ_ONCE(engine->props.timeslice_duration_ms);
}

static void start_timeslice(struct intel_engine_cs *engine)
{
	struct intel_engine_execlists *el = &engine->execlists;
	unsigned long duration;

	 
	duration = 0;
	if (needs_timeslice(engine, *el->active)) {
		 
		if (timer_active(&el->timer)) {
			 
			if (!timer_pending(&el->timer))
				tasklet_hi_schedule(&engine->sched_engine->tasklet);
			return;
		}

		duration = timeslice(engine);
	}

	set_timer_ms(&el->timer, duration);
}

static void record_preemption(struct intel_engine_execlists *execlists)
{
	(void)I915_SELFTEST_ONLY(execlists->preempt_hang.count++);
}

static unsigned long active_preempt_timeout(struct intel_engine_cs *engine,
					    const struct i915_request *rq)
{
	if (!rq)
		return 0;

	 
	engine->execlists.preempt_target = rq;

	 
	if (unlikely(intel_context_is_banned(rq->context) || bad_request(rq)))
		return INTEL_CONTEXT_BANNED_PREEMPT_TIMEOUT_MS;

	return READ_ONCE(engine->props.preempt_timeout_ms);
}

static void set_preempt_timeout(struct intel_engine_cs *engine,
				const struct i915_request *rq)
{
	if (!intel_engine_has_preempt_reset(engine))
		return;

	set_timer_ms(&engine->execlists.preempt,
		     active_preempt_timeout(engine, rq));
}

static bool completed(const struct i915_request *rq)
{
	if (i915_request_has_sentinel(rq))
		return false;

	return __i915_request_is_complete(rq);
}

static void execlists_dequeue(struct intel_engine_cs *engine)
{
	struct intel_engine_execlists * const execlists = &engine->execlists;
	struct i915_sched_engine * const sched_engine = engine->sched_engine;
	struct i915_request **port = execlists->pending;
	struct i915_request ** const last_port = port + execlists->port_mask;
	struct i915_request *last, * const *active;
	struct virtual_engine *ve;
	struct rb_node *rb;
	bool submit = false;

	 

	spin_lock(&sched_engine->lock);

	 
	active = execlists->active;
	while ((last = *active) && completed(last))
		active++;

	if (last) {
		if (need_preempt(engine, last)) {
			ENGINE_TRACE(engine,
				     "preempting last=%llx:%lld, prio=%d, hint=%d\n",
				     last->fence.context,
				     last->fence.seqno,
				     last->sched.attr.priority,
				     sched_engine->queue_priority_hint);
			record_preemption(execlists);

			 
			ring_set_paused(engine, 1);

			 
			__unwind_incomplete_requests(engine);

			last = NULL;
		} else if (timeslice_expired(engine, last)) {
			ENGINE_TRACE(engine,
				     "expired:%s last=%llx:%lld, prio=%d, hint=%d, yield?=%s\n",
				     str_yes_no(timer_expired(&execlists->timer)),
				     last->fence.context, last->fence.seqno,
				     rq_prio(last),
				     sched_engine->queue_priority_hint,
				     str_yes_no(timeslice_yield(execlists, last)));

			 
			cancel_timer(&execlists->timer);
			ring_set_paused(engine, 1);
			defer_active(engine);

			 
			last = NULL;
		} else {
			 
			if (active[1]) {
				 
				spin_unlock(&sched_engine->lock);
				return;
			}
		}
	}

	 
	while ((ve = first_virtual_engine(engine))) {
		struct i915_request *rq;

		spin_lock(&ve->base.sched_engine->lock);

		rq = ve->request;
		if (unlikely(!virtual_matches(ve, rq, engine)))
			goto unlock;  

		GEM_BUG_ON(rq->engine != &ve->base);
		GEM_BUG_ON(rq->context != &ve->context);

		if (unlikely(rq_prio(rq) < queue_prio(sched_engine))) {
			spin_unlock(&ve->base.sched_engine->lock);
			break;
		}

		if (last && !can_merge_rq(last, rq)) {
			spin_unlock(&ve->base.sched_engine->lock);
			spin_unlock(&engine->sched_engine->lock);
			return;  
		}

		ENGINE_TRACE(engine,
			     "virtual rq=%llx:%lld%s, new engine? %s\n",
			     rq->fence.context,
			     rq->fence.seqno,
			     __i915_request_is_complete(rq) ? "!" :
			     __i915_request_has_started(rq) ? "*" :
			     "",
			     str_yes_no(engine != ve->siblings[0]));

		WRITE_ONCE(ve->request, NULL);
		WRITE_ONCE(ve->base.sched_engine->queue_priority_hint, INT_MIN);

		rb = &ve->nodes[engine->id].rb;
		rb_erase_cached(rb, &execlists->virtual);
		RB_CLEAR_NODE(rb);

		GEM_BUG_ON(!(rq->execution_mask & engine->mask));
		WRITE_ONCE(rq->engine, engine);

		if (__i915_request_submit(rq)) {
			 
			virtual_xfer_context(ve, engine);
			GEM_BUG_ON(ve->siblings[0] != engine);

			submit = true;
			last = rq;
		}

		i915_request_put(rq);
unlock:
		spin_unlock(&ve->base.sched_engine->lock);

		 
		if (submit)
			break;
	}

	while ((rb = rb_first_cached(&sched_engine->queue))) {
		struct i915_priolist *p = to_priolist(rb);
		struct i915_request *rq, *rn;

		priolist_for_each_request_consume(rq, rn, p) {
			bool merge = true;

			 
			if (last && !can_merge_rq(last, rq)) {
				 
				if (port == last_port)
					goto done;

				 
				if (last->context == rq->context)
					goto done;

				if (i915_request_has_sentinel(last))
					goto done;

				 
				if (rq->execution_mask != engine->mask)
					goto done;

				 
				if (ctx_single_port_submission(last->context) ||
				    ctx_single_port_submission(rq->context))
					goto done;

				merge = false;
			}

			if (__i915_request_submit(rq)) {
				if (!merge) {
					*port++ = i915_request_get(last);
					last = NULL;
				}

				GEM_BUG_ON(last &&
					   !can_merge_ctx(last->context,
							  rq->context));
				GEM_BUG_ON(last &&
					   i915_seqno_passed(last->fence.seqno,
							     rq->fence.seqno));

				submit = true;
				last = rq;
			}
		}

		rb_erase_cached(&p->node, &sched_engine->queue);
		i915_priolist_free(p);
	}
done:
	*port++ = i915_request_get(last);

	 
	sched_engine->queue_priority_hint = queue_prio(sched_engine);
	i915_sched_engine_reset_on_empty(sched_engine);
	spin_unlock(&sched_engine->lock);

	 
	if (submit &&
	    memcmp(active,
		   execlists->pending,
		   (port - execlists->pending) * sizeof(*port))) {
		*port = NULL;
		while (port-- != execlists->pending)
			execlists_schedule_in(*port, port - execlists->pending);

		WRITE_ONCE(execlists->yield, -1);
		set_preempt_timeout(engine, *active);
		execlists_submit_ports(engine);
	} else {
		ring_set_paused(engine, 0);
		while (port-- != execlists->pending)
			i915_request_put(*port);
		*execlists->pending = NULL;
	}
}

static void execlists_dequeue_irq(struct intel_engine_cs *engine)
{
	local_irq_disable();  
	execlists_dequeue(engine);
	local_irq_enable();  
}

static void clear_ports(struct i915_request **ports, int count)
{
	memset_p((void **)ports, NULL, count);
}

static void
copy_ports(struct i915_request **dst, struct i915_request **src, int count)
{
	 
	while (count--)
		WRITE_ONCE(*dst++, *src++);  
}

static struct i915_request **
cancel_port_requests(struct intel_engine_execlists * const execlists,
		     struct i915_request **inactive)
{
	struct i915_request * const *port;

	for (port = execlists->pending; *port; port++)
		*inactive++ = *port;
	clear_ports(execlists->pending, ARRAY_SIZE(execlists->pending));

	 
	for (port = xchg(&execlists->active, execlists->pending); *port; port++)
		*inactive++ = *port;
	clear_ports(execlists->inflight, ARRAY_SIZE(execlists->inflight));

	smp_wmb();  
	WRITE_ONCE(execlists->active, execlists->inflight);

	 
	GEM_BUG_ON(execlists->pending[0]);
	cancel_timer(&execlists->timer);
	cancel_timer(&execlists->preempt);

	return inactive;
}

 
static inline bool
__gen12_csb_parse(bool ctx_to_valid, bool ctx_away_valid, bool new_queue,
		  u8 switch_detail)
{
	 
	if (!ctx_away_valid || new_queue) {
		GEM_BUG_ON(!ctx_to_valid);
		return true;
	}

	 
	GEM_BUG_ON(switch_detail);
	return false;
}

static bool xehp_csb_parse(const u64 csb)
{
	return __gen12_csb_parse(XEHP_CSB_CTX_VALID(lower_32_bits(csb)),  
				 XEHP_CSB_CTX_VALID(upper_32_bits(csb)),  
				 upper_32_bits(csb) & XEHP_CTX_STATUS_SWITCHED_TO_NEW_QUEUE,
				 GEN12_CTX_SWITCH_DETAIL(lower_32_bits(csb)));
}

static bool gen12_csb_parse(const u64 csb)
{
	return __gen12_csb_parse(GEN12_CSB_CTX_VALID(lower_32_bits(csb)),  
				 GEN12_CSB_CTX_VALID(upper_32_bits(csb)),  
				 lower_32_bits(csb) & GEN12_CTX_STATUS_SWITCHED_TO_NEW_QUEUE,
				 GEN12_CTX_SWITCH_DETAIL(upper_32_bits(csb)));
}

static bool gen8_csb_parse(const u64 csb)
{
	return csb & (GEN8_CTX_STATUS_IDLE_ACTIVE | GEN8_CTX_STATUS_PREEMPTED);
}

static noinline u64
wa_csb_read(const struct intel_engine_cs *engine, u64 * const csb)
{
	u64 entry;

	 
	preempt_disable();
	if (wait_for_atomic_us((entry = READ_ONCE(*csb)) != -1, 10)) {
		int idx = csb - engine->execlists.csb_status;
		int status;

		status = GEN8_EXECLISTS_STATUS_BUF;
		if (idx >= 6) {
			status = GEN11_EXECLISTS_STATUS_BUF2;
			idx -= 6;
		}
		status += sizeof(u64) * idx;

		entry = intel_uncore_read64(engine->uncore,
					    _MMIO(engine->mmio_base + status));
	}
	preempt_enable();

	return entry;
}

static u64 csb_read(const struct intel_engine_cs *engine, u64 * const csb)
{
	u64 entry = READ_ONCE(*csb);

	 
	if (unlikely(entry == -1))
		entry = wa_csb_read(engine, csb);

	 
	WRITE_ONCE(*csb, -1);

	 
	return entry;
}

static void new_timeslice(struct intel_engine_execlists *el)
{
	 
	cancel_timer(&el->timer);
}

static struct i915_request **
process_csb(struct intel_engine_cs *engine, struct i915_request **inactive)
{
	struct intel_engine_execlists * const execlists = &engine->execlists;
	u64 * const buf = execlists->csb_status;
	const u8 num_entries = execlists->csb_size;
	struct i915_request **prev;
	u8 head, tail;

	 
	GEM_BUG_ON(!tasklet_is_locked(&engine->sched_engine->tasklet) &&
		   !reset_in_progress(engine));

	 
	head = execlists->csb_head;
	tail = READ_ONCE(*execlists->csb_write);
	if (unlikely(head == tail))
		return inactive;

	 
	execlists->csb_head = tail;
	ENGINE_TRACE(engine, "cs-irq head=%d, tail=%d\n", head, tail);

	 
	rmb();

	 
	prev = inactive;
	*prev = NULL;

	do {
		bool promote;
		u64 csb;

		if (++head == num_entries)
			head = 0;

		 

		csb = csb_read(engine, buf + head);
		ENGINE_TRACE(engine, "csb[%d]: status=0x%08x:0x%08x\n",
			     head, upper_32_bits(csb), lower_32_bits(csb));

		if (GRAPHICS_VER_FULL(engine->i915) >= IP_VER(12, 50))
			promote = xehp_csb_parse(csb);
		else if (GRAPHICS_VER(engine->i915) >= 12)
			promote = gen12_csb_parse(csb);
		else
			promote = gen8_csb_parse(csb);
		if (promote) {
			struct i915_request * const *old = execlists->active;

			if (GEM_WARN_ON(!*execlists->pending)) {
				execlists->error_interrupt |= ERROR_CSB;
				break;
			}

			ring_set_paused(engine, 0);

			 
			WRITE_ONCE(execlists->active, execlists->pending);
			smp_wmb();  

			 
			trace_ports(execlists, "preempted", old);
			while (*old)
				*inactive++ = *old++;

			 
			GEM_BUG_ON(!assert_pending_valid(execlists, "promote"));
			copy_ports(execlists->inflight,
				   execlists->pending,
				   execlists_num_ports(execlists));
			smp_wmb();  
			WRITE_ONCE(execlists->active, execlists->inflight);

			 
			ENGINE_POSTING_READ(engine, RING_CONTEXT_STATUS_PTR);

			WRITE_ONCE(execlists->pending[0], NULL);
		} else {
			if (GEM_WARN_ON(!*execlists->active)) {
				execlists->error_interrupt |= ERROR_CSB;
				break;
			}

			 
			trace_ports(execlists, "completed", execlists->active);

			 
			if (GEM_SHOW_DEBUG() &&
			    !__i915_request_is_complete(*execlists->active)) {
				struct i915_request *rq = *execlists->active;
				const u32 *regs __maybe_unused =
					rq->context->lrc_reg_state;

				ENGINE_TRACE(engine,
					     "context completed before request!\n");
				ENGINE_TRACE(engine,
					     "ring:{start:0x%08x, head:%04x, tail:%04x, ctl:%08x, mode:%08x}\n",
					     ENGINE_READ(engine, RING_START),
					     ENGINE_READ(engine, RING_HEAD) & HEAD_ADDR,
					     ENGINE_READ(engine, RING_TAIL) & TAIL_ADDR,
					     ENGINE_READ(engine, RING_CTL),
					     ENGINE_READ(engine, RING_MI_MODE));
				ENGINE_TRACE(engine,
					     "rq:{start:%08x, head:%04x, tail:%04x, seqno:%llx:%d, hwsp:%d}, ",
					     i915_ggtt_offset(rq->ring->vma),
					     rq->head, rq->tail,
					     rq->fence.context,
					     lower_32_bits(rq->fence.seqno),
					     hwsp_seqno(rq));
				ENGINE_TRACE(engine,
					     "ctx:{start:%08x, head:%04x, tail:%04x}, ",
					     regs[CTX_RING_START],
					     regs[CTX_RING_HEAD],
					     regs[CTX_RING_TAIL]);
			}

			*inactive++ = *execlists->active++;

			GEM_BUG_ON(execlists->active - execlists->inflight >
				   execlists_num_ports(execlists));
		}
	} while (head != tail);

	 
	drm_clflush_virt_range(&buf[0], num_entries * sizeof(buf[0]));

	 
	if (*prev != *execlists->active) {  
		struct intel_context *prev_ce = NULL, *active_ce = NULL;

		 
		if (*prev)
			prev_ce = (*prev)->context;
		if (*execlists->active)
			active_ce = (*execlists->active)->context;
		if (prev_ce != active_ce) {
			if (prev_ce)
				lrc_runtime_stop(prev_ce);
			if (active_ce)
				lrc_runtime_start(active_ce);
		}
		new_timeslice(execlists);
	}

	return inactive;
}

static void post_process_csb(struct i915_request **port,
			     struct i915_request **last)
{
	while (port != last)
		execlists_schedule_out(*port++);
}

static void __execlists_hold(struct i915_request *rq)
{
	LIST_HEAD(list);

	do {
		struct i915_dependency *p;

		if (i915_request_is_active(rq))
			__i915_request_unsubmit(rq);

		clear_bit(I915_FENCE_FLAG_PQUEUE, &rq->fence.flags);
		list_move_tail(&rq->sched.link,
			       &rq->engine->sched_engine->hold);
		i915_request_set_hold(rq);
		RQ_TRACE(rq, "on hold\n");

		for_each_waiter(p, rq) {
			struct i915_request *w =
				container_of(p->waiter, typeof(*w), sched);

			if (p->flags & I915_DEPENDENCY_WEAK)
				continue;

			 
			if (w->engine != rq->engine)
				continue;

			if (!i915_request_is_ready(w))
				continue;

			if (__i915_request_is_complete(w))
				continue;

			if (i915_request_on_hold(w))
				continue;

			list_move_tail(&w->sched.link, &list);
		}

		rq = list_first_entry_or_null(&list, typeof(*rq), sched.link);
	} while (rq);
}

static bool execlists_hold(struct intel_engine_cs *engine,
			   struct i915_request *rq)
{
	if (i915_request_on_hold(rq))
		return false;

	spin_lock_irq(&engine->sched_engine->lock);

	if (__i915_request_is_complete(rq)) {  
		rq = NULL;
		goto unlock;
	}

	 
	GEM_BUG_ON(i915_request_on_hold(rq));
	GEM_BUG_ON(rq->engine != engine);
	__execlists_hold(rq);
	GEM_BUG_ON(list_empty(&engine->sched_engine->hold));

unlock:
	spin_unlock_irq(&engine->sched_engine->lock);
	return rq;
}

static bool hold_request(const struct i915_request *rq)
{
	struct i915_dependency *p;
	bool result = false;

	 
	rcu_read_lock();
	for_each_signaler(p, rq) {
		const struct i915_request *s =
			container_of(p->signaler, typeof(*s), sched);

		if (s->engine != rq->engine)
			continue;

		result = i915_request_on_hold(s);
		if (result)
			break;
	}
	rcu_read_unlock();

	return result;
}

static void __execlists_unhold(struct i915_request *rq)
{
	LIST_HEAD(list);

	do {
		struct i915_dependency *p;

		RQ_TRACE(rq, "hold release\n");

		GEM_BUG_ON(!i915_request_on_hold(rq));
		GEM_BUG_ON(!i915_sw_fence_signaled(&rq->submit));

		i915_request_clear_hold(rq);
		list_move_tail(&rq->sched.link,
			       i915_sched_lookup_priolist(rq->engine->sched_engine,
							  rq_prio(rq)));
		set_bit(I915_FENCE_FLAG_PQUEUE, &rq->fence.flags);

		 
		for_each_waiter(p, rq) {
			struct i915_request *w =
				container_of(p->waiter, typeof(*w), sched);

			if (p->flags & I915_DEPENDENCY_WEAK)
				continue;

			if (w->engine != rq->engine)
				continue;

			if (!i915_request_on_hold(w))
				continue;

			 
			if (hold_request(w))
				continue;

			list_move_tail(&w->sched.link, &list);
		}

		rq = list_first_entry_or_null(&list, typeof(*rq), sched.link);
	} while (rq);
}

static void execlists_unhold(struct intel_engine_cs *engine,
			     struct i915_request *rq)
{
	spin_lock_irq(&engine->sched_engine->lock);

	 
	__execlists_unhold(rq);

	if (rq_prio(rq) > engine->sched_engine->queue_priority_hint) {
		engine->sched_engine->queue_priority_hint = rq_prio(rq);
		tasklet_hi_schedule(&engine->sched_engine->tasklet);
	}

	spin_unlock_irq(&engine->sched_engine->lock);
}

struct execlists_capture {
	struct work_struct work;
	struct i915_request *rq;
	struct i915_gpu_coredump *error;
};

static void execlists_capture_work(struct work_struct *work)
{
	struct execlists_capture *cap = container_of(work, typeof(*cap), work);
	const gfp_t gfp = __GFP_KSWAPD_RECLAIM | __GFP_RETRY_MAYFAIL |
		__GFP_NOWARN;
	struct intel_engine_cs *engine = cap->rq->engine;
	struct intel_gt_coredump *gt = cap->error->gt;
	struct intel_engine_capture_vma *vma;

	 
	vma = intel_engine_coredump_add_request(gt->engine, cap->rq, gfp);
	if (vma) {
		struct i915_vma_compress *compress =
			i915_vma_capture_prepare(gt);

		intel_engine_coredump_add_vma(gt->engine, vma, compress);
		i915_vma_capture_finish(gt, compress);
	}

	gt->simulated = gt->engine->simulated;
	cap->error->simulated = gt->simulated;

	 
	i915_error_state_store(cap->error);
	i915_gpu_coredump_put(cap->error);

	 
	execlists_unhold(engine, cap->rq);
	i915_request_put(cap->rq);

	kfree(cap);
}

static struct execlists_capture *capture_regs(struct intel_engine_cs *engine)
{
	const gfp_t gfp = GFP_ATOMIC | __GFP_NOWARN;
	struct execlists_capture *cap;

	cap = kmalloc(sizeof(*cap), gfp);
	if (!cap)
		return NULL;

	cap->error = i915_gpu_coredump_alloc(engine->i915, gfp);
	if (!cap->error)
		goto err_cap;

	cap->error->gt = intel_gt_coredump_alloc(engine->gt, gfp, CORE_DUMP_FLAG_NONE);
	if (!cap->error->gt)
		goto err_gpu;

	cap->error->gt->engine = intel_engine_coredump_alloc(engine, gfp, CORE_DUMP_FLAG_NONE);
	if (!cap->error->gt->engine)
		goto err_gt;

	cap->error->gt->engine->hung = true;

	return cap;

err_gt:
	kfree(cap->error->gt);
err_gpu:
	kfree(cap->error);
err_cap:
	kfree(cap);
	return NULL;
}

static struct i915_request *
active_context(struct intel_engine_cs *engine, u32 ccid)
{
	const struct intel_engine_execlists * const el = &engine->execlists;
	struct i915_request * const *port, *rq;

	 

	for (port = el->active; (rq = *port); port++) {
		if (rq->context->lrc.ccid == ccid) {
			ENGINE_TRACE(engine,
				     "ccid:%x found at active:%zd\n",
				     ccid, port - el->active);
			return rq;
		}
	}

	for (port = el->pending; (rq = *port); port++) {
		if (rq->context->lrc.ccid == ccid) {
			ENGINE_TRACE(engine,
				     "ccid:%x found at pending:%zd\n",
				     ccid, port - el->pending);
			return rq;
		}
	}

	ENGINE_TRACE(engine, "ccid:%x not found\n", ccid);
	return NULL;
}

static u32 active_ccid(struct intel_engine_cs *engine)
{
	return ENGINE_READ_FW(engine, RING_EXECLIST_STATUS_HI);
}

static void execlists_capture(struct intel_engine_cs *engine)
{
	struct drm_i915_private *i915 = engine->i915;
	struct execlists_capture *cap;

	if (!IS_ENABLED(CONFIG_DRM_I915_CAPTURE_ERROR))
		return;

	 
	cap = capture_regs(engine);
	if (!cap)
		return;

	spin_lock_irq(&engine->sched_engine->lock);
	cap->rq = active_context(engine, active_ccid(engine));
	if (cap->rq) {
		cap->rq = active_request(cap->rq->context->timeline, cap->rq);
		cap->rq = i915_request_get_rcu(cap->rq);
	}
	spin_unlock_irq(&engine->sched_engine->lock);
	if (!cap->rq)
		goto err_free;

	 
	if (!execlists_hold(engine, cap->rq))
		goto err_rq;

	INIT_WORK(&cap->work, execlists_capture_work);
	queue_work(i915->unordered_wq, &cap->work);
	return;

err_rq:
	i915_request_put(cap->rq);
err_free:
	i915_gpu_coredump_put(cap->error);
	kfree(cap);
}

static void execlists_reset(struct intel_engine_cs *engine, const char *msg)
{
	const unsigned int bit = I915_RESET_ENGINE + engine->id;
	unsigned long *lock = &engine->gt->reset.flags;

	if (!intel_has_reset_engine(engine->gt))
		return;

	if (test_and_set_bit(bit, lock))
		return;

	ENGINE_TRACE(engine, "reset for %s\n", msg);

	 
	tasklet_disable_nosync(&engine->sched_engine->tasklet);

	ring_set_paused(engine, 1);  
	execlists_capture(engine);
	intel_engine_reset(engine, msg);

	tasklet_enable(&engine->sched_engine->tasklet);
	clear_and_wake_up_bit(bit, lock);
}

static bool preempt_timeout(const struct intel_engine_cs *const engine)
{
	const struct timer_list *t = &engine->execlists.preempt;

	if (!CONFIG_DRM_I915_PREEMPT_TIMEOUT)
		return false;

	if (!timer_expired(t))
		return false;

	return engine->execlists.pending[0];
}

 
static void execlists_submission_tasklet(struct tasklet_struct *t)
{
	struct i915_sched_engine *sched_engine =
		from_tasklet(sched_engine, t, tasklet);
	struct intel_engine_cs * const engine = sched_engine->private_data;
	struct i915_request *post[2 * EXECLIST_MAX_PORTS];
	struct i915_request **inactive;

	rcu_read_lock();
	inactive = process_csb(engine, post);
	GEM_BUG_ON(inactive - post > ARRAY_SIZE(post));

	if (unlikely(preempt_timeout(engine))) {
		const struct i915_request *rq = *engine->execlists.active;

		 
		cancel_timer(&engine->execlists.preempt);
		if (rq == engine->execlists.preempt_target)
			engine->execlists.error_interrupt |= ERROR_PREEMPT;
		else
			set_timer_ms(&engine->execlists.preempt,
				     active_preempt_timeout(engine, rq));
	}

	if (unlikely(READ_ONCE(engine->execlists.error_interrupt))) {
		const char *msg;

		 
		if (engine->execlists.error_interrupt & GENMASK(15, 0))
			msg = "CS error";  
		else if (engine->execlists.error_interrupt & ERROR_CSB)
			msg = "invalid CSB event";
		else if (engine->execlists.error_interrupt & ERROR_PREEMPT)
			msg = "preemption time out";
		else
			msg = "internal error";

		engine->execlists.error_interrupt = 0;
		execlists_reset(engine, msg);
	}

	if (!engine->execlists.pending[0]) {
		execlists_dequeue_irq(engine);
		start_timeslice(engine);
	}

	post_process_csb(post, inactive);
	rcu_read_unlock();
}

static void execlists_irq_handler(struct intel_engine_cs *engine, u16 iir)
{
	bool tasklet = false;

	if (unlikely(iir & GT_CS_MASTER_ERROR_INTERRUPT)) {
		u32 eir;

		 
		eir = ENGINE_READ(engine, RING_EIR) & GENMASK(15, 0);
		ENGINE_TRACE(engine, "CS error: %x\n", eir);

		 
		if (likely(eir)) {
			ENGINE_WRITE(engine, RING_EMR, ~0u);
			ENGINE_WRITE(engine, RING_EIR, eir);
			WRITE_ONCE(engine->execlists.error_interrupt, eir);
			tasklet = true;
		}
	}

	if (iir & GT_WAIT_SEMAPHORE_INTERRUPT) {
		WRITE_ONCE(engine->execlists.yield,
			   ENGINE_READ_FW(engine, RING_EXECLIST_STATUS_HI));
		ENGINE_TRACE(engine, "semaphore yield: %08x\n",
			     engine->execlists.yield);
		if (del_timer(&engine->execlists.timer))
			tasklet = true;
	}

	if (iir & GT_CONTEXT_SWITCH_INTERRUPT)
		tasklet = true;

	if (iir & GT_RENDER_USER_INTERRUPT)
		intel_engine_signal_breadcrumbs(engine);

	if (tasklet)
		tasklet_hi_schedule(&engine->sched_engine->tasklet);
}

static void __execlists_kick(struct intel_engine_execlists *execlists)
{
	struct intel_engine_cs *engine =
		container_of(execlists, typeof(*engine), execlists);

	 
	tasklet_hi_schedule(&engine->sched_engine->tasklet);
}

#define execlists_kick(t, member) \
	__execlists_kick(container_of(t, struct intel_engine_execlists, member))

static void execlists_timeslice(struct timer_list *timer)
{
	execlists_kick(timer, timer);
}

static void execlists_preempt(struct timer_list *timer)
{
	execlists_kick(timer, preempt);
}

static void queue_request(struct intel_engine_cs *engine,
			  struct i915_request *rq)
{
	GEM_BUG_ON(!list_empty(&rq->sched.link));
	list_add_tail(&rq->sched.link,
		      i915_sched_lookup_priolist(engine->sched_engine,
						 rq_prio(rq)));
	set_bit(I915_FENCE_FLAG_PQUEUE, &rq->fence.flags);
}

static bool submit_queue(struct intel_engine_cs *engine,
			 const struct i915_request *rq)
{
	struct i915_sched_engine *sched_engine = engine->sched_engine;

	if (rq_prio(rq) <= sched_engine->queue_priority_hint)
		return false;

	sched_engine->queue_priority_hint = rq_prio(rq);
	return true;
}

static bool ancestor_on_hold(const struct intel_engine_cs *engine,
			     const struct i915_request *rq)
{
	GEM_BUG_ON(i915_request_on_hold(rq));
	return !list_empty(&engine->sched_engine->hold) && hold_request(rq);
}

static void execlists_submit_request(struct i915_request *request)
{
	struct intel_engine_cs *engine = request->engine;
	unsigned long flags;

	 
	spin_lock_irqsave(&engine->sched_engine->lock, flags);

	if (unlikely(ancestor_on_hold(engine, request))) {
		RQ_TRACE(request, "ancestor on hold\n");
		list_add_tail(&request->sched.link,
			      &engine->sched_engine->hold);
		i915_request_set_hold(request);
	} else {
		queue_request(engine, request);

		GEM_BUG_ON(i915_sched_engine_is_empty(engine->sched_engine));
		GEM_BUG_ON(list_empty(&request->sched.link));

		if (submit_queue(engine, request))
			__execlists_kick(&engine->execlists);
	}

	spin_unlock_irqrestore(&engine->sched_engine->lock, flags);
}

static int
__execlists_context_pre_pin(struct intel_context *ce,
			    struct intel_engine_cs *engine,
			    struct i915_gem_ww_ctx *ww, void **vaddr)
{
	int err;

	err = lrc_pre_pin(ce, engine, ww, vaddr);
	if (err)
		return err;

	if (!__test_and_set_bit(CONTEXT_INIT_BIT, &ce->flags)) {
		lrc_init_state(ce, engine, *vaddr);

		__i915_gem_object_flush_map(ce->state->obj, 0, engine->context_size);
	}

	return 0;
}

static int execlists_context_pre_pin(struct intel_context *ce,
				     struct i915_gem_ww_ctx *ww,
				     void **vaddr)
{
	return __execlists_context_pre_pin(ce, ce->engine, ww, vaddr);
}

static int execlists_context_pin(struct intel_context *ce, void *vaddr)
{
	return lrc_pin(ce, ce->engine, vaddr);
}

static int execlists_context_alloc(struct intel_context *ce)
{
	return lrc_alloc(ce, ce->engine);
}

static void execlists_context_cancel_request(struct intel_context *ce,
					     struct i915_request *rq)
{
	struct intel_engine_cs *engine = NULL;

	i915_request_active_engine(rq, &engine);

	if (engine && intel_engine_pulse(engine))
		intel_gt_handle_error(engine->gt, engine->mask, 0,
				      "request cancellation by %s",
				      current->comm);
}

static struct intel_context *
execlists_create_parallel(struct intel_engine_cs **engines,
			  unsigned int num_siblings,
			  unsigned int width)
{
	struct intel_context *parent = NULL, *ce, *err;
	int i;

	GEM_BUG_ON(num_siblings != 1);

	for (i = 0; i < width; ++i) {
		ce = intel_context_create(engines[i]);
		if (IS_ERR(ce)) {
			err = ce;
			goto unwind;
		}

		if (i == 0)
			parent = ce;
		else
			intel_context_bind_parent_child(parent, ce);
	}

	parent->parallel.fence_context = dma_fence_context_alloc(1);

	intel_context_set_nopreempt(parent);
	for_each_child(parent, ce)
		intel_context_set_nopreempt(ce);

	return parent;

unwind:
	if (parent)
		intel_context_put(parent);
	return err;
}

static const struct intel_context_ops execlists_context_ops = {
	.flags = COPS_HAS_INFLIGHT | COPS_RUNTIME_CYCLES,

	.alloc = execlists_context_alloc,

	.cancel_request = execlists_context_cancel_request,

	.pre_pin = execlists_context_pre_pin,
	.pin = execlists_context_pin,
	.unpin = lrc_unpin,
	.post_unpin = lrc_post_unpin,

	.enter = intel_context_enter_engine,
	.exit = intel_context_exit_engine,

	.reset = lrc_reset,
	.destroy = lrc_destroy,

	.create_parallel = execlists_create_parallel,
	.create_virtual = execlists_create_virtual,
};

static int emit_pdps(struct i915_request *rq)
{
	const struct intel_engine_cs * const engine = rq->engine;
	struct i915_ppgtt * const ppgtt = i915_vm_to_ppgtt(rq->context->vm);
	int err, i;
	u32 *cs;

	GEM_BUG_ON(intel_vgpu_active(rq->i915));

	 

	cs = intel_ring_begin(rq, 2);
	if (IS_ERR(cs))
		return PTR_ERR(cs);

	*cs++ = MI_ARB_ON_OFF | MI_ARB_DISABLE;
	*cs++ = MI_NOOP;
	intel_ring_advance(rq, cs);

	 
	err = engine->emit_flush(rq, EMIT_FLUSH);
	if (err)
		return err;

	 
	err = engine->emit_flush(rq, EMIT_INVALIDATE);
	if (err)
		return err;

	cs = intel_ring_begin(rq, 4 * GEN8_3LVL_PDPES + 2);
	if (IS_ERR(cs))
		return PTR_ERR(cs);

	 
	*cs++ = MI_LOAD_REGISTER_IMM(2 * GEN8_3LVL_PDPES) | MI_LRI_FORCE_POSTED;
	for (i = GEN8_3LVL_PDPES; i--; ) {
		const dma_addr_t pd_daddr = i915_page_dir_dma_addr(ppgtt, i);
		u32 base = engine->mmio_base;

		*cs++ = i915_mmio_reg_offset(GEN8_RING_PDP_UDW(base, i));
		*cs++ = upper_32_bits(pd_daddr);
		*cs++ = i915_mmio_reg_offset(GEN8_RING_PDP_LDW(base, i));
		*cs++ = lower_32_bits(pd_daddr);
	}
	*cs++ = MI_ARB_ON_OFF | MI_ARB_ENABLE;
	intel_ring_advance(rq, cs);

	intel_ring_advance(rq, cs);

	return 0;
}

static int execlists_request_alloc(struct i915_request *request)
{
	int ret;

	GEM_BUG_ON(!intel_context_is_pinned(request->context));

	 
	request->reserved_space += EXECLISTS_REQUEST_SIZE;

	 

	if (!i915_vm_is_4lvl(request->context->vm)) {
		ret = emit_pdps(request);
		if (ret)
			return ret;
	}

	 
	ret = request->engine->emit_flush(request, EMIT_INVALIDATE);
	if (ret)
		return ret;

	request->reserved_space -= EXECLISTS_REQUEST_SIZE;
	return 0;
}

static void reset_csb_pointers(struct intel_engine_cs *engine)
{
	struct intel_engine_execlists * const execlists = &engine->execlists;
	const unsigned int reset_value = execlists->csb_size - 1;

	ring_set_paused(engine, 0);

	 
	ENGINE_WRITE(engine, RING_CONTEXT_STATUS_PTR,
		     0xffff << 16 | reset_value << 8 | reset_value);
	ENGINE_POSTING_READ(engine, RING_CONTEXT_STATUS_PTR);

	 
	execlists->csb_head = reset_value;
	WRITE_ONCE(*execlists->csb_write, reset_value);
	wmb();  

	 
	memset(execlists->csb_status, -1, (reset_value + 1) * sizeof(u64));
	drm_clflush_virt_range(execlists->csb_status,
			       execlists->csb_size *
			       sizeof(execlists->csb_status));

	 
	ENGINE_WRITE(engine, RING_CONTEXT_STATUS_PTR,
		     0xffff << 16 | reset_value << 8 | reset_value);
	ENGINE_POSTING_READ(engine, RING_CONTEXT_STATUS_PTR);

	GEM_BUG_ON(READ_ONCE(*execlists->csb_write) != reset_value);
}

static void sanitize_hwsp(struct intel_engine_cs *engine)
{
	struct intel_timeline *tl;

	list_for_each_entry(tl, &engine->status_page.timelines, engine_link)
		intel_timeline_reset_seqno(tl);
}

static void execlists_sanitize(struct intel_engine_cs *engine)
{
	GEM_BUG_ON(execlists_active(&engine->execlists));

	 
	if (IS_ENABLED(CONFIG_DRM_I915_DEBUG_GEM))
		memset(engine->status_page.addr, POISON_INUSE, PAGE_SIZE);

	reset_csb_pointers(engine);

	 
	sanitize_hwsp(engine);

	 
	drm_clflush_virt_range(engine->status_page.addr, PAGE_SIZE);

	intel_engine_reset_pinned_contexts(engine);
}

static void enable_error_interrupt(struct intel_engine_cs *engine)
{
	u32 status;

	engine->execlists.error_interrupt = 0;
	ENGINE_WRITE(engine, RING_EMR, ~0u);
	ENGINE_WRITE(engine, RING_EIR, ~0u);  

	status = ENGINE_READ(engine, RING_ESR);
	if (unlikely(status)) {
		drm_err(&engine->i915->drm,
			"engine '%s' resumed still in error: %08x\n",
			engine->name, status);
		__intel_gt_reset(engine->gt, engine->mask);
	}

	 
	ENGINE_WRITE(engine, RING_EMR, ~I915_ERROR_INSTRUCTION);
}

static void enable_execlists(struct intel_engine_cs *engine)
{
	u32 mode;

	assert_forcewakes_active(engine->uncore, FORCEWAKE_ALL);

	intel_engine_set_hwsp_writemask(engine, ~0u);  

	if (GRAPHICS_VER(engine->i915) >= 11)
		mode = _MASKED_BIT_ENABLE(GEN11_GFX_DISABLE_LEGACY_MODE);
	else
		mode = _MASKED_BIT_ENABLE(GFX_RUN_LIST_ENABLE);
	ENGINE_WRITE_FW(engine, RING_MODE_GEN7, mode);

	ENGINE_WRITE_FW(engine, RING_MI_MODE, _MASKED_BIT_DISABLE(STOP_RING));

	ENGINE_WRITE_FW(engine,
			RING_HWS_PGA,
			i915_ggtt_offset(engine->status_page.vma));
	ENGINE_POSTING_READ(engine, RING_HWS_PGA);

	enable_error_interrupt(engine);
}

static int execlists_resume(struct intel_engine_cs *engine)
{
	intel_mocs_init_engine(engine);
	intel_breadcrumbs_reset(engine->breadcrumbs);

	enable_execlists(engine);

	if (engine->flags & I915_ENGINE_FIRST_RENDER_COMPUTE)
		xehp_enable_ccs_engines(engine);

	return 0;
}

static void execlists_reset_prepare(struct intel_engine_cs *engine)
{
	ENGINE_TRACE(engine, "depth<-%d\n",
		     atomic_read(&engine->sched_engine->tasklet.count));

	 
	__tasklet_disable_sync_once(&engine->sched_engine->tasklet);
	GEM_BUG_ON(!reset_in_progress(engine));

	 
	ring_set_paused(engine, 1);
	intel_engine_stop_cs(engine);

	 
	if (IS_MTL_GRAPHICS_STEP(engine->i915, M, STEP_A0, STEP_B0) ||
	    (GRAPHICS_VER(engine->i915) >= 11 &&
	    GRAPHICS_VER_FULL(engine->i915) < IP_VER(12, 70)))
		intel_engine_wait_for_pending_mi_fw(engine);

	engine->execlists.reset_ccid = active_ccid(engine);
}

static struct i915_request **
reset_csb(struct intel_engine_cs *engine, struct i915_request **inactive)
{
	struct intel_engine_execlists * const execlists = &engine->execlists;

	drm_clflush_virt_range(execlists->csb_write,
			       sizeof(execlists->csb_write[0]));

	inactive = process_csb(engine, inactive);  

	 
	reset_csb_pointers(engine);

	return inactive;
}

static void
execlists_reset_active(struct intel_engine_cs *engine, bool stalled)
{
	struct intel_context *ce;
	struct i915_request *rq;
	u32 head;

	 
	rq = active_context(engine, engine->execlists.reset_ccid);
	if (!rq)
		return;

	ce = rq->context;
	GEM_BUG_ON(!i915_vma_is_pinned(ce->state));

	if (__i915_request_is_complete(rq)) {
		 
		head = intel_ring_wrap(ce->ring, rq->tail);
		goto out_replay;
	}

	 
	GEM_BUG_ON(!intel_engine_pm_is_awake(engine));

	 
	GEM_BUG_ON(i915_active_is_idle(&ce->active));

	rq = active_request(ce->timeline, rq);
	head = intel_ring_wrap(ce->ring, rq->head);
	GEM_BUG_ON(head == ce->ring->tail);

	 
	if (!__i915_request_has_started(rq))
		goto out_replay;

	 
	__i915_request_reset(rq, stalled);

	 
out_replay:
	ENGINE_TRACE(engine, "replay {head:%04x, tail:%04x}\n",
		     head, ce->ring->tail);
	lrc_reset_regs(ce, engine);
	ce->lrc.lrca = lrc_update_regs(ce, engine, head);
}

static void execlists_reset_csb(struct intel_engine_cs *engine, bool stalled)
{
	struct intel_engine_execlists * const execlists = &engine->execlists;
	struct i915_request *post[2 * EXECLIST_MAX_PORTS];
	struct i915_request **inactive;

	rcu_read_lock();
	inactive = reset_csb(engine, post);

	execlists_reset_active(engine, true);

	inactive = cancel_port_requests(execlists, inactive);
	post_process_csb(post, inactive);
	rcu_read_unlock();
}

static void execlists_reset_rewind(struct intel_engine_cs *engine, bool stalled)
{
	unsigned long flags;

	ENGINE_TRACE(engine, "\n");

	 
	execlists_reset_csb(engine, stalled);

	 
	rcu_read_lock();
	spin_lock_irqsave(&engine->sched_engine->lock, flags);
	__unwind_incomplete_requests(engine);
	spin_unlock_irqrestore(&engine->sched_engine->lock, flags);
	rcu_read_unlock();
}

static void nop_submission_tasklet(struct tasklet_struct *t)
{
	struct i915_sched_engine *sched_engine =
		from_tasklet(sched_engine, t, tasklet);
	struct intel_engine_cs * const engine = sched_engine->private_data;

	 
	WRITE_ONCE(engine->sched_engine->queue_priority_hint, INT_MIN);
}

static void execlists_reset_cancel(struct intel_engine_cs *engine)
{
	struct intel_engine_execlists * const execlists = &engine->execlists;
	struct i915_sched_engine * const sched_engine = engine->sched_engine;
	struct i915_request *rq, *rn;
	struct rb_node *rb;
	unsigned long flags;

	ENGINE_TRACE(engine, "\n");

	 
	execlists_reset_csb(engine, true);

	rcu_read_lock();
	spin_lock_irqsave(&engine->sched_engine->lock, flags);

	 
	list_for_each_entry(rq, &engine->sched_engine->requests, sched.link)
		i915_request_put(i915_request_mark_eio(rq));
	intel_engine_signal_breadcrumbs(engine);

	 
	while ((rb = rb_first_cached(&sched_engine->queue))) {
		struct i915_priolist *p = to_priolist(rb);

		priolist_for_each_request_consume(rq, rn, p) {
			if (i915_request_mark_eio(rq)) {
				__i915_request_submit(rq);
				i915_request_put(rq);
			}
		}

		rb_erase_cached(&p->node, &sched_engine->queue);
		i915_priolist_free(p);
	}

	 
	list_for_each_entry(rq, &sched_engine->hold, sched.link)
		i915_request_put(i915_request_mark_eio(rq));

	 
	while ((rb = rb_first_cached(&execlists->virtual))) {
		struct virtual_engine *ve =
			rb_entry(rb, typeof(*ve), nodes[engine->id].rb);

		rb_erase_cached(rb, &execlists->virtual);
		RB_CLEAR_NODE(rb);

		spin_lock(&ve->base.sched_engine->lock);
		rq = fetch_and_zero(&ve->request);
		if (rq) {
			if (i915_request_mark_eio(rq)) {
				rq->engine = engine;
				__i915_request_submit(rq);
				i915_request_put(rq);
			}
			i915_request_put(rq);

			ve->base.sched_engine->queue_priority_hint = INT_MIN;
		}
		spin_unlock(&ve->base.sched_engine->lock);
	}

	 

	sched_engine->queue_priority_hint = INT_MIN;
	sched_engine->queue = RB_ROOT_CACHED;

	GEM_BUG_ON(__tasklet_is_enabled(&engine->sched_engine->tasklet));
	engine->sched_engine->tasklet.callback = nop_submission_tasklet;

	spin_unlock_irqrestore(&engine->sched_engine->lock, flags);
	rcu_read_unlock();
}

static void execlists_reset_finish(struct intel_engine_cs *engine)
{
	struct intel_engine_execlists * const execlists = &engine->execlists;

	 
	GEM_BUG_ON(!reset_in_progress(engine));

	 
	if (__tasklet_enable(&engine->sched_engine->tasklet))
		__execlists_kick(execlists);

	ENGINE_TRACE(engine, "depth->%d\n",
		     atomic_read(&engine->sched_engine->tasklet.count));
}

static void gen8_logical_ring_enable_irq(struct intel_engine_cs *engine)
{
	ENGINE_WRITE(engine, RING_IMR,
		     ~(engine->irq_enable_mask | engine->irq_keep_mask));
	ENGINE_POSTING_READ(engine, RING_IMR);
}

static void gen8_logical_ring_disable_irq(struct intel_engine_cs *engine)
{
	ENGINE_WRITE(engine, RING_IMR, ~engine->irq_keep_mask);
}

static void execlists_park(struct intel_engine_cs *engine)
{
	cancel_timer(&engine->execlists.timer);
	cancel_timer(&engine->execlists.preempt);
}

static void add_to_engine(struct i915_request *rq)
{
	lockdep_assert_held(&rq->engine->sched_engine->lock);
	list_move_tail(&rq->sched.link, &rq->engine->sched_engine->requests);
}

static void remove_from_engine(struct i915_request *rq)
{
	struct intel_engine_cs *engine, *locked;

	 
	locked = READ_ONCE(rq->engine);
	spin_lock_irq(&locked->sched_engine->lock);
	while (unlikely(locked != (engine = READ_ONCE(rq->engine)))) {
		spin_unlock(&locked->sched_engine->lock);
		spin_lock(&engine->sched_engine->lock);
		locked = engine;
	}
	list_del_init(&rq->sched.link);

	clear_bit(I915_FENCE_FLAG_PQUEUE, &rq->fence.flags);
	clear_bit(I915_FENCE_FLAG_HOLD, &rq->fence.flags);

	 
	set_bit(I915_FENCE_FLAG_ACTIVE, &rq->fence.flags);

	spin_unlock_irq(&locked->sched_engine->lock);

	i915_request_notify_execute_cb_imm(rq);
}

static bool can_preempt(struct intel_engine_cs *engine)
{
	if (GRAPHICS_VER(engine->i915) > 8)
		return true;

	 
	return engine->class != RENDER_CLASS;
}

static void kick_execlists(const struct i915_request *rq, int prio)
{
	struct intel_engine_cs *engine = rq->engine;
	struct i915_sched_engine *sched_engine = engine->sched_engine;
	const struct i915_request *inflight;

	 
	if (prio <= sched_engine->queue_priority_hint)
		return;

	rcu_read_lock();

	 
	inflight = execlists_active(&engine->execlists);
	if (!inflight)
		goto unlock;

	 
	if (inflight->context == rq->context)
		goto unlock;

	ENGINE_TRACE(engine,
		     "bumping queue-priority-hint:%d for rq:%llx:%lld, inflight:%llx:%lld prio %d\n",
		     prio,
		     rq->fence.context, rq->fence.seqno,
		     inflight->fence.context, inflight->fence.seqno,
		     inflight->sched.attr.priority);

	sched_engine->queue_priority_hint = prio;

	 
	if (prio >= max(I915_PRIORITY_NORMAL, rq_prio(inflight)))
		tasklet_hi_schedule(&sched_engine->tasklet);

unlock:
	rcu_read_unlock();
}

static void execlists_set_default_submission(struct intel_engine_cs *engine)
{
	engine->submit_request = execlists_submit_request;
	engine->sched_engine->schedule = i915_schedule;
	engine->sched_engine->kick_backend = kick_execlists;
	engine->sched_engine->tasklet.callback = execlists_submission_tasklet;
}

static void execlists_shutdown(struct intel_engine_cs *engine)
{
	 
	del_timer_sync(&engine->execlists.timer);
	del_timer_sync(&engine->execlists.preempt);
	tasklet_kill(&engine->sched_engine->tasklet);
}

static void execlists_release(struct intel_engine_cs *engine)
{
	engine->sanitize = NULL;  

	execlists_shutdown(engine);

	intel_engine_cleanup_common(engine);
	lrc_fini_wa_ctx(engine);
}

static ktime_t __execlists_engine_busyness(struct intel_engine_cs *engine,
					   ktime_t *now)
{
	struct intel_engine_execlists_stats *stats = &engine->stats.execlists;
	ktime_t total = stats->total;

	 
	*now = ktime_get();
	if (READ_ONCE(stats->active))
		total = ktime_add(total, ktime_sub(*now, stats->start));

	return total;
}

static ktime_t execlists_engine_busyness(struct intel_engine_cs *engine,
					 ktime_t *now)
{
	struct intel_engine_execlists_stats *stats = &engine->stats.execlists;
	unsigned int seq;
	ktime_t total;

	do {
		seq = read_seqcount_begin(&stats->lock);
		total = __execlists_engine_busyness(engine, now);
	} while (read_seqcount_retry(&stats->lock, seq));

	return total;
}

static void
logical_ring_default_vfuncs(struct intel_engine_cs *engine)
{
	 

	engine->resume = execlists_resume;

	engine->cops = &execlists_context_ops;
	engine->request_alloc = execlists_request_alloc;
	engine->add_active_request = add_to_engine;
	engine->remove_active_request = remove_from_engine;

	engine->reset.prepare = execlists_reset_prepare;
	engine->reset.rewind = execlists_reset_rewind;
	engine->reset.cancel = execlists_reset_cancel;
	engine->reset.finish = execlists_reset_finish;

	engine->park = execlists_park;
	engine->unpark = NULL;

	engine->emit_flush = gen8_emit_flush_xcs;
	engine->emit_init_breadcrumb = gen8_emit_init_breadcrumb;
	engine->emit_fini_breadcrumb = gen8_emit_fini_breadcrumb_xcs;
	if (GRAPHICS_VER(engine->i915) >= 12) {
		engine->emit_fini_breadcrumb = gen12_emit_fini_breadcrumb_xcs;
		engine->emit_flush = gen12_emit_flush_xcs;
	}
	engine->set_default_submission = execlists_set_default_submission;

	if (GRAPHICS_VER(engine->i915) < 11) {
		engine->irq_enable = gen8_logical_ring_enable_irq;
		engine->irq_disable = gen8_logical_ring_disable_irq;
	} else {
		 
	}
	intel_engine_set_irq_handler(engine, execlists_irq_handler);

	engine->flags |= I915_ENGINE_SUPPORTS_STATS;
	if (!intel_vgpu_active(engine->i915)) {
		engine->flags |= I915_ENGINE_HAS_SEMAPHORES;
		if (can_preempt(engine)) {
			engine->flags |= I915_ENGINE_HAS_PREEMPTION;
			if (CONFIG_DRM_I915_TIMESLICE_DURATION)
				engine->flags |= I915_ENGINE_HAS_TIMESLICES;
		}
	}

	if (GRAPHICS_VER_FULL(engine->i915) >= IP_VER(12, 50)) {
		if (intel_engine_has_preemption(engine))
			engine->emit_bb_start = xehp_emit_bb_start;
		else
			engine->emit_bb_start = xehp_emit_bb_start_noarb;
	} else {
		if (intel_engine_has_preemption(engine))
			engine->emit_bb_start = gen8_emit_bb_start;
		else
			engine->emit_bb_start = gen8_emit_bb_start_noarb;
	}

	engine->busyness = execlists_engine_busyness;
}

static void logical_ring_default_irqs(struct intel_engine_cs *engine)
{
	unsigned int shift = 0;

	if (GRAPHICS_VER(engine->i915) < 11) {
		const u8 irq_shifts[] = {
			[RCS0]  = GEN8_RCS_IRQ_SHIFT,
			[BCS0]  = GEN8_BCS_IRQ_SHIFT,
			[VCS0]  = GEN8_VCS0_IRQ_SHIFT,
			[VCS1]  = GEN8_VCS1_IRQ_SHIFT,
			[VECS0] = GEN8_VECS_IRQ_SHIFT,
		};

		shift = irq_shifts[engine->id];
	}

	engine->irq_enable_mask = GT_RENDER_USER_INTERRUPT << shift;
	engine->irq_keep_mask = GT_CONTEXT_SWITCH_INTERRUPT << shift;
	engine->irq_keep_mask |= GT_CS_MASTER_ERROR_INTERRUPT << shift;
	engine->irq_keep_mask |= GT_WAIT_SEMAPHORE_INTERRUPT << shift;
}

static void rcs_submission_override(struct intel_engine_cs *engine)
{
	switch (GRAPHICS_VER(engine->i915)) {
	case 12:
		engine->emit_flush = gen12_emit_flush_rcs;
		engine->emit_fini_breadcrumb = gen12_emit_fini_breadcrumb_rcs;
		break;
	case 11:
		engine->emit_flush = gen11_emit_flush_rcs;
		engine->emit_fini_breadcrumb = gen11_emit_fini_breadcrumb_rcs;
		break;
	default:
		engine->emit_flush = gen8_emit_flush_rcs;
		engine->emit_fini_breadcrumb = gen8_emit_fini_breadcrumb_rcs;
		break;
	}
}

int intel_execlists_submission_setup(struct intel_engine_cs *engine)
{
	struct intel_engine_execlists * const execlists = &engine->execlists;
	struct drm_i915_private *i915 = engine->i915;
	struct intel_uncore *uncore = engine->uncore;
	u32 base = engine->mmio_base;

	tasklet_setup(&engine->sched_engine->tasklet, execlists_submission_tasklet);
	timer_setup(&engine->execlists.timer, execlists_timeslice, 0);
	timer_setup(&engine->execlists.preempt, execlists_preempt, 0);

	logical_ring_default_vfuncs(engine);
	logical_ring_default_irqs(engine);

	seqcount_init(&engine->stats.execlists.lock);

	if (engine->flags & I915_ENGINE_HAS_RCS_REG_STATE)
		rcs_submission_override(engine);

	lrc_init_wa_ctx(engine);

	if (HAS_LOGICAL_RING_ELSQ(i915)) {
		execlists->submit_reg = intel_uncore_regs(uncore) +
			i915_mmio_reg_offset(RING_EXECLIST_SQ_CONTENTS(base));
		execlists->ctrl_reg = intel_uncore_regs(uncore) +
			i915_mmio_reg_offset(RING_EXECLIST_CONTROL(base));

		engine->fw_domain = intel_uncore_forcewake_for_reg(engine->uncore,
				    RING_EXECLIST_CONTROL(engine->mmio_base),
				    FW_REG_WRITE);
	} else {
		execlists->submit_reg = intel_uncore_regs(uncore) +
			i915_mmio_reg_offset(RING_ELSP(base));
	}

	execlists->csb_status =
		(u64 *)&engine->status_page.addr[I915_HWS_CSB_BUF0_INDEX];

	execlists->csb_write =
		&engine->status_page.addr[INTEL_HWS_CSB_WRITE_INDEX(i915)];

	if (GRAPHICS_VER(i915) < 11)
		execlists->csb_size = GEN8_CSB_ENTRIES;
	else
		execlists->csb_size = GEN11_CSB_ENTRIES;

	engine->context_tag = GENMASK(BITS_PER_LONG - 2, 0);
	if (GRAPHICS_VER(engine->i915) >= 11 &&
	    GRAPHICS_VER_FULL(engine->i915) < IP_VER(12, 50)) {
		execlists->ccid |= engine->instance << (GEN11_ENGINE_INSTANCE_SHIFT - 32);
		execlists->ccid |= engine->class << (GEN11_ENGINE_CLASS_SHIFT - 32);
	}

	 
	engine->sanitize = execlists_sanitize;
	engine->release = execlists_release;

	return 0;
}

static struct list_head *virtual_queue(struct virtual_engine *ve)
{
	return &ve->base.sched_engine->default_priolist.requests;
}

static void rcu_virtual_context_destroy(struct work_struct *wrk)
{
	struct virtual_engine *ve =
		container_of(wrk, typeof(*ve), rcu.work);
	unsigned int n;

	GEM_BUG_ON(ve->context.inflight);

	 
	if (unlikely(ve->request)) {
		struct i915_request *old;

		spin_lock_irq(&ve->base.sched_engine->lock);

		old = fetch_and_zero(&ve->request);
		if (old) {
			GEM_BUG_ON(!__i915_request_is_complete(old));
			__i915_request_submit(old);
			i915_request_put(old);
		}

		spin_unlock_irq(&ve->base.sched_engine->lock);
	}

	 
	tasklet_kill(&ve->base.sched_engine->tasklet);

	 
	for (n = 0; n < ve->num_siblings; n++) {
		struct intel_engine_cs *sibling = ve->siblings[n];
		struct rb_node *node = &ve->nodes[sibling->id].rb;

		if (RB_EMPTY_NODE(node))
			continue;

		spin_lock_irq(&sibling->sched_engine->lock);

		 
		if (!RB_EMPTY_NODE(node))
			rb_erase_cached(node, &sibling->execlists.virtual);

		spin_unlock_irq(&sibling->sched_engine->lock);
	}
	GEM_BUG_ON(__tasklet_is_scheduled(&ve->base.sched_engine->tasklet));
	GEM_BUG_ON(!list_empty(virtual_queue(ve)));

	lrc_fini(&ve->context);
	intel_context_fini(&ve->context);

	if (ve->base.breadcrumbs)
		intel_breadcrumbs_put(ve->base.breadcrumbs);
	if (ve->base.sched_engine)
		i915_sched_engine_put(ve->base.sched_engine);
	intel_engine_free_request_pool(&ve->base);

	kfree(ve);
}

static void virtual_context_destroy(struct kref *kref)
{
	struct virtual_engine *ve =
		container_of(kref, typeof(*ve), context.ref);

	GEM_BUG_ON(!list_empty(&ve->context.signals));

	 
	INIT_RCU_WORK(&ve->rcu, rcu_virtual_context_destroy);
	queue_rcu_work(ve->context.engine->i915->unordered_wq, &ve->rcu);
}

static void virtual_engine_initial_hint(struct virtual_engine *ve)
{
	int swp;

	 
	swp = get_random_u32_below(ve->num_siblings);
	if (swp)
		swap(ve->siblings[swp], ve->siblings[0]);
}

static int virtual_context_alloc(struct intel_context *ce)
{
	struct virtual_engine *ve = container_of(ce, typeof(*ve), context);

	return lrc_alloc(ce, ve->siblings[0]);
}

static int virtual_context_pre_pin(struct intel_context *ce,
				   struct i915_gem_ww_ctx *ww,
				   void **vaddr)
{
	struct virtual_engine *ve = container_of(ce, typeof(*ve), context);

	  
	return __execlists_context_pre_pin(ce, ve->siblings[0], ww, vaddr);
}

static int virtual_context_pin(struct intel_context *ce, void *vaddr)
{
	struct virtual_engine *ve = container_of(ce, typeof(*ve), context);

	return lrc_pin(ce, ve->siblings[0], vaddr);
}

static void virtual_context_enter(struct intel_context *ce)
{
	struct virtual_engine *ve = container_of(ce, typeof(*ve), context);
	unsigned int n;

	for (n = 0; n < ve->num_siblings; n++)
		intel_engine_pm_get(ve->siblings[n]);

	intel_timeline_enter(ce->timeline);
}

static void virtual_context_exit(struct intel_context *ce)
{
	struct virtual_engine *ve = container_of(ce, typeof(*ve), context);
	unsigned int n;

	intel_timeline_exit(ce->timeline);

	for (n = 0; n < ve->num_siblings; n++)
		intel_engine_pm_put(ve->siblings[n]);
}

static struct intel_engine_cs *
virtual_get_sibling(struct intel_engine_cs *engine, unsigned int sibling)
{
	struct virtual_engine *ve = to_virtual_engine(engine);

	if (sibling >= ve->num_siblings)
		return NULL;

	return ve->siblings[sibling];
}

static const struct intel_context_ops virtual_context_ops = {
	.flags = COPS_HAS_INFLIGHT | COPS_RUNTIME_CYCLES,

	.alloc = virtual_context_alloc,

	.cancel_request = execlists_context_cancel_request,

	.pre_pin = virtual_context_pre_pin,
	.pin = virtual_context_pin,
	.unpin = lrc_unpin,
	.post_unpin = lrc_post_unpin,

	.enter = virtual_context_enter,
	.exit = virtual_context_exit,

	.destroy = virtual_context_destroy,

	.get_sibling = virtual_get_sibling,
};

static intel_engine_mask_t virtual_submission_mask(struct virtual_engine *ve)
{
	struct i915_request *rq;
	intel_engine_mask_t mask;

	rq = READ_ONCE(ve->request);
	if (!rq)
		return 0;

	 
	mask = rq->execution_mask;
	if (unlikely(!mask)) {
		 
		i915_request_set_error_once(rq, -ENODEV);
		mask = ve->siblings[0]->mask;
	}

	ENGINE_TRACE(&ve->base, "rq=%llx:%lld, mask=%x, prio=%d\n",
		     rq->fence.context, rq->fence.seqno,
		     mask, ve->base.sched_engine->queue_priority_hint);

	return mask;
}

static void virtual_submission_tasklet(struct tasklet_struct *t)
{
	struct i915_sched_engine *sched_engine =
		from_tasklet(sched_engine, t, tasklet);
	struct virtual_engine * const ve =
		(struct virtual_engine *)sched_engine->private_data;
	const int prio = READ_ONCE(sched_engine->queue_priority_hint);
	intel_engine_mask_t mask;
	unsigned int n;

	rcu_read_lock();
	mask = virtual_submission_mask(ve);
	rcu_read_unlock();
	if (unlikely(!mask))
		return;

	for (n = 0; n < ve->num_siblings; n++) {
		struct intel_engine_cs *sibling = READ_ONCE(ve->siblings[n]);
		struct ve_node * const node = &ve->nodes[sibling->id];
		struct rb_node **parent, *rb;
		bool first;

		if (!READ_ONCE(ve->request))
			break;  

		spin_lock_irq(&sibling->sched_engine->lock);

		if (unlikely(!(mask & sibling->mask))) {
			if (!RB_EMPTY_NODE(&node->rb)) {
				rb_erase_cached(&node->rb,
						&sibling->execlists.virtual);
				RB_CLEAR_NODE(&node->rb);
			}

			goto unlock_engine;
		}

		if (unlikely(!RB_EMPTY_NODE(&node->rb))) {
			 
			first = rb_first_cached(&sibling->execlists.virtual) ==
				&node->rb;
			if (prio == node->prio || (prio > node->prio && first))
				goto submit_engine;

			rb_erase_cached(&node->rb, &sibling->execlists.virtual);
		}

		rb = NULL;
		first = true;
		parent = &sibling->execlists.virtual.rb_root.rb_node;
		while (*parent) {
			struct ve_node *other;

			rb = *parent;
			other = rb_entry(rb, typeof(*other), rb);
			if (prio > other->prio) {
				parent = &rb->rb_left;
			} else {
				parent = &rb->rb_right;
				first = false;
			}
		}

		rb_link_node(&node->rb, rb, parent);
		rb_insert_color_cached(&node->rb,
				       &sibling->execlists.virtual,
				       first);

submit_engine:
		GEM_BUG_ON(RB_EMPTY_NODE(&node->rb));
		node->prio = prio;
		if (first && prio > sibling->sched_engine->queue_priority_hint)
			tasklet_hi_schedule(&sibling->sched_engine->tasklet);

unlock_engine:
		spin_unlock_irq(&sibling->sched_engine->lock);

		if (intel_context_inflight(&ve->context))
			break;
	}
}

static void virtual_submit_request(struct i915_request *rq)
{
	struct virtual_engine *ve = to_virtual_engine(rq->engine);
	unsigned long flags;

	ENGINE_TRACE(&ve->base, "rq=%llx:%lld\n",
		     rq->fence.context,
		     rq->fence.seqno);

	GEM_BUG_ON(ve->base.submit_request != virtual_submit_request);

	spin_lock_irqsave(&ve->base.sched_engine->lock, flags);

	 
	if (__i915_request_is_complete(rq)) {
		__i915_request_submit(rq);
		goto unlock;
	}

	if (ve->request) {  
		GEM_BUG_ON(!__i915_request_is_complete(ve->request));
		__i915_request_submit(ve->request);
		i915_request_put(ve->request);
	}

	ve->base.sched_engine->queue_priority_hint = rq_prio(rq);
	ve->request = i915_request_get(rq);

	GEM_BUG_ON(!list_empty(virtual_queue(ve)));
	list_move_tail(&rq->sched.link, virtual_queue(ve));

	tasklet_hi_schedule(&ve->base.sched_engine->tasklet);

unlock:
	spin_unlock_irqrestore(&ve->base.sched_engine->lock, flags);
}

static struct intel_context *
execlists_create_virtual(struct intel_engine_cs **siblings, unsigned int count,
			 unsigned long flags)
{
	struct drm_i915_private *i915 = siblings[0]->i915;
	struct virtual_engine *ve;
	unsigned int n;
	int err;

	ve = kzalloc(struct_size(ve, siblings, count), GFP_KERNEL);
	if (!ve)
		return ERR_PTR(-ENOMEM);

	ve->base.i915 = i915;
	ve->base.gt = siblings[0]->gt;
	ve->base.uncore = siblings[0]->uncore;
	ve->base.id = -1;

	ve->base.class = OTHER_CLASS;
	ve->base.uabi_class = I915_ENGINE_CLASS_INVALID;
	ve->base.instance = I915_ENGINE_CLASS_INVALID_VIRTUAL;
	ve->base.uabi_instance = I915_ENGINE_CLASS_INVALID_VIRTUAL;

	 
	ve->base.saturated = ALL_ENGINES;

	snprintf(ve->base.name, sizeof(ve->base.name), "virtual");

	intel_engine_init_execlists(&ve->base);

	ve->base.sched_engine = i915_sched_engine_create(ENGINE_VIRTUAL);
	if (!ve->base.sched_engine) {
		err = -ENOMEM;
		goto err_put;
	}
	ve->base.sched_engine->private_data = &ve->base;

	ve->base.cops = &virtual_context_ops;
	ve->base.request_alloc = execlists_request_alloc;

	ve->base.sched_engine->schedule = i915_schedule;
	ve->base.sched_engine->kick_backend = kick_execlists;
	ve->base.submit_request = virtual_submit_request;

	INIT_LIST_HEAD(virtual_queue(ve));
	tasklet_setup(&ve->base.sched_engine->tasklet, virtual_submission_tasklet);

	intel_context_init(&ve->context, &ve->base);

	ve->base.breadcrumbs = intel_breadcrumbs_create(NULL);
	if (!ve->base.breadcrumbs) {
		err = -ENOMEM;
		goto err_put;
	}

	for (n = 0; n < count; n++) {
		struct intel_engine_cs *sibling = siblings[n];

		GEM_BUG_ON(!is_power_of_2(sibling->mask));
		if (sibling->mask & ve->base.mask) {
			drm_dbg(&i915->drm,
				"duplicate %s entry in load balancer\n",
				sibling->name);
			err = -EINVAL;
			goto err_put;
		}

		 
		if (sibling->sched_engine->tasklet.callback !=
		    execlists_submission_tasklet) {
			err = -ENODEV;
			goto err_put;
		}

		GEM_BUG_ON(RB_EMPTY_NODE(&ve->nodes[sibling->id].rb));
		RB_CLEAR_NODE(&ve->nodes[sibling->id].rb);

		ve->siblings[ve->num_siblings++] = sibling;
		ve->base.mask |= sibling->mask;
		ve->base.logical_mask |= sibling->logical_mask;

		 
		if (ve->base.class != OTHER_CLASS) {
			if (ve->base.class != sibling->class) {
				drm_dbg(&i915->drm,
					"invalid mixing of engine class, sibling %d, already %d\n",
					sibling->class, ve->base.class);
				err = -EINVAL;
				goto err_put;
			}
			continue;
		}

		ve->base.class = sibling->class;
		ve->base.uabi_class = sibling->uabi_class;
		snprintf(ve->base.name, sizeof(ve->base.name),
			 "v%dx%d", ve->base.class, count);
		ve->base.context_size = sibling->context_size;

		ve->base.add_active_request = sibling->add_active_request;
		ve->base.remove_active_request = sibling->remove_active_request;
		ve->base.emit_bb_start = sibling->emit_bb_start;
		ve->base.emit_flush = sibling->emit_flush;
		ve->base.emit_init_breadcrumb = sibling->emit_init_breadcrumb;
		ve->base.emit_fini_breadcrumb = sibling->emit_fini_breadcrumb;
		ve->base.emit_fini_breadcrumb_dw =
			sibling->emit_fini_breadcrumb_dw;

		ve->base.flags = sibling->flags;
	}

	ve->base.flags |= I915_ENGINE_IS_VIRTUAL;

	virtual_engine_initial_hint(ve);
	return &ve->context;

err_put:
	intel_context_put(&ve->context);
	return ERR_PTR(err);
}

void intel_execlists_show_requests(struct intel_engine_cs *engine,
				   struct drm_printer *m,
				   void (*show_request)(struct drm_printer *m,
							const struct i915_request *rq,
							const char *prefix,
							int indent),
				   unsigned int max)
{
	const struct intel_engine_execlists *execlists = &engine->execlists;
	struct i915_sched_engine *sched_engine = engine->sched_engine;
	struct i915_request *rq, *last;
	unsigned long flags;
	unsigned int count;
	struct rb_node *rb;

	spin_lock_irqsave(&sched_engine->lock, flags);

	last = NULL;
	count = 0;
	list_for_each_entry(rq, &sched_engine->requests, sched.link) {
		if (count++ < max - 1)
			show_request(m, rq, "\t\t", 0);
		else
			last = rq;
	}
	if (last) {
		if (count > max) {
			drm_printf(m,
				   "\t\t...skipping %d executing requests...\n",
				   count - max);
		}
		show_request(m, last, "\t\t", 0);
	}

	if (sched_engine->queue_priority_hint != INT_MIN)
		drm_printf(m, "\t\tQueue priority hint: %d\n",
			   READ_ONCE(sched_engine->queue_priority_hint));

	last = NULL;
	count = 0;
	for (rb = rb_first_cached(&sched_engine->queue); rb; rb = rb_next(rb)) {
		struct i915_priolist *p = rb_entry(rb, typeof(*p), node);

		priolist_for_each_request(rq, p) {
			if (count++ < max - 1)
				show_request(m, rq, "\t\t", 0);
			else
				last = rq;
		}
	}
	if (last) {
		if (count > max) {
			drm_printf(m,
				   "\t\t...skipping %d queued requests...\n",
				   count - max);
		}
		show_request(m, last, "\t\t", 0);
	}

	last = NULL;
	count = 0;
	for (rb = rb_first_cached(&execlists->virtual); rb; rb = rb_next(rb)) {
		struct virtual_engine *ve =
			rb_entry(rb, typeof(*ve), nodes[engine->id].rb);
		struct i915_request *rq = READ_ONCE(ve->request);

		if (rq) {
			if (count++ < max - 1)
				show_request(m, rq, "\t\t", 0);
			else
				last = rq;
		}
	}
	if (last) {
		if (count > max) {
			drm_printf(m,
				   "\t\t...skipping %d virtual requests...\n",
				   count - max);
		}
		show_request(m, last, "\t\t", 0);
	}

	spin_unlock_irqrestore(&sched_engine->lock, flags);
}

void intel_execlists_dump_active_requests(struct intel_engine_cs *engine,
					  struct i915_request *hung_rq,
					  struct drm_printer *m)
{
	unsigned long flags;

	spin_lock_irqsave(&engine->sched_engine->lock, flags);

	intel_engine_dump_active_requests(&engine->sched_engine->requests, hung_rq, m);

	drm_printf(m, "\tOn hold?: %zu\n",
		   list_count_nodes(&engine->sched_engine->hold));

	spin_unlock_irqrestore(&engine->sched_engine->lock, flags);
}

#if IS_ENABLED(CONFIG_DRM_I915_SELFTEST)
#include "selftest_execlists.c"
#endif
