 
 

#ifndef __INTEL_GT_TYPES__
#define __INTEL_GT_TYPES__

#include <linux/ktime.h>
#include <linux/list.h>
#include <linux/llist.h>
#include <linux/mutex.h>
#include <linux/notifier.h>
#include <linux/seqlock.h>
#include <linux/spinlock.h>
#include <linux/types.h>
#include <linux/workqueue.h>

#include "uc/intel_uc.h"
#include "intel_gsc.h"

#include "i915_vma.h"
#include "i915_perf_types.h"
#include "intel_engine_types.h"
#include "intel_gt_buffer_pool_types.h"
#include "intel_hwconfig.h"
#include "intel_llc_types.h"
#include "intel_reset_types.h"
#include "intel_rc6_types.h"
#include "intel_rps_types.h"
#include "intel_migrate_types.h"
#include "intel_wakeref.h"
#include "intel_wopcm.h"

struct drm_i915_private;
struct i915_ggtt;
struct intel_engine_cs;
struct intel_uncore;

struct intel_mmio_range {
	u32 start;
	u32 end;
};

 
enum intel_steering_type {
	L3BANK,
	MSLICE,
	LNCF,
	GAM,
	DSS,
	OADDRM,

	 
	INSTANCE0,

	NUM_STEERING_TYPES
};

enum intel_submission_method {
	INTEL_SUBMISSION_RING,
	INTEL_SUBMISSION_ELSP,
	INTEL_SUBMISSION_GUC,
};

struct gt_defaults {
	u32 min_freq;
	u32 max_freq;

	u8 rps_up_threshold;
	u8 rps_down_threshold;
};

enum intel_gt_type {
	GT_PRIMARY,
	GT_TILE,
	GT_MEDIA,
};

struct intel_gt {
	struct drm_i915_private *i915;
	const char *name;
	enum intel_gt_type type;

	struct intel_uncore *uncore;
	struct i915_ggtt *ggtt;

	struct intel_uc uc;
	struct intel_gsc gsc;
	struct intel_wopcm wopcm;

	struct {
		 
		struct mutex invalidate_lock;

		 
		seqcount_mutex_t seqno;
	} tlb;

	struct i915_wa_list wa_list;

	struct intel_gt_timelines {
		spinlock_t lock;  
		struct list_head active_list;
	} timelines;

	struct intel_gt_requests {
		 
		struct delayed_work retire_work;
	} requests;

	struct {
		struct llist_head list;
		struct work_struct work;
	} watchdog;

	struct intel_wakeref wakeref;
	atomic_t user_wakeref;

	struct list_head closed_vma;
	spinlock_t closed_lock;  

	ktime_t last_init_time;
	struct intel_reset reset;

	 
	intel_wakeref_t awake;

	u32 clock_frequency;
	u32 clock_period_ns;

	struct intel_llc llc;
	struct intel_rc6 rc6;
	struct intel_rps rps;

	spinlock_t *irq_lock;
	u32 gt_imr;
	u32 pm_ier;
	u32 pm_imr;

	u32 pm_guc_events;

	struct {
		bool active;

		 
		seqcount_mutex_t lock;

		 
		ktime_t total;

		 
		ktime_t start;
	} stats;

	struct intel_engine_cs *engine[I915_NUM_ENGINES];
	struct intel_engine_cs *engine_class[MAX_ENGINE_CLASS + 1]
					    [MAX_ENGINE_INSTANCE + 1];
	enum intel_submission_method submission_method;

	 
	struct i915_address_space *vm;

	 
	struct intel_gt_buffer_pool buffer_pool;

	struct i915_vma *scratch;

	struct intel_migrate migrate;

	const struct intel_mmio_range *steering_table[NUM_STEERING_TYPES];

	struct {
		u8 groupid;
		u8 instanceid;
	} default_steering;

	 
	spinlock_t mcr_lock;

	 
	phys_addr_t phys_addr;

	struct intel_gt_info {
		unsigned int id;

		intel_engine_mask_t engine_mask;

		u32 l3bank_mask;

		u8 num_engines;

		 
		u8 sfc_mask;

		 
		u8 vdbox_sfc_access;

		 
		struct sseu_dev_info sseu;

		unsigned long mslice_mask;

		 
		struct intel_hwconfig hwconfig;
	} info;

	struct {
		u8 uc_index;
		u8 wb_index;  
	} mocs;

	 
	struct kobject sysfs_gt;

	 
	struct gt_defaults defaults;
	struct kobject *sysfs_defaults;

	struct i915_perf_gt perf;

	 
	struct list_head ggtt_link;
};

struct intel_gt_definition {
	enum intel_gt_type type;
	char *name;
	u32 mapping_base;
	u32 gsi_offset;
	intel_engine_mask_t engine_mask;
};

enum intel_gt_scratch_field {
	 
	INTEL_GT_SCRATCH_FIELD_DEFAULT = 0,

	 
	INTEL_GT_SCRATCH_FIELD_RENDER_FLUSH = 128,

	 
	INTEL_GT_SCRATCH_FIELD_COHERENTL3_WA = 256,
};

#define intel_gt_support_legacy_fencing(gt) ((gt)->ggtt->num_fences > 0)

#endif  
