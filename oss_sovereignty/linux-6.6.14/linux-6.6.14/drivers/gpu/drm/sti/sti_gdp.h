#ifndef _STI_GDP_H_
#define _STI_GDP_H_
#include <linux/types.h>
#include <drm/drm_plane.h>
struct drm_device;
struct device;
struct drm_plane *sti_gdp_create(struct drm_device *drm_dev,
				 struct device *dev, int desc,
				 void __iomem *baseaddr,
				 unsigned int possible_crtcs,
				 enum drm_plane_type type);
#endif
