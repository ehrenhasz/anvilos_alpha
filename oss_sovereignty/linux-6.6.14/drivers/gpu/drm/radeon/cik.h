 

#ifndef __CIK_H__
#define __CIK_H__

struct radeon_device;

void cik_enter_rlc_safe_mode(struct radeon_device *rdev);
void cik_exit_rlc_safe_mode(struct radeon_device *rdev);
int ci_mc_load_microcode(struct radeon_device *rdev);
void cik_update_cg(struct radeon_device *rdev, u32 block, bool enable);
u32 cik_gpu_check_soft_reset(struct radeon_device *rdev);
void cik_init_cp_pg_table(struct radeon_device *rdev);
u32 cik_get_csb_size(struct radeon_device *rdev);
void cik_get_csb_buffer(struct radeon_device *rdev, volatile u32 *buffer);

int cik_sdma_resume(struct radeon_device *rdev);
void cik_sdma_enable(struct radeon_device *rdev, bool enable);
void cik_sdma_fini(struct radeon_device *rdev);
#endif                          
