#ifndef __SI_H__
#define __SI_H__
struct radeon_device;
struct radeon_mc;
int si_mc_load_microcode(struct radeon_device *rdev);
u32 si_gpu_check_soft_reset(struct radeon_device *rdev);
void si_vram_gtt_location(struct radeon_device *rdev, struct radeon_mc *mc);
void si_rlc_reset(struct radeon_device *rdev);
void si_init_uvd_internal_cg(struct radeon_device *rdev);
u32 si_get_csb_size(struct radeon_device *rdev);
void si_get_csb_buffer(struct radeon_device *rdev, volatile u32 *buffer);
#endif                          
