#ifndef __ATOMBIOS_CRTC_H__
#define __ATOMBIOS_CRTC_H__
void amdgpu_atombios_crtc_overscan_setup(struct drm_crtc *crtc,
				  struct drm_display_mode *mode,
				  struct drm_display_mode *adjusted_mode);
void amdgpu_atombios_crtc_scaler_setup(struct drm_crtc *crtc);
void amdgpu_atombios_crtc_lock(struct drm_crtc *crtc, int lock);
void amdgpu_atombios_crtc_enable(struct drm_crtc *crtc, int state);
void amdgpu_atombios_crtc_blank(struct drm_crtc *crtc, int state);
void amdgpu_atombios_crtc_powergate(struct drm_crtc *crtc, int state);
void amdgpu_atombios_crtc_powergate_init(struct amdgpu_device *adev);
void amdgpu_atombios_crtc_set_dtd_timing(struct drm_crtc *crtc,
				  struct drm_display_mode *mode);
void amdgpu_atombios_crtc_set_disp_eng_pll(struct amdgpu_device *adev,
				    u32 dispclk);
u32 amdgpu_atombios_crtc_set_dce_clock(struct amdgpu_device *adev,
				       u32 freq, u8 clk_type, u8 clk_src);
void amdgpu_atombios_crtc_program_pll(struct drm_crtc *crtc,
			       u32 crtc_id,
			       int pll_id,
			       u32 encoder_mode,
			       u32 encoder_id,
			       u32 clock,
			       u32 ref_div,
			       u32 fb_div,
			       u32 frac_fb_div,
			       u32 post_div,
			       int bpc,
			       bool ss_enabled,
			       struct amdgpu_atom_ss *ss);
int amdgpu_atombios_crtc_prepare_pll(struct drm_crtc *crtc,
			      struct drm_display_mode *mode);
void amdgpu_atombios_crtc_set_pll(struct drm_crtc *crtc,
			   struct drm_display_mode *mode);
#endif
