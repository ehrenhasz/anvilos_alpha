 
#ifndef __ATHUB_V2_1_H__
#define __ATHUB_V2_1_H__

int athub_v2_1_set_clockgating(struct amdgpu_device *adev,
			       enum amd_clockgating_state state);
void athub_v2_1_get_clockgating(struct amdgpu_device *adev, u64 *flags);

#endif
