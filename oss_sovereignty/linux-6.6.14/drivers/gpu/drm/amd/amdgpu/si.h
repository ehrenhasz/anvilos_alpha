 

#ifndef __SI_H__
#define __SI_H__

#define SI_FLUSH_GPU_TLB_NUM_WREG	2

void si_srbm_select(struct amdgpu_device *adev,
		     u32 me, u32 pipe, u32 queue, u32 vmid);
int si_set_ip_blocks(struct amdgpu_device *adev);

#endif
