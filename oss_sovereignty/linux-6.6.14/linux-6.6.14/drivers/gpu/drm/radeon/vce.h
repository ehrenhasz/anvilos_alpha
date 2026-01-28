#ifndef __VCE_H__
#define __VCE_H__
struct radeon_device;
void vce_v1_0_enable_mgcg(struct radeon_device *rdev, bool enable);
void vce_v2_0_enable_mgcg(struct radeon_device *rdev, bool enable);
#endif                          
