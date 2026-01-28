#ifndef __RADEON_LEGACY_ENCODERS_H__
#define __RADEON_LEGACY_ENCODERS_H__
void radeon_legacy_backlight_init(struct radeon_encoder *radeon_encoder,
				  struct drm_connector *drm_connector);
void radeon_add_legacy_encoder(struct drm_device *dev, uint32_t encoder_enum,
			       uint32_t supported_device);
#endif				 
