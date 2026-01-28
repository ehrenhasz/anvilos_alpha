#ifndef __RADEON_KMS_H__
#define __RADEON_KMS_H__
u32 radeon_get_vblank_counter_kms(struct drm_crtc *crtc);
int radeon_enable_vblank_kms(struct drm_crtc *crtc);
void radeon_disable_vblank_kms(struct drm_crtc *crtc);
#endif				 
