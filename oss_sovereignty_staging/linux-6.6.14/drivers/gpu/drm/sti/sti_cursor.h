 
 

#ifndef _STI_CURSOR_H_
#define _STI_CURSOR_H_

struct drm_device;
struct device;

struct drm_plane *sti_cursor_create(struct drm_device *drm_dev,
				    struct device *dev, int desc,
				    void __iomem *baseaddr,
				    unsigned int possible_crtcs);

#endif
