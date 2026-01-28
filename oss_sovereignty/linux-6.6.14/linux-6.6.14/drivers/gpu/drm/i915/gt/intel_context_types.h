#ifndef __INTEL_CONTEXT_TYPES__
#define __INTEL_CONTEXT_TYPES__
#include <linux/average.h>
#include <linux/kref.h>
#include <linux/list.h>
#include <linux/mutex.h>
#include <linux/types.h>
#include "i915_active_types.h"
#include "i915_sw_fence.h"
#include "i915_utils.h"
#include "intel_engine_types.h"
#include "intel_sseu.h"
#include "uc/intel_guc_fwif.h"
#define CONTEXT_REDZONE POISON_INUSE
DECLARE_EWMA(runtime, 3, 8);
struct i915_gem_context;
struct i915_gem_ww_ctx;
struct i915_vma;
struct intel_breadcrumbs;
struct intel_context;
struct intel_ring;
struct intel_context_ops {
	unsigned long flags;
#define COPS_HAS_INFLIGHT_BIT 0
#define COPS_HAS_INFLIGHT BIT(COPS_HAS_INFLIGHT_BIT)
#define COPS_RUNTIME_CYCLES_BIT 1
#define COPS_RUNTIME_CYCLES BIT(COPS_RUNTIME_CYCLES_BIT)
	int (*alloc)(struct intel_context *ce);
	void (*revoke)(struct intel_context *ce, struct i915_request *rq,
		       unsigned int preempt_timeout_ms);
	void (*close)(struct intel_context *ce);
	int (*pre_pin)(struct intel_context *ce, struct i915_gem_ww_ctx *ww, void **vaddr);
	int (*pin)(struct intel_context *ce, void *vaddr);
	void (*unpin)(struct intel_context *ce);
	void (*post_unpin)(struct intel_context *ce);
	void (*cancel_request)(struct intel_context *ce,
			       struct i915_request *rq);
	void (*enter)(struct intel_context *ce);
	void (*exit)(struct intel_context *ce);
	void (*sched_disable)(struct intel_context *ce);
	void (*update_stats)(struct intel_context *ce);
	void (*reset)(struct intel_context *ce);
	void (*destroy)(struct kref *kref);
	struct intel_context *(*create_virtual)(struct intel_engine_cs **engine,
						unsigned int count,
						unsigned long flags);
	struct intel_context *(*create_parallel)(struct intel_engine_cs **engines,
						 unsigned int num_siblings,
						 unsigned int width);
	struct intel_engine_cs *(*get_sibling)(struct intel_engine_cs *engine,
					       unsigned int sibling);
};
struct intel_context {
	union {
		struct kref ref;  
		struct rcu_head rcu;
	};
	struct intel_engine_cs *engine;
	struct intel_engine_cs *inflight;
#define __intel_context_inflight(engine) ptr_mask_bits(engine, 3)
#define __intel_context_inflight_count(engine) ptr_unmask_bits(engine, 3)
#define intel_context_inflight(ce) \
	__intel_context_inflight(READ_ONCE((ce)->inflight))
#define intel_context_inflight_count(ce) \
	__intel_context_inflight_count(READ_ONCE((ce)->inflight))
	struct i915_address_space *vm;
	struct i915_gem_context __rcu *gem_context;
	struct list_head signal_link;  
	struct list_head signals;  
	spinlock_t signal_lock;  
	struct i915_vma *state;
	u32 ring_size;
	struct intel_ring *ring;
	struct intel_timeline *timeline;
	unsigned long flags;
#define CONTEXT_BARRIER_BIT		0
#define CONTEXT_ALLOC_BIT		1
#define CONTEXT_INIT_BIT		2
#define CONTEXT_VALID_BIT		3
#define CONTEXT_CLOSED_BIT		4
#define CONTEXT_USE_SEMAPHORES		5
#define CONTEXT_BANNED			6
#define CONTEXT_FORCE_SINGLE_SUBMISSION	7
#define CONTEXT_NOPREEMPT		8
#define CONTEXT_LRCA_DIRTY		9
#define CONTEXT_GUC_INIT		10
#define CONTEXT_PERMA_PIN		11
#define CONTEXT_IS_PARKING		12
#define CONTEXT_EXITING			13
	struct {
		u64 timeout_us;
	} watchdog;
	u32 *lrc_reg_state;
	union {
		struct {
			u32 lrca;
			u32 ccid;
		};
		u64 desc;
	} lrc;
	u32 tag;  
	struct intel_context_stats {
		u64 active;
		struct {
			struct ewma_runtime avg;
			u64 total;
			u32 last;
			I915_SELFTEST_DECLARE(u32 num_underflow);
			I915_SELFTEST_DECLARE(u32 max_underflow);
		} runtime;
	} stats;
	unsigned int active_count;  
	atomic_t pin_count;
	struct mutex pin_mutex;  
	struct i915_active active;
	const struct intel_context_ops *ops;
	struct intel_sseu sseu;
	struct list_head pinned_contexts_link;
	u8 wa_bb_page;  
	struct {
		spinlock_t lock;
		u32 sched_state;
		struct list_head fences;
		struct i915_sw_fence blocked;
		struct list_head requests;
		u8 prio;
		u32 prio_count[GUC_CLIENT_PRIORITY_NUM];
		struct delayed_work sched_disable_delay_work;
	} guc_state;
	struct {
		u16 id;
		atomic_t ref;
		struct list_head link;
	} guc_id;
	struct list_head destroyed_link;
	struct {
		union {
			struct list_head child_list;
			struct list_head child_link;
		};
		struct intel_context *parent;
		struct i915_request *last_rq;
		u64 fence_context;
		u32 seqno;
		u8 number_children;
		u8 child_index;
		struct {
			u16 wqi_head;
			u16 wqi_tail;
			u32 *wq_head;
			u32 *wq_tail;
			u32 *wq_status;
			u8 parent_page;
		} guc;
	} parallel;
#ifdef CONFIG_DRM_I915_SELFTEST
	bool drop_schedule_enable;
	bool drop_schedule_disable;
	bool drop_deregister;
#endif
};
#endif  
