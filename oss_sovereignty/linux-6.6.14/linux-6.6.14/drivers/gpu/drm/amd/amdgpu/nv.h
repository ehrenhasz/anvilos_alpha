#ifndef __NV_H__
#define __NV_H__
#include "nbio_v2_3.h"
extern const struct amdgpu_ip_block_version nv_common_ip_block;
void nv_grbm_select(struct amdgpu_device *adev,
		    u32 me, u32 pipe, u32 queue, u32 vmid);
void nv_set_virt_ops(struct amdgpu_device *adev);
#endif
