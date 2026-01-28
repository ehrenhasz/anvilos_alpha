#ifndef __AMDGPU_HDP_H__
#define __AMDGPU_HDP_H__
#include "amdgpu_ras.h"
struct amdgpu_hdp_ras {
	struct amdgpu_ras_block_object ras_block;
};
struct amdgpu_hdp_funcs {
	void (*flush_hdp)(struct amdgpu_device *adev, struct amdgpu_ring *ring);
	void (*invalidate_hdp)(struct amdgpu_device *adev,
			       struct amdgpu_ring *ring);
	void (*update_clock_gating)(struct amdgpu_device *adev, bool enable);
	void (*get_clock_gating_state)(struct amdgpu_device *adev, u64 *flags);
	void (*init_registers)(struct amdgpu_device *adev);
};
struct amdgpu_hdp {
	struct ras_common_if			*ras_if;
	const struct amdgpu_hdp_funcs		*funcs;
	struct amdgpu_hdp_ras	*ras;
};
int amdgpu_hdp_ras_sw_init(struct amdgpu_device *adev);
#endif  
