 

#include <drm/drm.h>

#include "qxl_drv.h"
#include "qxl_object.h"

void qxl_gem_object_free(struct drm_gem_object *gobj)
{
	struct qxl_bo *qobj = gem_to_qxl_bo(gobj);
	struct qxl_device *qdev;
	struct ttm_buffer_object *tbo;

	qdev = to_qxl(gobj->dev);

	qxl_surface_evict(qdev, qobj, false);

	tbo = &qobj->tbo;
	ttm_bo_put(tbo);
}

int qxl_gem_object_create(struct qxl_device *qdev, int size,
			  int alignment, int initial_domain,
			  bool discardable, bool kernel,
			  struct qxl_surface *surf,
			  struct drm_gem_object **obj)
{
	struct qxl_bo *qbo;
	int r;

	*obj = NULL;
	 
	if (alignment < PAGE_SIZE)
		alignment = PAGE_SIZE;
	r = qxl_bo_create(qdev, size, kernel, false, initial_domain, 0, surf, &qbo);
	if (r) {
		if (r != -ERESTARTSYS)
			DRM_ERROR(
			"Failed to allocate GEM object (%d, %d, %u, %d)\n",
				  size, initial_domain, alignment, r);
		return r;
	}
	*obj = &qbo->tbo.base;

	mutex_lock(&qdev->gem.mutex);
	list_add_tail(&qbo->list, &qdev->gem.objects);
	mutex_unlock(&qdev->gem.mutex);

	return 0;
}

 
int qxl_gem_object_create_with_handle(struct qxl_device *qdev,
				      struct drm_file *file_priv,
				      u32 domain,
				      size_t size,
				      struct qxl_surface *surf,
				      struct drm_gem_object **gobj,
				      uint32_t *handle)
{
	int r;
	struct drm_gem_object *local_gobj;

	BUG_ON(!handle);

	r = qxl_gem_object_create(qdev, size, 0,
				  domain,
				  false, false, surf,
				  &local_gobj);
	if (r)
		return -ENOMEM;
	r = drm_gem_handle_create(file_priv, local_gobj, handle);
	if (r)
		return r;

	if (gobj)
		*gobj = local_gobj;
	else
		 
		drm_gem_object_put(local_gobj);

	return 0;
}

int qxl_gem_object_open(struct drm_gem_object *obj, struct drm_file *file_priv)
{
	return 0;
}

void qxl_gem_object_close(struct drm_gem_object *obj,
			  struct drm_file *file_priv)
{
}

void qxl_gem_init(struct qxl_device *qdev)
{
	INIT_LIST_HEAD(&qdev->gem.objects);
}

void qxl_gem_fini(struct qxl_device *qdev)
{
	qxl_bo_force_delete(qdev);
}
