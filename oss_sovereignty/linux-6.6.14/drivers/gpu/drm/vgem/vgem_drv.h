 

#ifndef _VGEM_DRV_H_
#define _VGEM_DRV_H_

#include <drm/drm_gem.h>
#include <drm/drm_cache.h>

#include <uapi/drm/vgem_drm.h>

struct vgem_file {
	struct idr fence_idr;
	struct mutex fence_mutex;
};

int vgem_fence_open(struct vgem_file *file);
int vgem_fence_attach_ioctl(struct drm_device *dev,
			    void *data,
			    struct drm_file *file);
int vgem_fence_signal_ioctl(struct drm_device *dev,
			    void *data,
			    struct drm_file *file);
void vgem_fence_close(struct vgem_file *file);

#endif
