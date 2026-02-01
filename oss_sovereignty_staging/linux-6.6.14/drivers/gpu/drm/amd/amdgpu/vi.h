 

#ifndef __VI_H__
#define __VI_H__

#define VI_FLUSH_GPU_TLB_NUM_WREG	3

void vi_srbm_select(struct amdgpu_device *adev,
		    u32 me, u32 pipe, u32 queue, u32 vmid);
void vi_set_virt_ops(struct amdgpu_device *adev);
int vi_set_ip_blocks(struct amdgpu_device *adev);

void legacy_doorbell_index_init(struct amdgpu_device *adev);

#endif
