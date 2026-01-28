#ifndef __SOC21_H__
#define __SOC21_H__
extern const struct amdgpu_ip_block_version soc21_common_ip_block;
void soc21_grbm_select(struct amdgpu_device *adev,
		    u32 me, u32 pipe, u32 queue, u32 vmid);
#endif
