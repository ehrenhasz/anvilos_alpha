#ifndef __RADEON_RV770_H__
#define __RADEON_RV770_H__
struct radeon_device;
struct radeon_ps;
void rv770_set_clk_bypass_mode(struct radeon_device *rdev);
struct rv7xx_ps *rv770_get_ps(struct radeon_ps *rps);
struct rv7xx_power_info *rv770_get_pi(struct radeon_device *rdev);
#endif				 
