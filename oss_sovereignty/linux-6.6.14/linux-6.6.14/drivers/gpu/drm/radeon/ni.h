#ifndef __NI_H__
#define __NI_H__
struct radeon_device;
void cayman_cp_int_cntl_setup(struct radeon_device *rdev,
			      int ring, u32 cp_int_cntl);
void cayman_vm_decode_fault(struct radeon_device *rdev,
			    u32 status, u32 addr);
u32 cayman_gpu_check_soft_reset(struct radeon_device *rdev);
#endif				 
