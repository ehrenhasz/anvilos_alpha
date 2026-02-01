
 

 

#include <linux/dma-buf.h>
#include <linux/pfn_t.h>

#include "v3d_drv.h"
#include "uapi/drm/v3d_drm.h"

 
void v3d_free_object(struct drm_gem_object *obj)
{
	struct v3d_dev *v3d = to_v3d_dev(obj->dev);
	struct v3d_bo *bo = to_v3d_bo(obj);

	v3d_mmu_remove_ptes(bo);

	mutex_lock(&v3d->bo_lock);
	v3d->bo_stats.num_allocated--;
	v3d->bo_stats.pages_allocated -= obj->size >> PAGE_SHIFT;
	mutex_unlock(&v3d->bo_lock);

	spin_lock(&v3d->mm_lock);
	drm_mm_remove_node(&bo->node);
	spin_unlock(&v3d->mm_lock);

	 
	bo->base.pages_mark_dirty_on_put = true;

	drm_gem_shmem_free(&bo->base);
}

static const struct drm_gem_object_funcs v3d_gem_funcs = {
	.free = v3d_free_object,
	.print_info = drm_gem_shmem_object_print_info,
	.pin = drm_gem_shmem_object_pin,
	.unpin = drm_gem_shmem_object_unpin,
	.get_sg_table = drm_gem_shmem_object_get_sg_table,
	.vmap = drm_gem_shmem_object_vmap,
	.vunmap = drm_gem_shmem_object_vunmap,
	.mmap = drm_gem_shmem_object_mmap,
	.vm_ops = &drm_gem_shmem_vm_ops,
};

 
struct drm_gem_object *v3d_create_object(struct drm_device *dev, size_t size)
{
	struct v3d_bo *bo;
	struct drm_gem_object *obj;

	if (size == 0)
		return ERR_PTR(-EINVAL);

	bo = kzalloc(sizeof(*bo), GFP_KERNEL);
	if (!bo)
		return ERR_PTR(-ENOMEM);
	obj = &bo->base.base;

	obj->funcs = &v3d_gem_funcs;
	bo->base.map_wc = true;
	INIT_LIST_HEAD(&bo->unref_head);

	return &bo->base.base;
}

static int
v3d_bo_create_finish(struct drm_gem_object *obj)
{
	struct v3d_dev *v3d = to_v3d_dev(obj->dev);
	struct v3d_bo *bo = to_v3d_bo(obj);
	struct sg_table *sgt;
	int ret;

	 
	sgt = drm_gem_shmem_get_pages_sgt(&bo->base);
	if (IS_ERR(sgt))
		return PTR_ERR(sgt);

	spin_lock(&v3d->mm_lock);
	 
	ret = drm_mm_insert_node_generic(&v3d->mm, &bo->node,
					 obj->size >> PAGE_SHIFT,
					 GMP_GRANULARITY >> PAGE_SHIFT, 0, 0);
	spin_unlock(&v3d->mm_lock);
	if (ret)
		return ret;

	 
	mutex_lock(&v3d->bo_lock);
	v3d->bo_stats.num_allocated++;
	v3d->bo_stats.pages_allocated += obj->size >> PAGE_SHIFT;
	mutex_unlock(&v3d->bo_lock);

	v3d_mmu_insert_ptes(bo);

	return 0;
}

struct v3d_bo *v3d_bo_create(struct drm_device *dev, struct drm_file *file_priv,
			     size_t unaligned_size)
{
	struct drm_gem_shmem_object *shmem_obj;
	struct v3d_bo *bo;
	int ret;

	shmem_obj = drm_gem_shmem_create(dev, unaligned_size);
	if (IS_ERR(shmem_obj))
		return ERR_CAST(shmem_obj);
	bo = to_v3d_bo(&shmem_obj->base);

	ret = v3d_bo_create_finish(&shmem_obj->base);
	if (ret)
		goto free_obj;

	return bo;

free_obj:
	drm_gem_shmem_free(shmem_obj);
	return ERR_PTR(ret);
}

struct drm_gem_object *
v3d_prime_import_sg_table(struct drm_device *dev,
			  struct dma_buf_attachment *attach,
			  struct sg_table *sgt)
{
	struct drm_gem_object *obj;
	int ret;

	obj = drm_gem_shmem_prime_import_sg_table(dev, attach, sgt);
	if (IS_ERR(obj))
		return obj;

	ret = v3d_bo_create_finish(obj);
	if (ret) {
		drm_gem_shmem_free(&to_v3d_bo(obj)->base);
		return ERR_PTR(ret);
	}

	return obj;
}

int v3d_create_bo_ioctl(struct drm_device *dev, void *data,
			struct drm_file *file_priv)
{
	struct drm_v3d_create_bo *args = data;
	struct v3d_bo *bo = NULL;
	int ret;

	if (args->flags != 0) {
		DRM_INFO("unknown create_bo flags: %d\n", args->flags);
		return -EINVAL;
	}

	bo = v3d_bo_create(dev, file_priv, PAGE_ALIGN(args->size));
	if (IS_ERR(bo))
		return PTR_ERR(bo);

	args->offset = bo->node.start << PAGE_SHIFT;

	ret = drm_gem_handle_create(file_priv, &bo->base.base, &args->handle);
	drm_gem_object_put(&bo->base.base);

	return ret;
}

int v3d_mmap_bo_ioctl(struct drm_device *dev, void *data,
		      struct drm_file *file_priv)
{
	struct drm_v3d_mmap_bo *args = data;
	struct drm_gem_object *gem_obj;

	if (args->flags != 0) {
		DRM_INFO("unknown mmap_bo flags: %d\n", args->flags);
		return -EINVAL;
	}

	gem_obj = drm_gem_object_lookup(file_priv, args->handle);
	if (!gem_obj) {
		DRM_DEBUG("Failed to look up GEM BO %d\n", args->handle);
		return -ENOENT;
	}

	args->offset = drm_vma_node_offset_addr(&gem_obj->vma_node);
	drm_gem_object_put(gem_obj);

	return 0;
}

int v3d_get_bo_offset_ioctl(struct drm_device *dev, void *data,
			    struct drm_file *file_priv)
{
	struct drm_v3d_get_bo_offset *args = data;
	struct drm_gem_object *gem_obj;
	struct v3d_bo *bo;

	gem_obj = drm_gem_object_lookup(file_priv, args->handle);
	if (!gem_obj) {
		DRM_DEBUG("Failed to look up GEM BO %d\n", args->handle);
		return -ENOENT;
	}
	bo = to_v3d_bo(gem_obj);

	args->offset = bo->node.start << PAGE_SHIFT;

	drm_gem_object_put(gem_obj);
	return 0;
}
