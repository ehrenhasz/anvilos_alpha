#ifndef I915_REQUEST_H
#define I915_REQUEST_H
#include <linux/dma-fence.h>
#include <linux/hrtimer.h>
#include <linux/irq_work.h>
#include <linux/llist.h>
#include <linux/lockdep.h>
#include "gem/i915_gem_context_types.h"
#include "gt/intel_context_types.h"
#include "gt/intel_engine_types.h"
#include "gt/intel_timeline_types.h"
#include "i915_gem.h"
#include "i915_scheduler.h"
#include "i915_selftest.h"
#include "i915_sw_fence.h"
#include "i915_vma_resource.h"
#include <uapi/drm/i915_drm.h>
struct drm_file;
struct drm_i915_gem_object;
struct drm_printer;
struct i915_deps;
struct i915_request;
#if IS_ENABLED(CONFIG_DRM_I915_CAPTURE_ERROR)
struct i915_capture_list {
	struct i915_vma_resource *vma_res;
	struct i915_capture_list *next;
};
void i915_request_free_capture_list(struct i915_capture_list *capture);
#else
#define i915_request_free_capture_list(_a) do {} while (0)
#endif
#define RQ_TRACE(rq, fmt, ...) do {					\
	const struct i915_request *rq__ = (rq);				\
	ENGINE_TRACE(rq__->engine, "fence %llx:%lld, current %d " fmt,	\
		     rq__->fence.context, rq__->fence.seqno,		\
		     hwsp_seqno(rq__), ##__VA_ARGS__);			\
} while (0)
enum {
	I915_FENCE_FLAG_ACTIVE = DMA_FENCE_FLAG_USER_BITS,
	I915_FENCE_FLAG_PQUEUE,
	I915_FENCE_FLAG_HOLD,
	I915_FENCE_FLAG_INITIAL_BREADCRUMB,
	I915_FENCE_FLAG_SIGNAL,
	I915_FENCE_FLAG_NOPREEMPT,
	I915_FENCE_FLAG_SENTINEL,
	I915_FENCE_FLAG_BOOST,
	I915_FENCE_FLAG_SUBMIT_PARALLEL,
	I915_FENCE_FLAG_SKIP_PARALLEL,
	I915_FENCE_FLAG_COMPOSITE,
};
struct i915_request {
	struct dma_fence fence;
	spinlock_t lock;
	struct drm_i915_private *i915;
	struct intel_engine_cs *engine;
	struct intel_context *context;
	struct intel_ring *ring;
	struct intel_timeline __rcu *timeline;
	struct list_head signal_link;
	struct llist_node signal_node;
	unsigned long rcustate;
	struct pin_cookie cookie;
	struct i915_sw_fence submit;
	union {
		wait_queue_entry_t submitq;
		struct i915_sw_dma_fence_cb dmaq;
		struct i915_request_duration_cb {
			struct dma_fence_cb cb;
			ktime_t emitted;
		} duration;
	};
	struct llist_head execute_cb;
	struct i915_sw_fence semaphore;
	struct irq_work submit_work;
	struct i915_sched_node sched;
	struct i915_dependency dep;
	intel_engine_mask_t execution_mask;
	const u32 *hwsp_seqno;
	u32 head;
	u32 infix;
	u32 postfix;
	u32 tail;
	u32 wa_tail;
	u32 reserved_space;
	I915_SELFTEST_DECLARE(struct i915_vma *batch);
	struct i915_vma_resource *batch_res;
#if IS_ENABLED(CONFIG_DRM_I915_CAPTURE_ERROR)
	struct i915_capture_list *capture_list;
#endif
	unsigned long emitted_jiffies;
	struct list_head link;
	struct i915_request_watchdog {
		struct llist_node link;
		struct hrtimer timer;
	} watchdog;
	struct list_head guc_fence_link;
#define	GUC_PRIO_INIT	0xff
#define	GUC_PRIO_FINI	0xfe
	u8 guc_prio;
	wait_queue_entry_t hucq;
	I915_SELFTEST_DECLARE(struct {
		struct list_head link;
		unsigned long delay;
	} mock;)
};
#define I915_FENCE_GFP (GFP_KERNEL | __GFP_RETRY_MAYFAIL | __GFP_NOWARN)
extern const struct dma_fence_ops i915_fence_ops;
static inline bool dma_fence_is_i915(const struct dma_fence *fence)
{
	return fence->ops == &i915_fence_ops;
}
struct kmem_cache *i915_request_slab_cache(void);
struct i915_request * __must_check
__i915_request_create(struct intel_context *ce, gfp_t gfp);
struct i915_request * __must_check
i915_request_create(struct intel_context *ce);
void __i915_request_skip(struct i915_request *rq);
bool i915_request_set_error_once(struct i915_request *rq, int error);
struct i915_request *i915_request_mark_eio(struct i915_request *rq);
struct i915_request *__i915_request_commit(struct i915_request *request);
void __i915_request_queue(struct i915_request *rq,
			  const struct i915_sched_attr *attr);
void __i915_request_queue_bh(struct i915_request *rq);
bool i915_request_retire(struct i915_request *rq);
void i915_request_retire_upto(struct i915_request *rq);
static inline struct i915_request *
to_request(struct dma_fence *fence)
{
	BUILD_BUG_ON(offsetof(struct i915_request, fence) != 0);
	GEM_BUG_ON(fence && !dma_fence_is_i915(fence));
	return container_of(fence, struct i915_request, fence);
}
static inline struct i915_request *
i915_request_get(struct i915_request *rq)
{
	return to_request(dma_fence_get(&rq->fence));
}
static inline struct i915_request *
i915_request_get_rcu(struct i915_request *rq)
{
	return to_request(dma_fence_get_rcu(&rq->fence));
}
static inline void
i915_request_put(struct i915_request *rq)
{
	dma_fence_put(&rq->fence);
}
int i915_request_await_object(struct i915_request *to,
			      struct drm_i915_gem_object *obj,
			      bool write);
int i915_request_await_dma_fence(struct i915_request *rq,
				 struct dma_fence *fence);
int i915_request_await_deps(struct i915_request *rq, const struct i915_deps *deps);
int i915_request_await_execution(struct i915_request *rq,
				 struct dma_fence *fence);
void i915_request_add(struct i915_request *rq);
bool __i915_request_submit(struct i915_request *request);
void i915_request_submit(struct i915_request *request);
void __i915_request_unsubmit(struct i915_request *request);
void i915_request_unsubmit(struct i915_request *request);
void i915_request_cancel(struct i915_request *rq, int error);
long i915_request_wait_timeout(struct i915_request *rq,
			       unsigned int flags,
			       long timeout)
	__attribute__((nonnull(1)));
long i915_request_wait(struct i915_request *rq,
		       unsigned int flags,
		       long timeout)
	__attribute__((nonnull(1)));
#define I915_WAIT_INTERRUPTIBLE	BIT(0)
#define I915_WAIT_PRIORITY	BIT(1)  
#define I915_WAIT_ALL		BIT(2)  
void i915_request_show(struct drm_printer *m,
		       const struct i915_request *rq,
		       const char *prefix,
		       int indent);
static inline bool i915_request_signaled(const struct i915_request *rq)
{
	return test_bit(DMA_FENCE_FLAG_SIGNALED_BIT, &rq->fence.flags);
}
static inline bool i915_request_is_active(const struct i915_request *rq)
{
	return test_bit(I915_FENCE_FLAG_ACTIVE, &rq->fence.flags);
}
static inline bool i915_request_in_priority_queue(const struct i915_request *rq)
{
	return test_bit(I915_FENCE_FLAG_PQUEUE, &rq->fence.flags);
}
static inline bool
i915_request_has_initial_breadcrumb(const struct i915_request *rq)
{
	return test_bit(I915_FENCE_FLAG_INITIAL_BREADCRUMB, &rq->fence.flags);
}
static inline bool i915_seqno_passed(u32 seq1, u32 seq2)
{
	return (s32)(seq1 - seq2) >= 0;
}
static inline u32 __hwsp_seqno(const struct i915_request *rq)
{
	const u32 *hwsp = READ_ONCE(rq->hwsp_seqno);
	return READ_ONCE(*hwsp);
}
static inline u32 hwsp_seqno(const struct i915_request *rq)
{
	u32 seqno;
	rcu_read_lock();  
	seqno = __hwsp_seqno(rq);
	rcu_read_unlock();
	return seqno;
}
static inline bool __i915_request_has_started(const struct i915_request *rq)
{
	return i915_seqno_passed(__hwsp_seqno(rq), rq->fence.seqno - 1);
}
static inline bool i915_request_started(const struct i915_request *rq)
{
	bool result;
	if (i915_request_signaled(rq))
		return true;
	result = true;
	rcu_read_lock();  
	if (likely(!i915_request_signaled(rq)))
		result = __i915_request_has_started(rq);
	rcu_read_unlock();
	return result;
}
static inline bool i915_request_is_running(const struct i915_request *rq)
{
	bool result;
	if (!i915_request_is_active(rq))
		return false;
	rcu_read_lock();
	result = __i915_request_has_started(rq) && i915_request_is_active(rq);
	rcu_read_unlock();
	return result;
}
static inline bool i915_request_is_ready(const struct i915_request *rq)
{
	return !list_empty(&rq->sched.link);
}
static inline bool __i915_request_is_complete(const struct i915_request *rq)
{
	return i915_seqno_passed(__hwsp_seqno(rq), rq->fence.seqno);
}
static inline bool i915_request_completed(const struct i915_request *rq)
{
	bool result;
	if (i915_request_signaled(rq))
		return true;
	result = true;
	rcu_read_lock();  
	if (likely(!i915_request_signaled(rq)))
		result = __i915_request_is_complete(rq);
	rcu_read_unlock();
	return result;
}
static inline void i915_request_mark_complete(struct i915_request *rq)
{
	WRITE_ONCE(rq->hwsp_seqno,  
		   (u32 *)&rq->fence.seqno);
}
static inline bool i915_request_has_waitboost(const struct i915_request *rq)
{
	return test_bit(I915_FENCE_FLAG_BOOST, &rq->fence.flags);
}
static inline bool i915_request_has_nopreempt(const struct i915_request *rq)
{
	return unlikely(test_bit(I915_FENCE_FLAG_NOPREEMPT, &rq->fence.flags));
}
static inline bool i915_request_has_sentinel(const struct i915_request *rq)
{
	return unlikely(test_bit(I915_FENCE_FLAG_SENTINEL, &rq->fence.flags));
}
static inline bool i915_request_on_hold(const struct i915_request *rq)
{
	return unlikely(test_bit(I915_FENCE_FLAG_HOLD, &rq->fence.flags));
}
static inline void i915_request_set_hold(struct i915_request *rq)
{
	set_bit(I915_FENCE_FLAG_HOLD, &rq->fence.flags);
}
static inline void i915_request_clear_hold(struct i915_request *rq)
{
	clear_bit(I915_FENCE_FLAG_HOLD, &rq->fence.flags);
}
static inline struct intel_timeline *
i915_request_timeline(const struct i915_request *rq)
{
	return rcu_dereference_protected(rq->timeline,
					 lockdep_is_held(&rcu_access_pointer(rq->timeline)->mutex) ||
					 test_bit(CONTEXT_IS_PARKING, &rq->context->flags));
}
static inline struct i915_gem_context *
i915_request_gem_context(const struct i915_request *rq)
{
	return rcu_dereference_protected(rq->context->gem_context, true);
}
static inline struct intel_timeline *
i915_request_active_timeline(const struct i915_request *rq)
{
	return rcu_dereference_protected(rq->timeline,
					 lockdep_is_held(&rq->engine->sched_engine->lock));
}
static inline u32
i915_request_active_seqno(const struct i915_request *rq)
{
	u32 hwsp_phys_base =
		page_mask_bits(i915_request_active_timeline(rq)->hwsp_offset);
	u32 hwsp_relative_offset = offset_in_page(rq->hwsp_seqno);
	return hwsp_phys_base + hwsp_relative_offset;
}
bool
i915_request_active_engine(struct i915_request *rq,
			   struct intel_engine_cs **active);
void i915_request_notify_execute_cb_imm(struct i915_request *rq);
enum i915_request_state {
	I915_REQUEST_UNKNOWN = 0,
	I915_REQUEST_COMPLETE,
	I915_REQUEST_PENDING,
	I915_REQUEST_QUEUED,
	I915_REQUEST_ACTIVE,
};
enum i915_request_state i915_test_request_state(struct i915_request *rq);
void i915_request_module_exit(void);
int i915_request_module_init(void);
#endif  
