#ifndef __OMAPDRM_ENCODER_H__
#define __OMAPDRM_ENCODER_H__
struct drm_device;
struct drm_encoder;
struct omap_dss_device;
struct drm_encoder *omap_encoder_init(struct drm_device *dev,
				      struct omap_dss_device *output);
#endif  
