 

#ifndef __ATOMBIOS_DP_H__
#define __ATOMBIOS_DP_H__

void amdgpu_atombios_dp_aux_init(struct amdgpu_connector *amdgpu_connector);
u8 amdgpu_atombios_dp_get_sinktype(struct amdgpu_connector *amdgpu_connector);
int amdgpu_atombios_dp_get_dpcd(struct amdgpu_connector *amdgpu_connector);
int amdgpu_atombios_dp_get_panel_mode(struct drm_encoder *encoder,
			       struct drm_connector *connector);
void amdgpu_atombios_dp_set_link_config(struct drm_connector *connector,
				 const struct drm_display_mode *mode);
int amdgpu_atombios_dp_mode_valid_helper(struct drm_connector *connector,
				  struct drm_display_mode *mode);
bool amdgpu_atombios_dp_needs_link_train(struct amdgpu_connector *amdgpu_connector);
void amdgpu_atombios_dp_set_rx_power_state(struct drm_connector *connector,
				    u8 power_state);
void amdgpu_atombios_dp_link_train(struct drm_encoder *encoder,
			    struct drm_connector *connector);

#endif
