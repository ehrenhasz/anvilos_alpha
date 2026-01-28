#ifndef __CIK_H__
#define __CIK_H__
#define CIK_FLUSH_GPU_TLB_NUM_WREG	3
void cik_srbm_select(struct amdgpu_device *adev,
		     u32 me, u32 pipe, u32 queue, u32 vmid);
int cik_set_ip_blocks(struct amdgpu_device *adev);
void legacy_doorbell_index_init(struct amdgpu_device *adev);
#endif
