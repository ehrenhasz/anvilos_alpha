 
 

#ifndef _INTEL_GUC_H_
#define _INTEL_GUC_H_

#include <linux/delay.h>
#include <linux/iosys-map.h>
#include <linux/xarray.h>

#include "intel_guc_ct.h"
#include "intel_guc_fw.h"
#include "intel_guc_fwif.h"
#include "intel_guc_log.h"
#include "intel_guc_reg.h"
#include "intel_guc_slpc_types.h"
#include "intel_uc_fw.h"
#include "intel_uncore.h"
#include "i915_utils.h"
#include "i915_vma.h"

struct __guc_ads_blob;
struct intel_guc_state_capture;

 
struct intel_guc {
	 
	struct intel_uc_fw fw;
	 
	struct intel_guc_log log;
	 
	struct intel_guc_ct ct;
	 
	struct intel_guc_slpc slpc;
	 
	struct intel_guc_state_capture *capture;

	 
	struct dentry *dbgfs_node;

	 
	struct i915_sched_engine *sched_engine;
	 
	struct i915_request *stalled_request;
	 
	enum {
		STALL_NONE,
		STALL_REGISTER_CONTEXT,
		STALL_MOVE_LRC_TAIL,
		STALL_ADD_REQUEST,
	} submission_stall_reason;

	 
	 
	spinlock_t irq_lock;
	 
	unsigned int msg_enabled_mask;

	 
	atomic_t outstanding_submission_g2h;

	 
	struct {
		bool enabled;
		void (*reset)(struct intel_guc *guc);
		void (*enable)(struct intel_guc *guc);
		void (*disable)(struct intel_guc *guc);
	} interrupts;

	 
	struct {
		 
		spinlock_t lock;
		 
		struct ida guc_ids;
		 
		int num_guc_ids;
		 
		unsigned long *guc_ids_bitmap;
		 
		struct list_head guc_id_list;
		 
		unsigned int guc_ids_in_use;
		 
		struct list_head destroyed_contexts;
		 
		struct work_struct destroyed_worker;
		 
		struct work_struct reset_fail_worker;
		 
		intel_engine_mask_t reset_fail_mask;
		 
		unsigned int sched_disable_delay_ms;
		 
		unsigned int sched_disable_gucid_threshold;
	} submission_state;

	 
	bool submission_supported;
	 
	bool submission_selected;
	 
	bool submission_initialized;
	 
	struct intel_uc_fw_ver submission_version;

	 
	bool rc_supported;
	 
	bool rc_selected;

	 
	struct i915_vma *ads_vma;
	 
	struct iosys_map ads_map;
	 
	u32 ads_regset_size;
	 
	u32 ads_regset_count[I915_NUM_ENGINES];
	 
	struct guc_mmio_reg *ads_regset;
	 
	u32 ads_golden_ctxt_size;
	 
	u32 ads_capture_size;
	 
	u32 ads_engine_usage_size;

	 
	struct i915_vma *lrc_desc_pool_v69;
	 
	void *lrc_desc_pool_vaddr_v69;

	 
	struct xarray context_lookup;

	 
	u32 params[GUC_CTL_MAX_DWORDS];

	 
	struct {
		u32 base;
		unsigned int count;
		enum forcewake_domains fw_domains;
	} send_regs;

	 
	i915_reg_t notify_reg;

	 
	u32 mmio_msg;

	 
	struct mutex send_mutex;

	 
	struct {
		 
		spinlock_t lock;

		 
		u64 gt_stamp;

		 
		unsigned long ping_delay;

		 
		struct delayed_work work;

		 
		u32 shift;

		 
		unsigned long last_stat_jiffies;
	} timestamp;

#ifdef CONFIG_DRM_I915_SELFTEST
	 
	int number_guc_id_stolen;
#endif
};

 
#define MAKE_GUC_VER(maj, min, pat)	(((maj) << 16) | ((min) << 8) | (pat))
#define MAKE_GUC_VER_STRUCT(ver)	MAKE_GUC_VER((ver).major, (ver).minor, (ver).patch)
#define GUC_SUBMIT_VER(guc)		MAKE_GUC_VER_STRUCT((guc)->submission_version)

static inline struct intel_guc *log_to_guc(struct intel_guc_log *log)
{
	return container_of(log, struct intel_guc, log);
}

static
inline int intel_guc_send(struct intel_guc *guc, const u32 *action, u32 len)
{
	return intel_guc_ct_send(&guc->ct, action, len, NULL, 0, 0);
}

static
inline int intel_guc_send_nb(struct intel_guc *guc, const u32 *action, u32 len,
			     u32 g2h_len_dw)
{
	return intel_guc_ct_send(&guc->ct, action, len, NULL, 0,
				 MAKE_SEND_FLAGS(g2h_len_dw));
}

static inline int
intel_guc_send_and_receive(struct intel_guc *guc, const u32 *action, u32 len,
			   u32 *response_buf, u32 response_buf_size)
{
	return intel_guc_ct_send(&guc->ct, action, len,
				 response_buf, response_buf_size, 0);
}

static inline int intel_guc_send_busy_loop(struct intel_guc *guc,
					   const u32 *action,
					   u32 len,
					   u32 g2h_len_dw,
					   bool loop)
{
	int err;
	unsigned int sleep_period_ms = 1;
	bool not_atomic = !in_atomic() && !irqs_disabled();

	 

	 
	might_sleep_if(loop && not_atomic);

retry:
	err = intel_guc_send_nb(guc, action, len, g2h_len_dw);
	if (unlikely(err == -EBUSY && loop)) {
		if (likely(not_atomic)) {
			if (msleep_interruptible(sleep_period_ms))
				return -EINTR;
			sleep_period_ms = sleep_period_ms << 1;
		} else {
			cpu_relax();
		}
		goto retry;
	}

	return err;
}

 
static inline void intel_guc_to_host_event_handler(struct intel_guc *guc)
{
	if (guc->interrupts.enabled)
		intel_guc_ct_event_handler(&guc->ct);
}

 
#define GUC_GGTT_TOP	0xFEE00000

 
static inline u32 intel_guc_ggtt_offset(struct intel_guc *guc,
					struct i915_vma *vma)
{
	u32 offset = i915_ggtt_offset(vma);

	GEM_BUG_ON(offset < i915_ggtt_pin_bias(vma));
	GEM_BUG_ON(range_overflows_t(u64, offset, vma->size, GUC_GGTT_TOP));

	return offset;
}

void intel_guc_init_early(struct intel_guc *guc);
void intel_guc_init_late(struct intel_guc *guc);
void intel_guc_init_send_regs(struct intel_guc *guc);
void intel_guc_write_params(struct intel_guc *guc);
int intel_guc_init(struct intel_guc *guc);
void intel_guc_fini(struct intel_guc *guc);
void intel_guc_notify(struct intel_guc *guc);
int intel_guc_send_mmio(struct intel_guc *guc, const u32 *action, u32 len,
			u32 *response_buf, u32 response_buf_size);
int intel_guc_to_host_process_recv_msg(struct intel_guc *guc,
				       const u32 *payload, u32 len);
int intel_guc_auth_huc(struct intel_guc *guc, u32 rsa_offset);
int intel_guc_suspend(struct intel_guc *guc);
int intel_guc_resume(struct intel_guc *guc);
struct i915_vma *intel_guc_allocate_vma(struct intel_guc *guc, u32 size);
int intel_guc_allocate_and_map_vma(struct intel_guc *guc, u32 size,
				   struct i915_vma **out_vma, void **out_vaddr);
int intel_guc_self_cfg32(struct intel_guc *guc, u16 key, u32 value);
int intel_guc_self_cfg64(struct intel_guc *guc, u16 key, u64 value);

static inline bool intel_guc_is_supported(struct intel_guc *guc)
{
	return intel_uc_fw_is_supported(&guc->fw);
}

static inline bool intel_guc_is_wanted(struct intel_guc *guc)
{
	return intel_uc_fw_is_enabled(&guc->fw);
}

static inline bool intel_guc_is_used(struct intel_guc *guc)
{
	GEM_BUG_ON(__intel_uc_fw_status(&guc->fw) == INTEL_UC_FIRMWARE_SELECTED);
	return intel_uc_fw_is_available(&guc->fw);
}

static inline bool intel_guc_is_fw_running(struct intel_guc *guc)
{
	return intel_uc_fw_is_running(&guc->fw);
}

static inline bool intel_guc_is_ready(struct intel_guc *guc)
{
	return intel_guc_is_fw_running(guc) && intel_guc_ct_enabled(&guc->ct);
}

static inline void intel_guc_reset_interrupts(struct intel_guc *guc)
{
	guc->interrupts.reset(guc);
}

static inline void intel_guc_enable_interrupts(struct intel_guc *guc)
{
	guc->interrupts.enable(guc);
}

static inline void intel_guc_disable_interrupts(struct intel_guc *guc)
{
	guc->interrupts.disable(guc);
}

static inline int intel_guc_sanitize(struct intel_guc *guc)
{
	intel_uc_fw_sanitize(&guc->fw);
	intel_guc_disable_interrupts(guc);
	intel_guc_ct_sanitize(&guc->ct);
	guc->mmio_msg = 0;

	return 0;
}

static inline void intel_guc_enable_msg(struct intel_guc *guc, u32 mask)
{
	spin_lock_irq(&guc->irq_lock);
	guc->msg_enabled_mask |= mask;
	spin_unlock_irq(&guc->irq_lock);
}

static inline void intel_guc_disable_msg(struct intel_guc *guc, u32 mask)
{
	spin_lock_irq(&guc->irq_lock);
	guc->msg_enabled_mask &= ~mask;
	spin_unlock_irq(&guc->irq_lock);
}

int intel_guc_wait_for_idle(struct intel_guc *guc, long timeout);

int intel_guc_deregister_done_process_msg(struct intel_guc *guc,
					  const u32 *msg, u32 len);
int intel_guc_sched_done_process_msg(struct intel_guc *guc,
				     const u32 *msg, u32 len);
int intel_guc_context_reset_process_msg(struct intel_guc *guc,
					const u32 *msg, u32 len);
int intel_guc_engine_failure_process_msg(struct intel_guc *guc,
					 const u32 *msg, u32 len);
int intel_guc_error_capture_process_msg(struct intel_guc *guc,
					const u32 *msg, u32 len);

struct intel_engine_cs *
intel_guc_lookup_engine(struct intel_guc *guc, u8 guc_class, u8 instance);

void intel_guc_find_hung_context(struct intel_engine_cs *engine);

int intel_guc_global_policies_update(struct intel_guc *guc);

void intel_guc_context_ban(struct intel_context *ce, struct i915_request *rq);

void intel_guc_submission_reset_prepare(struct intel_guc *guc);
void intel_guc_submission_reset(struct intel_guc *guc, intel_engine_mask_t stalled);
void intel_guc_submission_reset_finish(struct intel_guc *guc);
void intel_guc_submission_cancel_requests(struct intel_guc *guc);

void intel_guc_load_status(struct intel_guc *guc, struct drm_printer *p);

void intel_guc_write_barrier(struct intel_guc *guc);

void intel_guc_dump_time_info(struct intel_guc *guc, struct drm_printer *p);

int intel_guc_sched_disable_gucid_threshold_max(struct intel_guc *guc);

#endif
