#ifndef _MTK_DRM_GEM_H_
#define _MTK_DRM_GEM_H_
#include <drm/drm_gem.h>
struct mtk_drm_gem_obj {
	struct drm_gem_object	base;
	void			*cookie;
	void			*kvaddr;
	dma_addr_t		dma_addr;
	unsigned long		dma_attrs;
	struct sg_table		*sg;
	struct page		**pages;
};
#define to_mtk_gem_obj(x)	container_of(x, struct mtk_drm_gem_obj, base)
void mtk_drm_gem_free_object(struct drm_gem_object *gem);
struct mtk_drm_gem_obj *mtk_drm_gem_create(struct drm_device *dev, size_t size,
					   bool alloc_kmap);
int mtk_drm_gem_dumb_create(struct drm_file *file_priv, struct drm_device *dev,
			    struct drm_mode_create_dumb *args);
struct sg_table *mtk_gem_prime_get_sg_table(struct drm_gem_object *obj);
struct drm_gem_object *mtk_gem_prime_import_sg_table(struct drm_device *dev,
			struct dma_buf_attachment *attach, struct sg_table *sg);
int mtk_drm_gem_prime_vmap(struct drm_gem_object *obj, struct iosys_map *map);
void mtk_drm_gem_prime_vunmap(struct drm_gem_object *obj,
			      struct iosys_map *map);
#endif
