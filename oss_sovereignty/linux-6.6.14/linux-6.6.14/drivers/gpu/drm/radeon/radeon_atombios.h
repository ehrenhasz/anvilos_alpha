#ifndef __RADEON_ATOMBIOS_H__
#define __RADEON_ATOMBIOS_H__
struct drm_connector;
struct drm_device;
struct drm_display_mode;
struct radeon_device;
struct radeon_encoder;
bool radeon_atom_get_tv_timings(struct radeon_device *rdev, int index,
				struct drm_display_mode *mode);
void radeon_add_atom_encoder(struct drm_device *dev, uint32_t encoder_enum,
			     uint32_t supported_device, u16 caps);
void radeon_atom_backlight_init(struct radeon_encoder *radeon_encoder,
				struct drm_connector *drm_connector);
#endif                          
