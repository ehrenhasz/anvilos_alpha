#ifndef __GVT_SCHED_POLICY__
#define __GVT_SCHED_POLICY__
struct intel_gvt;
struct intel_vgpu;
struct intel_gvt_sched_policy_ops {
	int (*init)(struct intel_gvt *gvt);
	void (*clean)(struct intel_gvt *gvt);
	int (*init_vgpu)(struct intel_vgpu *vgpu);
	void (*clean_vgpu)(struct intel_vgpu *vgpu);
	void (*start_schedule)(struct intel_vgpu *vgpu);
	void (*stop_schedule)(struct intel_vgpu *vgpu);
};
void intel_gvt_schedule(struct intel_gvt *gvt);
int intel_gvt_init_sched_policy(struct intel_gvt *gvt);
void intel_gvt_clean_sched_policy(struct intel_gvt *gvt);
int intel_vgpu_init_sched_policy(struct intel_vgpu *vgpu);
void intel_vgpu_clean_sched_policy(struct intel_vgpu *vgpu);
void intel_vgpu_start_schedule(struct intel_vgpu *vgpu);
void intel_vgpu_stop_schedule(struct intel_vgpu *vgpu);
void intel_gvt_kick_schedule(struct intel_gvt *gvt);
#endif
