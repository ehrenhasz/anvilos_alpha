 

#include "qxl_drv.h"
#include "qxl_object.h"

 

int qxl_gem_prime_pin(struct drm_gem_object *obj)
{
	struct qxl_bo *bo = gem_to_qxl_bo(obj);

	return qxl_bo_pin(bo);
}

void qxl_gem_prime_unpin(struct drm_gem_object *obj)
{
	struct qxl_bo *bo = gem_to_qxl_bo(obj);

	qxl_bo_unpin(bo);
}

struct sg_table *qxl_gem_prime_get_sg_table(struct drm_gem_object *obj)
{
	return ERR_PTR(-ENOSYS);
}

struct drm_gem_object *qxl_gem_prime_import_sg_table(
	struct drm_device *dev, struct dma_buf_attachment *attach,
	struct sg_table *table)
{
	return ERR_PTR(-ENOSYS);
}

int qxl_gem_prime_vmap(struct drm_gem_object *obj, struct iosys_map *map)
{
	struct qxl_bo *bo = gem_to_qxl_bo(obj);
	int ret;

	ret = qxl_bo_vmap_locked(bo, map);
	if (ret < 0)
		return ret;

	return 0;
}

void qxl_gem_prime_vunmap(struct drm_gem_object *obj,
			  struct iosys_map *map)
{
	struct qxl_bo *bo = gem_to_qxl_bo(obj);

	qxl_bo_vunmap_locked(bo);
}
