 
 

#ifndef __OMAPDRM_FBDEV_H__
#define __OMAPDRM_FBDEV_H__

struct drm_device;

#ifdef CONFIG_DRM_FBDEV_EMULATION
void omap_fbdev_setup(struct drm_device *dev);
#else
static inline void omap_fbdev_setup(struct drm_device *dev)
{
}
#endif

#endif  
