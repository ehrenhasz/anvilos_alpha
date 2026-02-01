 
 

#ifndef __SHMOB_DRM_PLANE_H__
#define __SHMOB_DRM_PLANE_H__

struct drm_plane;
struct shmob_drm_device;

int shmob_drm_plane_create(struct shmob_drm_device *sdev, unsigned int index);
void shmob_drm_plane_setup(struct drm_plane *plane);

#endif  
