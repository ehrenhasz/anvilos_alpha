#ifndef __GFX_V8_0_H__
#define __GFX_V8_0_H__
extern const struct amdgpu_ip_block_version gfx_v8_0_ip_block;
extern const struct amdgpu_ip_block_version gfx_v8_1_ip_block;
struct amdgpu_device;
struct vi_mqd;
int gfx_v8_0_mqd_commit(struct amdgpu_device *adev, struct vi_mqd *mqd);
#endif
