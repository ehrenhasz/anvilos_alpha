 

#ifndef __AMDGPU_CONNECTORS_H__
#define __AMDGPU_CONNECTORS_H__

struct edid *amdgpu_connector_edid(struct drm_connector *connector);
void amdgpu_connector_hotplug(struct drm_connector *connector);
int amdgpu_connector_get_monitor_bpc(struct drm_connector *connector);
u16 amdgpu_connector_encoder_get_dp_bridge_encoder_id(struct drm_connector *connector);
bool amdgpu_connector_is_dp12_capable(struct drm_connector *connector);
void
amdgpu_connector_add(struct amdgpu_device *adev,
		      uint32_t connector_id,
		      uint32_t supported_device,
		      int connector_type,
		      struct amdgpu_i2c_bus_rec *i2c_bus,
		      uint16_t connector_object_id,
		      struct amdgpu_hpd *hpd,
		      struct amdgpu_router *router);

#endif
