#ifndef _GVT_SCHEDULER_H_
#define _GVT_SCHEDULER_H_
#include "gt/intel_engine_types.h"
#include "execlist.h"
#include "interrupt.h"
struct intel_gvt_workload_scheduler {
	struct intel_vgpu *current_vgpu;
	struct intel_vgpu *next_vgpu;
	struct intel_vgpu_workload *current_workload[I915_NUM_ENGINES];
	bool need_reschedule;
	spinlock_t mmio_context_lock;
	struct intel_vgpu *engine_owner[I915_NUM_ENGINES];
	wait_queue_head_t workload_complete_wq;
	struct task_struct *thread[I915_NUM_ENGINES];
	wait_queue_head_t waitq[I915_NUM_ENGINES];
	void *sched_data;
	const struct intel_gvt_sched_policy_ops *sched_ops;
};
#define INDIRECT_CTX_ADDR_MASK 0xffffffc0
#define INDIRECT_CTX_SIZE_MASK 0x3f
struct shadow_indirect_ctx {
	struct drm_i915_gem_object *obj;
	unsigned long guest_gma;
	unsigned long shadow_gma;
	void *shadow_va;
	u32 size;
};
#define PER_CTX_ADDR_MASK 0xfffff000
struct shadow_per_ctx {
	unsigned long guest_gma;
	unsigned long shadow_gma;
	unsigned valid;
};
struct intel_shadow_wa_ctx {
	struct shadow_indirect_ctx indirect_ctx;
	struct shadow_per_ctx per_ctx;
};
struct intel_vgpu_workload {
	struct intel_vgpu *vgpu;
	const struct intel_engine_cs *engine;
	struct i915_request *req;
	bool dispatched;
	bool shadow;       
	int status;
	struct intel_vgpu_mm *shadow_mm;
	struct list_head lri_shadow_mm;  
	int (*prepare)(struct intel_vgpu_workload *);
	int (*complete)(struct intel_vgpu_workload *);
	struct list_head list;
	DECLARE_BITMAP(pending_events, INTEL_GVT_EVENT_MAX);
	void *shadow_ring_buffer_va;
	struct execlist_ctx_descriptor_format ctx_desc;
	struct execlist_ring_context *ring_context;
	unsigned long rb_head, rb_tail, rb_ctl, rb_start, rb_len;
	unsigned long guest_rb_head;
	bool restore_inhibit;
	struct intel_vgpu_elsp_dwords elsp_dwords;
	bool emulate_schedule_in;
	atomic_t shadow_ctx_active;
	wait_queue_head_t shadow_ctx_status_wq;
	u64 ring_context_gpa;
	struct list_head shadow_bb;
	struct intel_shadow_wa_ctx wa_ctx;
	u32 oactxctrl;
	u32 flex_mmio[7];
};
struct intel_vgpu_shadow_bb {
	struct list_head list;
	struct drm_i915_gem_object *obj;
	struct i915_vma *vma;
	void *va;
	u32 *bb_start_cmd_va;
	unsigned long bb_offset;
	bool ppgtt;
};
#define workload_q_head(vgpu, e) \
	(&(vgpu)->submission.workload_q_head[(e)->id])
void intel_vgpu_queue_workload(struct intel_vgpu_workload *workload);
int intel_gvt_init_workload_scheduler(struct intel_gvt *gvt);
void intel_gvt_clean_workload_scheduler(struct intel_gvt *gvt);
void intel_gvt_wait_vgpu_idle(struct intel_vgpu *vgpu);
int intel_vgpu_setup_submission(struct intel_vgpu *vgpu);
void intel_vgpu_reset_submission(struct intel_vgpu *vgpu,
				 intel_engine_mask_t engine_mask);
void intel_vgpu_clean_submission(struct intel_vgpu *vgpu);
int intel_vgpu_select_submission_ops(struct intel_vgpu *vgpu,
				     intel_engine_mask_t engine_mask,
				     unsigned int interface);
extern const struct intel_vgpu_submission_ops
intel_vgpu_execlist_submission_ops;
struct intel_vgpu_workload *
intel_vgpu_create_workload(struct intel_vgpu *vgpu,
			   const struct intel_engine_cs *engine,
			   struct execlist_ctx_descriptor_format *desc);
void intel_vgpu_destroy_workload(struct intel_vgpu_workload *workload);
void intel_vgpu_clean_workloads(struct intel_vgpu *vgpu,
				intel_engine_mask_t engine_mask);
#endif
