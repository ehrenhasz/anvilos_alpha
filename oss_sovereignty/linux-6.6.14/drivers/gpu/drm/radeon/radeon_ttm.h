 

#ifndef __RADEON_TTM_H__
#define __RADEON_TTM_H__

struct radeon_device;

int radeon_ttm_init(struct radeon_device *rdev);
void radeon_ttm_fini(struct radeon_device *rdev);

#endif				 
