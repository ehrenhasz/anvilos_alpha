 
 

#ifndef __AMDGPU_DM_CRTC_H__
#define __AMDGPU_DM_CRTC_H__

void amdgpu_dm_crtc_handle_vblank(struct amdgpu_crtc *acrtc);

bool amdgpu_dm_crtc_modeset_required(struct drm_crtc_state *crtc_state,
		      struct dc_stream_state *new_stream,
		      struct dc_stream_state *old_stream);

int amdgpu_dm_crtc_set_vupdate_irq(struct drm_crtc *crtc, bool enable);

bool amdgpu_dm_crtc_vrr_active_irq(struct amdgpu_crtc *acrtc);

bool amdgpu_dm_crtc_vrr_active(struct dm_crtc_state *dm_state);

int amdgpu_dm_crtc_enable_vblank(struct drm_crtc *crtc);

void amdgpu_dm_crtc_disable_vblank(struct drm_crtc *crtc);

int amdgpu_dm_crtc_init(struct amdgpu_display_manager *dm,
			struct drm_plane *plane,
			uint32_t link_index);

#endif

