#ifndef __AMDGPU_DISPLAY_H__
#define __AMDGPU_DISPLAY_H__
#define amdgpu_display_vblank_get_counter(adev, crtc) (adev)->mode_info.funcs->vblank_get_counter((adev), (crtc))
#define amdgpu_display_backlight_set_level(adev, e, l) (adev)->mode_info.funcs->backlight_set_level((e), (l))
#define amdgpu_display_backlight_get_level(adev, e) (adev)->mode_info.funcs->backlight_get_level((e))
#define amdgpu_display_hpd_sense(adev, h) (adev)->mode_info.funcs->hpd_sense((adev), (h))
#define amdgpu_display_hpd_set_polarity(adev, h) (adev)->mode_info.funcs->hpd_set_polarity((adev), (h))
#define amdgpu_display_hpd_get_gpio_reg(adev) (adev)->mode_info.funcs->hpd_get_gpio_reg((adev))
#define amdgpu_display_bandwidth_update(adev) (adev)->mode_info.funcs->bandwidth_update((adev))
#define amdgpu_display_page_flip(adev, crtc, base, async) (adev)->mode_info.funcs->page_flip((adev), (crtc), (base), (async))
#define amdgpu_display_page_flip_get_scanoutpos(adev, crtc, vbl, pos) (adev)->mode_info.funcs->page_flip_get_scanoutpos((adev), (crtc), (vbl), (pos))
#define amdgpu_display_add_encoder(adev, e, s, c) (adev)->mode_info.funcs->add_encoder((adev), (e), (s), (c))
#define amdgpu_display_add_connector(adev, ci, sd, ct, ib, coi, h, r) (adev)->mode_info.funcs->add_connector((adev), (ci), (sd), (ct), (ib), (coi), (h), (r))
void amdgpu_display_hotplug_work_func(struct work_struct *work);
void amdgpu_display_update_priority(struct amdgpu_device *adev);
uint32_t amdgpu_display_supported_domains(struct amdgpu_device *adev,
					  uint64_t bo_flags);
struct drm_framebuffer *
amdgpu_display_user_framebuffer_create(struct drm_device *dev,
				       struct drm_file *file_priv,
				       const struct drm_mode_fb_cmd2 *mode_cmd);
const struct drm_format_info *
amdgpu_lookup_format_info(u32 format, uint64_t modifier);
int amdgpu_display_suspend_helper(struct amdgpu_device *adev);
int amdgpu_display_resume_helper(struct amdgpu_device *adev);
#endif
