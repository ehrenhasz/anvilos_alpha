#ifndef __SHMOB_DRM_KMS_H__
#define __SHMOB_DRM_KMS_H__
#include <linux/types.h>
struct drm_gem_dma_object;
struct shmob_drm_device;
struct shmob_drm_format_info {
	u32 fourcc;
	unsigned int bpp;
	bool yuv;
	u32 lddfr;
};
const struct shmob_drm_format_info *shmob_drm_format_info(u32 fourcc);
int shmob_drm_modeset_init(struct shmob_drm_device *sdev);
#endif  
