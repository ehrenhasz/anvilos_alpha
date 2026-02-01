 
 

#ifndef _EXYNOS_DRM_FBDEV_H_
#define _EXYNOS_DRM_FBDEV_H_

#ifdef CONFIG_DRM_FBDEV_EMULATION
void exynos_drm_fbdev_setup(struct drm_device *dev);
#else
static inline void exynos_drm_fbdev_setup(struct drm_device *dev)
{
}
#endif

#endif
