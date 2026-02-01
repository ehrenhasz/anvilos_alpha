 
#ifndef __AMDGPU_DMA_BUF_H__
#define __AMDGPU_DMA_BUF_H__

#include <drm/drm_gem.h>

struct dma_buf *amdgpu_gem_prime_export(struct drm_gem_object *gobj,
					int flags);
struct drm_gem_object *amdgpu_gem_prime_import(struct drm_device *dev,
					    struct dma_buf *dma_buf);
bool amdgpu_dmabuf_is_xgmi_accessible(struct amdgpu_device *adev,
				      struct amdgpu_bo *bo);

extern const struct dma_buf_ops amdgpu_dmabuf_ops;

#endif
