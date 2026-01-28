#ifndef _EXYNOS_DRM_VIDI_H_
#define _EXYNOS_DRM_VIDI_H_
#ifdef CONFIG_DRM_EXYNOS_VIDI
int vidi_connection_ioctl(struct drm_device *drm_dev, void *data,
				struct drm_file *file_priv);
#else
#define vidi_connection_ioctl	NULL
#endif
#endif
