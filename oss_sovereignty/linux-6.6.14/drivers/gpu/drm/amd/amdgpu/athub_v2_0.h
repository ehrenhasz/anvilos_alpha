 
#ifndef __ATHUB_V2_0_H__
#define __ATHUB_V2_0_H__

int athub_v2_0_set_clockgating(struct amdgpu_device *adev,
			       enum amd_clockgating_state state);
void athub_v2_0_get_clockgating(struct amdgpu_device *adev, u64 *flags);

#endif
