 

#ifndef __GMC_V9_0_H__
#define __GMC_V9_0_H__

extern const struct amd_ip_funcs gmc_v9_0_ip_funcs;
extern const struct amdgpu_ip_block_version gmc_v9_0_ip_block;

void gmc_v9_0_restore_registers(struct amdgpu_device *adev);
#endif
