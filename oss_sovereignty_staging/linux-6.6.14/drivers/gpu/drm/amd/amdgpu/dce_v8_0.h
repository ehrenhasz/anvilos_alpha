 

#ifndef __DCE_V8_0_H__
#define __DCE_V8_0_H__

extern const struct amdgpu_ip_block_version dce_v8_0_ip_block;
extern const struct amdgpu_ip_block_version dce_v8_1_ip_block;
extern const struct amdgpu_ip_block_version dce_v8_2_ip_block;
extern const struct amdgpu_ip_block_version dce_v8_3_ip_block;
extern const struct amdgpu_ip_block_version dce_v8_5_ip_block;

void dce_v8_0_disable_dce(struct amdgpu_device *adev);

#endif
