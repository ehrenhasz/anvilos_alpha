#ifndef _EXYNOS_DRM_GEM_H_
#define _EXYNOS_DRM_GEM_H_
#include <drm/drm_gem.h>
#include <linux/mm_types.h>
#define to_exynos_gem(x)	container_of(x, struct exynos_drm_gem, base)
#define IS_NONCONTIG_BUFFER(f)		(f & EXYNOS_BO_NONCONTIG)
struct exynos_drm_gem {
	struct drm_gem_object	base;
	unsigned int		flags;
	unsigned long		size;
	void			*cookie;
	void			*kvaddr;
	dma_addr_t		dma_addr;
	unsigned long		dma_attrs;
	struct sg_table		*sgt;
};
void exynos_drm_gem_destroy(struct exynos_drm_gem *exynos_gem);
struct exynos_drm_gem *exynos_drm_gem_create(struct drm_device *dev,
					     unsigned int flags,
					     unsigned long size,
					     bool kvmap);
int exynos_drm_gem_create_ioctl(struct drm_device *dev, void *data,
				struct drm_file *file_priv);
int exynos_drm_gem_map_ioctl(struct drm_device *dev, void *data,
			     struct drm_file *file_priv);
struct exynos_drm_gem *exynos_drm_gem_get(struct drm_file *filp,
					  unsigned int gem_handle);
static inline void exynos_drm_gem_put(struct exynos_drm_gem *exynos_gem)
{
	drm_gem_object_put(&exynos_gem->base);
}
int exynos_drm_gem_get_ioctl(struct drm_device *dev, void *data,
				      struct drm_file *file_priv);
void exynos_drm_gem_free_object(struct drm_gem_object *obj);
int exynos_drm_gem_dumb_create(struct drm_file *file_priv,
			       struct drm_device *dev,
			       struct drm_mode_create_dumb *args);
struct drm_gem_object *exynos_drm_gem_prime_import(struct drm_device *dev,
					    struct dma_buf *dma_buf);
struct sg_table *exynos_drm_gem_prime_get_sg_table(struct drm_gem_object *obj);
struct drm_gem_object *
exynos_drm_gem_prime_import_sg_table(struct drm_device *dev,
				     struct dma_buf_attachment *attach,
				     struct sg_table *sgt);
#endif
