 
 

#ifndef __INTEL_ENGINE_TYPES__
#define __INTEL_ENGINE_TYPES__

#include <linux/average.h>
#include <linux/hashtable.h>
#include <linux/irq_work.h>
#include <linux/kref.h>
#include <linux/list.h>
#include <linux/llist.h>
#include <linux/rbtree.h>
#include <linux/timer.h>
#include <linux/types.h>
#include <linux/workqueue.h>

#include "i915_gem.h"
#include "i915_pmu.h"
#include "i915_priolist_types.h"
#include "i915_selftest.h"
#include "intel_sseu.h"
#include "intel_timeline_types.h"
#include "intel_uncore.h"
#include "intel_wakeref.h"
#include "intel_workarounds_types.h"

 
#define RENDER_CLASS		0
#define VIDEO_DECODE_CLASS	1
#define VIDEO_ENHANCEMENT_CLASS	2
#define COPY_ENGINE_CLASS	3
#define OTHER_CLASS		4
#define COMPUTE_CLASS		5
#define MAX_ENGINE_CLASS	5
#define MAX_ENGINE_INSTANCE	8

#define I915_MAX_SLICES	3
#define I915_MAX_SUBSLICES 8

#define I915_CMD_HASH_ORDER 9

struct dma_fence;
struct drm_i915_gem_object;
struct drm_i915_reg_table;
struct i915_gem_context;
struct i915_request;
struct i915_sched_attr;
struct i915_sched_engine;
struct intel_gt;
struct intel_ring;
struct intel_uncore;
struct intel_breadcrumbs;
struct intel_engine_cs;
struct i915_perf_group;

typedef u32 intel_engine_mask_t;
#define ALL_ENGINES ((intel_engine_mask_t)~0ul)
#define VIRTUAL_ENGINES BIT(BITS_PER_TYPE(intel_engine_mask_t) - 1)

struct intel_hw_status_page {
	struct list_head timelines;
	struct i915_vma *vma;
	u32 *addr;
};

struct intel_instdone {
	u32 instdone;
	 
	u32 slice_common;
	u32 slice_common_extra[2];
	u32 sampler[GEN_MAX_GSLICES][I915_MAX_SUBSLICES];
	u32 row[GEN_MAX_GSLICES][I915_MAX_SUBSLICES];

	 
	u32 geom_svg[GEN_MAX_GSLICES][I915_MAX_SUBSLICES];
};

 
struct i915_ctx_workarounds {
	struct i915_wa_ctx_bb {
		u32 offset;
		u32 size;
	} indirect_ctx, per_ctx;
	struct i915_vma *vma;
};

#define I915_MAX_VCS	8
#define I915_MAX_VECS	4
#define I915_MAX_SFC	(I915_MAX_VCS / 2)
#define I915_MAX_CCS	4
#define I915_MAX_RCS	1
#define I915_MAX_BCS	9

 
enum intel_engine_id {
	RCS0 = 0,
	BCS0,
	BCS1,
	BCS2,
	BCS3,
	BCS4,
	BCS5,
	BCS6,
	BCS7,
	BCS8,
#define _BCS(n) (BCS0 + (n))
	VCS0,
	VCS1,
	VCS2,
	VCS3,
	VCS4,
	VCS5,
	VCS6,
	VCS7,
#define _VCS(n) (VCS0 + (n))
	VECS0,
	VECS1,
	VECS2,
	VECS3,
#define _VECS(n) (VECS0 + (n))
	CCS0,
	CCS1,
	CCS2,
	CCS3,
#define _CCS(n) (CCS0 + (n))
	GSC0,
	I915_NUM_ENGINES
#define INVALID_ENGINE ((enum intel_engine_id)-1)
};

 
DECLARE_EWMA(_engine_latency, 6, 4)

struct st_preempt_hang {
	struct completion completion;
	unsigned int count;
};

 
struct intel_engine_execlists {
	 
	struct timer_list timer;

	 
	struct timer_list preempt;

	 
	const struct i915_request *preempt_target;

	 
	u32 ccid;

	 
	u32 yield;

	 
	u32 error_interrupt;
#define ERROR_CSB	BIT(31)
#define ERROR_PREEMPT	BIT(30)

	 
	u32 reset_ccid;

	 
	u32 __iomem *submit_reg;

	 
	u32 __iomem *ctrl_reg;

#define EXECLIST_MAX_PORTS 2
	 
	struct i915_request * const *active;
	 
	struct i915_request *inflight[EXECLIST_MAX_PORTS + 1  ];
	 
	struct i915_request *pending[EXECLIST_MAX_PORTS + 1];

	 
	unsigned int port_mask;

	 
	struct rb_root_cached virtual;

	 
	u32 *csb_write;

	 
	u64 *csb_status;

	 
	u8 csb_size;

	 
	u8 csb_head;

	 
	I915_SELFTEST_DECLARE(struct st_preempt_hang preempt_hang;)
};

#define INTEL_ENGINE_CS_MAX_NAME 8

struct intel_engine_execlists_stats {
	 
	unsigned int active;

	 
	seqcount_t lock;

	 
	ktime_t total;

	 
	ktime_t start;
};

struct intel_engine_guc_stats {
	 
	bool running;

	 
	u32 prev_total;

	 
	u64 total_gt_clks;

	 
	u64 start_gt_clk;
};

union intel_engine_tlb_inv_reg {
	i915_reg_t	reg;
	i915_mcr_reg_t	mcr_reg;
};

struct intel_engine_tlb_inv {
	bool mcr;
	union intel_engine_tlb_inv_reg reg;
	u32 request;
	u32 done;
};

struct intel_engine_cs {
	struct drm_i915_private *i915;
	struct intel_gt *gt;
	struct intel_uncore *uncore;
	char name[INTEL_ENGINE_CS_MAX_NAME];

	enum intel_engine_id id;
	enum intel_engine_id legacy_idx;

	unsigned int guc_id;

	intel_engine_mask_t mask;
	u32 reset_domain;
	 
	intel_engine_mask_t logical_mask;

	u8 class;
	u8 instance;

	u16 uabi_class;
	u16 uabi_instance;

	u32 uabi_capabilities;
	u32 context_size;
	u32 mmio_base;

	struct intel_engine_tlb_inv tlb_inv;

	 
	enum forcewake_domains fw_domain;
	unsigned int fw_active;

	unsigned long context_tag;

	struct rb_node uabi_node;

	struct intel_sseu sseu;

	struct i915_sched_engine *sched_engine;

	 
	struct i915_request *request_pool;

	struct intel_context *hung_ce;

	struct llist_head barrier_tasks;

	struct intel_context *kernel_context;  

	 
	struct list_head pinned_contexts_list;

	intel_engine_mask_t saturated;  

	struct {
		struct delayed_work work;
		struct i915_request *systole;
		unsigned long blocked;
	} heartbeat;

	unsigned long serial;

	unsigned long wakeref_serial;
	struct intel_wakeref wakeref;
	struct file *default_state;

	struct {
		struct intel_ring *ring;
		struct intel_timeline *timeline;
	} legacy;

	 
	struct ewma__engine_latency latency;

	 
	struct intel_breadcrumbs *breadcrumbs;

	struct intel_engine_pmu {
		 
		u32 enable;
		 
		unsigned int enable_count[I915_ENGINE_SAMPLE_COUNT];
		 
		struct i915_pmu_sample sample[I915_ENGINE_SAMPLE_COUNT];
	} pmu;

	struct intel_hw_status_page status_page;
	struct i915_ctx_workarounds wa_ctx;
	struct i915_wa_list ctx_wa_list;
	struct i915_wa_list wa_list;
	struct i915_wa_list whitelist;

	u32             irq_keep_mask;  
	u32		irq_enable_mask;  
	void		(*irq_enable)(struct intel_engine_cs *engine);
	void		(*irq_disable)(struct intel_engine_cs *engine);
	void		(*irq_handler)(struct intel_engine_cs *engine, u16 iir);

	void		(*sanitize)(struct intel_engine_cs *engine);
	int		(*resume)(struct intel_engine_cs *engine);

	struct {
		void (*prepare)(struct intel_engine_cs *engine);

		void (*rewind)(struct intel_engine_cs *engine, bool stalled);
		void (*cancel)(struct intel_engine_cs *engine);

		void (*finish)(struct intel_engine_cs *engine);
	} reset;

	void		(*park)(struct intel_engine_cs *engine);
	void		(*unpark)(struct intel_engine_cs *engine);

	void		(*bump_serial)(struct intel_engine_cs *engine);

	void		(*set_default_submission)(struct intel_engine_cs *engine);

	const struct intel_context_ops *cops;

	int		(*request_alloc)(struct i915_request *rq);

	int		(*emit_flush)(struct i915_request *request, u32 mode);
#define EMIT_INVALIDATE	BIT(0)
#define EMIT_FLUSH	BIT(1)
#define EMIT_BARRIER	(EMIT_INVALIDATE | EMIT_FLUSH)
	int		(*emit_bb_start)(struct i915_request *rq,
					 u64 offset, u32 length,
					 unsigned int dispatch_flags);
#define I915_DISPATCH_SECURE BIT(0)
#define I915_DISPATCH_PINNED BIT(1)
	int		 (*emit_init_breadcrumb)(struct i915_request *rq);
	u32		*(*emit_fini_breadcrumb)(struct i915_request *rq,
						 u32 *cs);
	unsigned int	emit_fini_breadcrumb_dw;

	 
	void		(*submit_request)(struct i915_request *rq);

	void		(*release)(struct intel_engine_cs *engine);

	 
	void		(*add_active_request)(struct i915_request *rq);
	void		(*remove_active_request)(struct i915_request *rq);

	 
	ktime_t		(*busyness)(struct intel_engine_cs *engine,
				    ktime_t *now);

	struct intel_engine_execlists execlists;

	 
	struct intel_timeline *retire;
	struct work_struct retire_work;

	 
	struct atomic_notifier_head context_status_notifier;

#define I915_ENGINE_USING_CMD_PARSER BIT(0)
#define I915_ENGINE_SUPPORTS_STATS   BIT(1)
#define I915_ENGINE_HAS_PREEMPTION   BIT(2)
#define I915_ENGINE_HAS_SEMAPHORES   BIT(3)
#define I915_ENGINE_HAS_TIMESLICES   BIT(4)
#define I915_ENGINE_IS_VIRTUAL       BIT(5)
#define I915_ENGINE_HAS_RELATIVE_MMIO BIT(6)
#define I915_ENGINE_REQUIRES_CMD_PARSER BIT(7)
#define I915_ENGINE_WANT_FORCED_PREEMPTION BIT(8)
#define I915_ENGINE_HAS_RCS_REG_STATE  BIT(9)
#define I915_ENGINE_HAS_EU_PRIORITY    BIT(10)
#define I915_ENGINE_FIRST_RENDER_COMPUTE BIT(11)
#define I915_ENGINE_USES_WA_HOLD_CCS_SWITCHOUT BIT(12)
	unsigned int flags;

	 
	DECLARE_HASHTABLE(cmd_hash, I915_CMD_HASH_ORDER);

	 
	const struct drm_i915_reg_table *reg_tables;
	int reg_table_count;

	 
	u32 (*get_cmd_length_mask)(u32 cmd_header);

	struct {
		union {
			struct intel_engine_execlists_stats execlists;
			struct intel_engine_guc_stats guc;
		};

		 
		ktime_t rps;
	} stats;

	struct {
		unsigned long heartbeat_interval_ms;
		unsigned long max_busywait_duration_ns;
		unsigned long preempt_timeout_ms;
		unsigned long stop_timeout_ms;
		unsigned long timeslice_duration_ms;
	} props, defaults;

	I915_SELFTEST_DECLARE(struct fault_attr reset_timeout);

	 
	struct i915_perf_group *oa_group;
};

static inline bool
intel_engine_using_cmd_parser(const struct intel_engine_cs *engine)
{
	return engine->flags & I915_ENGINE_USING_CMD_PARSER;
}

static inline bool
intel_engine_requires_cmd_parser(const struct intel_engine_cs *engine)
{
	return engine->flags & I915_ENGINE_REQUIRES_CMD_PARSER;
}

static inline bool
intel_engine_supports_stats(const struct intel_engine_cs *engine)
{
	return engine->flags & I915_ENGINE_SUPPORTS_STATS;
}

static inline bool
intel_engine_has_preemption(const struct intel_engine_cs *engine)
{
	return engine->flags & I915_ENGINE_HAS_PREEMPTION;
}

static inline bool
intel_engine_has_semaphores(const struct intel_engine_cs *engine)
{
	return engine->flags & I915_ENGINE_HAS_SEMAPHORES;
}

static inline bool
intel_engine_has_timeslices(const struct intel_engine_cs *engine)
{
	if (!CONFIG_DRM_I915_TIMESLICE_DURATION)
		return false;

	return engine->flags & I915_ENGINE_HAS_TIMESLICES;
}

static inline bool
intel_engine_is_virtual(const struct intel_engine_cs *engine)
{
	return engine->flags & I915_ENGINE_IS_VIRTUAL;
}

static inline bool
intel_engine_has_relative_mmio(const struct intel_engine_cs * const engine)
{
	return engine->flags & I915_ENGINE_HAS_RELATIVE_MMIO;
}

 
static inline bool
intel_engine_uses_wa_hold_ccs_switchout(struct intel_engine_cs *engine)
{
	return engine->flags & I915_ENGINE_USES_WA_HOLD_CCS_SWITCHOUT;
}

#endif  
