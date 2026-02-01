 
#ifndef __AMDGPU_GEM_H__
#define __AMDGPU_GEM_H__

#include <drm/amdgpu_drm.h>
#include <drm/drm_gem.h>

 

#define AMDGPU_GEM_DOMAIN_MAX		0x3
#define gem_to_amdgpu_bo(gobj) container_of((gobj), struct amdgpu_bo, tbo.base)

unsigned long amdgpu_gem_timeout(uint64_t timeout_ns);

 
void amdgpu_gem_force_release(struct amdgpu_device *adev);
int amdgpu_gem_object_create(struct amdgpu_device *adev, unsigned long size,
			     int alignment, u32 initial_domain,
			     u64 flags, enum ttm_bo_type type,
			     struct dma_resv *resv,
			     struct drm_gem_object **obj, int8_t xcp_id_plus1);
int amdgpu_mode_dumb_create(struct drm_file *file_priv,
			    struct drm_device *dev,
			    struct drm_mode_create_dumb *args);
int amdgpu_mode_dumb_mmap(struct drm_file *filp,
			  struct drm_device *dev,
			  uint32_t handle, uint64_t *offset_p);

int amdgpu_gem_create_ioctl(struct drm_device *dev, void *data,
			    struct drm_file *filp);
int amdgpu_gem_info_ioctl(struct drm_device *dev, void *data,
			  struct drm_file *filp);
int amdgpu_gem_userptr_ioctl(struct drm_device *dev, void *data,
			struct drm_file *filp);
int amdgpu_gem_mmap_ioctl(struct drm_device *dev, void *data,
			  struct drm_file *filp);
int amdgpu_gem_wait_idle_ioctl(struct drm_device *dev, void *data,
			      struct drm_file *filp);
uint64_t amdgpu_gem_va_map_flags(struct amdgpu_device *adev, uint32_t flags);
int amdgpu_gem_va_ioctl(struct drm_device *dev, void *data,
			  struct drm_file *filp);
int amdgpu_gem_op_ioctl(struct drm_device *dev, void *data,
			struct drm_file *filp);

int amdgpu_gem_metadata_ioctl(struct drm_device *dev, void *data,
				struct drm_file *filp);

#endif
