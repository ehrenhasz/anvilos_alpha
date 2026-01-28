#ifndef _I915_PERF_TYPES_H_
#define _I915_PERF_TYPES_H_
#include <linux/atomic.h>
#include <linux/device.h>
#include <linux/hrtimer.h>
#include <linux/llist.h>
#include <linux/poll.h>
#include <linux/sysfs.h>
#include <linux/types.h>
#include <linux/uuid.h>
#include <linux/wait.h>
#include <uapi/drm/i915_drm.h>
#include "gt/intel_engine_types.h"
#include "gt/intel_sseu.h"
#include "i915_reg_defs.h"
#include "intel_uncore.h"
#include "intel_wakeref.h"
struct drm_i915_private;
struct file;
struct i915_active;
struct i915_gem_context;
struct i915_perf;
struct i915_vma;
struct intel_context;
struct intel_engine_cs;
enum {
	PERF_GROUP_OAG = 0,
	PERF_GROUP_OAM_SAMEDIA_0 = 0,
	PERF_GROUP_MAX,
	PERF_GROUP_INVALID = U32_MAX,
};
enum report_header {
	HDR_32_BIT = 0,
	HDR_64_BIT,
};
struct i915_perf_regs {
	u32 base;
	i915_reg_t oa_head_ptr;
	i915_reg_t oa_tail_ptr;
	i915_reg_t oa_buffer;
	i915_reg_t oa_ctx_ctrl;
	i915_reg_t oa_ctrl;
	i915_reg_t oa_debug;
	i915_reg_t oa_status;
	u32 oa_ctrl_counter_format_shift;
};
enum oa_type {
	TYPE_OAG,
	TYPE_OAM,
};
struct i915_oa_format {
	u32 format;
	int size;
	int type;
	enum report_header header;
};
struct i915_oa_reg {
	i915_reg_t addr;
	u32 value;
};
struct i915_oa_config {
	struct i915_perf *perf;
	char uuid[UUID_STRING_LEN + 1];
	int id;
	const struct i915_oa_reg *mux_regs;
	u32 mux_regs_len;
	const struct i915_oa_reg *b_counter_regs;
	u32 b_counter_regs_len;
	const struct i915_oa_reg *flex_regs;
	u32 flex_regs_len;
	struct attribute_group sysfs_metric;
	struct attribute *attrs[2];
	struct kobj_attribute sysfs_metric_id;
	struct kref ref;
	struct rcu_head rcu;
};
struct i915_perf_stream;
struct i915_perf_stream_ops {
	void (*enable)(struct i915_perf_stream *stream);
	void (*disable)(struct i915_perf_stream *stream);
	void (*poll_wait)(struct i915_perf_stream *stream,
			  struct file *file,
			  poll_table *wait);
	int (*wait_unlocked)(struct i915_perf_stream *stream);
	int (*read)(struct i915_perf_stream *stream,
		    char __user *buf,
		    size_t count,
		    size_t *offset);
	void (*destroy)(struct i915_perf_stream *stream);
};
struct i915_perf_stream {
	struct i915_perf *perf;
	struct intel_uncore *uncore;
	struct intel_engine_cs *engine;
	struct mutex lock;
	u32 sample_flags;
	int sample_size;
	struct i915_gem_context *ctx;
	bool enabled;
	bool hold_preemption;
	const struct i915_perf_stream_ops *ops;
	struct i915_oa_config *oa_config;
	struct llist_head oa_config_bos;
	struct intel_context *pinned_ctx;
	u32 specific_ctx_id;
	u32 specific_ctx_id_mask;
	struct hrtimer poll_check_timer;
	wait_queue_head_t poll_wq;
	bool pollin;
	bool periodic;
	int period_exponent;
	struct {
		const struct i915_oa_format *format;
		struct i915_vma *vma;
		u8 *vaddr;
		u32 last_ctx_id;
		int size_exponent;
		spinlock_t ptr_lock;
		u32 head;
		u32 tail;
	} oa_buffer;
	struct i915_vma *noa_wait;
	u64 poll_oa_period;
	bool override_gucrc;
};
struct i915_oa_ops {
	bool (*is_valid_b_counter_reg)(struct i915_perf *perf, u32 addr);
	bool (*is_valid_mux_reg)(struct i915_perf *perf, u32 addr);
	bool (*is_valid_flex_reg)(struct i915_perf *perf, u32 addr);
	int (*enable_metric_set)(struct i915_perf_stream *stream,
				 struct i915_active *active);
	void (*disable_metric_set)(struct i915_perf_stream *stream);
	void (*oa_enable)(struct i915_perf_stream *stream);
	void (*oa_disable)(struct i915_perf_stream *stream);
	int (*read)(struct i915_perf_stream *stream,
		    char __user *buf,
		    size_t count,
		    size_t *offset);
	u32 (*oa_hw_tail_read)(struct i915_perf_stream *stream);
};
struct i915_perf_group {
	struct i915_perf_stream *exclusive_stream;
	u32 num_engines;
	struct i915_perf_regs regs;
	enum oa_type type;
};
struct i915_perf_gt {
	struct mutex lock;
	struct intel_sseu sseu;
	u32 num_perf_groups;
	struct i915_perf_group *group;
};
struct i915_perf {
	struct drm_i915_private *i915;
	struct kobject *metrics_kobj;
	struct mutex metrics_lock;
	struct idr metrics_idr;
	struct ratelimit_state spurious_report_rs;
	struct ratelimit_state tail_pointer_race;
	u32 gen7_latched_oastatus1;
	u32 ctx_oactxctrl_offset;
	u32 ctx_flexeu0_offset;
	u32 gen8_valid_ctx_bit;
	struct i915_oa_ops ops;
	const struct i915_oa_format *oa_formats;
#define FORMAT_MASK_SIZE DIV_ROUND_UP(I915_OA_FORMAT_MAX - 1, BITS_PER_LONG)
	unsigned long format_mask[FORMAT_MASK_SIZE];
	atomic64_t noa_programming_delay;
};
#endif  
