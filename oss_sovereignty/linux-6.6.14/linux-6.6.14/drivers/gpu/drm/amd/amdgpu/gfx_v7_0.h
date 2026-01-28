#ifndef __GFX_V7_0_H__
#define __GFX_V7_0_H__
extern const struct amdgpu_ip_block_version gfx_v7_1_ip_block;
extern const struct amdgpu_ip_block_version gfx_v7_2_ip_block;
extern const struct amdgpu_ip_block_version gfx_v7_3_ip_block;
struct amdgpu_device;
struct cik_mqd;
int gfx_v7_0_mqd_commit(struct amdgpu_device *adev, struct cik_mqd *mqd);
#endif
