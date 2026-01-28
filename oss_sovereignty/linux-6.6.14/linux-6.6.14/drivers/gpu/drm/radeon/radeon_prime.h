#ifndef __RADEON_PRIME_H__
#define __RADEON_PRIME_H__
struct dma_buf *radeon_gem_prime_export(struct drm_gem_object *gobj,
					int flags);
struct sg_table *radeon_gem_prime_get_sg_table(struct drm_gem_object *obj);
int radeon_gem_prime_pin(struct drm_gem_object *obj);
void radeon_gem_prime_unpin(struct drm_gem_object *obj);
void *radeon_gem_prime_vmap(struct drm_gem_object *obj);
void radeon_gem_prime_vunmap(struct drm_gem_object *obj, void *vaddr);
struct drm_gem_object *radeon_gem_prime_import_sg_table(struct drm_device *dev,
							struct dma_buf_attachment *,
							struct sg_table *sg);
#endif				 
