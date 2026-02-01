 

#ifndef __AMDGPU_PLL_H__
#define __AMDGPU_PLL_H__

void amdgpu_pll_compute(struct amdgpu_device *adev,
			 struct amdgpu_pll *pll,
			 u32 freq,
			 u32 *dot_clock_p,
			 u32 *fb_div_p,
			 u32 *frac_fb_div_p,
			 u32 *ref_div_p,
			 u32 *post_div_p);
u32 amdgpu_pll_get_use_mask(struct drm_crtc *crtc);
int amdgpu_pll_get_shared_dp_ppll(struct drm_crtc *crtc);
int amdgpu_pll_get_shared_nondp_ppll(struct drm_crtc *crtc);

#endif
