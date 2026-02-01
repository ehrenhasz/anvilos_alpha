
 

#include <linux/dma-buf.h>
#include <linux/dma-mapping.h>
#include <linux/export.h>
#include <linux/mm.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/slab.h>

#include <drm/drm.h>
#include <drm/drm_device.h>
#include <drm/drm_drv.h>
#include <drm/drm_gem_dma_helper.h>
#include <drm/drm_vma_manager.h>

 

static const struct drm_gem_object_funcs drm_gem_dma_default_funcs = {
	.free = drm_gem_dma_object_free,
	.print_info = drm_gem_dma_object_print_info,
	.get_sg_table = drm_gem_dma_object_get_sg_table,
	.vmap = drm_gem_dma_object_vmap,
	.mmap = drm_gem_dma_object_mmap,
	.vm_ops = &drm_gem_dma_vm_ops,
};

 
static struct drm_gem_dma_object *
__drm_gem_dma_create(struct drm_device *drm, size_t size, bool private)
{
	struct drm_gem_dma_object *dma_obj;
	struct drm_gem_object *gem_obj;
	int ret = 0;

	if (drm->driver->gem_create_object) {
		gem_obj = drm->driver->gem_create_object(drm, size);
		if (IS_ERR(gem_obj))
			return ERR_CAST(gem_obj);
		dma_obj = to_drm_gem_dma_obj(gem_obj);
	} else {
		dma_obj = kzalloc(sizeof(*dma_obj), GFP_KERNEL);
		if (!dma_obj)
			return ERR_PTR(-ENOMEM);
		gem_obj = &dma_obj->base;
	}

	if (!gem_obj->funcs)
		gem_obj->funcs = &drm_gem_dma_default_funcs;

	if (private) {
		drm_gem_private_object_init(drm, gem_obj, size);

		 
		dma_obj->map_noncoherent = false;
	} else {
		ret = drm_gem_object_init(drm, gem_obj, size);
	}
	if (ret)
		goto error;

	ret = drm_gem_create_mmap_offset(gem_obj);
	if (ret) {
		drm_gem_object_release(gem_obj);
		goto error;
	}

	return dma_obj;

error:
	kfree(dma_obj);
	return ERR_PTR(ret);
}

 
struct drm_gem_dma_object *drm_gem_dma_create(struct drm_device *drm,
					      size_t size)
{
	struct drm_gem_dma_object *dma_obj;
	int ret;

	size = round_up(size, PAGE_SIZE);

	dma_obj = __drm_gem_dma_create(drm, size, false);
	if (IS_ERR(dma_obj))
		return dma_obj;

	if (dma_obj->map_noncoherent) {
		dma_obj->vaddr = dma_alloc_noncoherent(drm->dev, size,
						       &dma_obj->dma_addr,
						       DMA_TO_DEVICE,
						       GFP_KERNEL | __GFP_NOWARN);
	} else {
		dma_obj->vaddr = dma_alloc_wc(drm->dev, size,
					      &dma_obj->dma_addr,
					      GFP_KERNEL | __GFP_NOWARN);
	}
	if (!dma_obj->vaddr) {
		drm_dbg(drm, "failed to allocate buffer with size %zu\n",
			 size);
		ret = -ENOMEM;
		goto error;
	}

	return dma_obj;

error:
	drm_gem_object_put(&dma_obj->base);
	return ERR_PTR(ret);
}
EXPORT_SYMBOL_GPL(drm_gem_dma_create);

 
static struct drm_gem_dma_object *
drm_gem_dma_create_with_handle(struct drm_file *file_priv,
			       struct drm_device *drm, size_t size,
			       uint32_t *handle)
{
	struct drm_gem_dma_object *dma_obj;
	struct drm_gem_object *gem_obj;
	int ret;

	dma_obj = drm_gem_dma_create(drm, size);
	if (IS_ERR(dma_obj))
		return dma_obj;

	gem_obj = &dma_obj->base;

	 
	ret = drm_gem_handle_create(file_priv, gem_obj, handle);
	 
	drm_gem_object_put(gem_obj);
	if (ret)
		return ERR_PTR(ret);

	return dma_obj;
}

 
void drm_gem_dma_free(struct drm_gem_dma_object *dma_obj)
{
	struct drm_gem_object *gem_obj = &dma_obj->base;
	struct iosys_map map = IOSYS_MAP_INIT_VADDR(dma_obj->vaddr);

	if (gem_obj->import_attach) {
		if (dma_obj->vaddr)
			dma_buf_vunmap_unlocked(gem_obj->import_attach->dmabuf, &map);
		drm_prime_gem_destroy(gem_obj, dma_obj->sgt);
	} else if (dma_obj->vaddr) {
		if (dma_obj->map_noncoherent)
			dma_free_noncoherent(gem_obj->dev->dev, dma_obj->base.size,
					     dma_obj->vaddr, dma_obj->dma_addr,
					     DMA_TO_DEVICE);
		else
			dma_free_wc(gem_obj->dev->dev, dma_obj->base.size,
				    dma_obj->vaddr, dma_obj->dma_addr);
	}

	drm_gem_object_release(gem_obj);

	kfree(dma_obj);
}
EXPORT_SYMBOL_GPL(drm_gem_dma_free);

 
int drm_gem_dma_dumb_create_internal(struct drm_file *file_priv,
				     struct drm_device *drm,
				     struct drm_mode_create_dumb *args)
{
	unsigned int min_pitch = DIV_ROUND_UP(args->width * args->bpp, 8);
	struct drm_gem_dma_object *dma_obj;

	if (args->pitch < min_pitch)
		args->pitch = min_pitch;

	if (args->size < args->pitch * args->height)
		args->size = args->pitch * args->height;

	dma_obj = drm_gem_dma_create_with_handle(file_priv, drm, args->size,
						 &args->handle);
	return PTR_ERR_OR_ZERO(dma_obj);
}
EXPORT_SYMBOL_GPL(drm_gem_dma_dumb_create_internal);

 
int drm_gem_dma_dumb_create(struct drm_file *file_priv,
			    struct drm_device *drm,
			    struct drm_mode_create_dumb *args)
{
	struct drm_gem_dma_object *dma_obj;

	args->pitch = DIV_ROUND_UP(args->width * args->bpp, 8);
	args->size = args->pitch * args->height;

	dma_obj = drm_gem_dma_create_with_handle(file_priv, drm, args->size,
						 &args->handle);
	return PTR_ERR_OR_ZERO(dma_obj);
}
EXPORT_SYMBOL_GPL(drm_gem_dma_dumb_create);

const struct vm_operations_struct drm_gem_dma_vm_ops = {
	.open = drm_gem_vm_open,
	.close = drm_gem_vm_close,
};
EXPORT_SYMBOL_GPL(drm_gem_dma_vm_ops);

#ifndef CONFIG_MMU
 
unsigned long drm_gem_dma_get_unmapped_area(struct file *filp,
					    unsigned long addr,
					    unsigned long len,
					    unsigned long pgoff,
					    unsigned long flags)
{
	struct drm_gem_dma_object *dma_obj;
	struct drm_gem_object *obj = NULL;
	struct drm_file *priv = filp->private_data;
	struct drm_device *dev = priv->minor->dev;
	struct drm_vma_offset_node *node;

	if (drm_dev_is_unplugged(dev))
		return -ENODEV;

	drm_vma_offset_lock_lookup(dev->vma_offset_manager);
	node = drm_vma_offset_exact_lookup_locked(dev->vma_offset_manager,
						  pgoff,
						  len >> PAGE_SHIFT);
	if (likely(node)) {
		obj = container_of(node, struct drm_gem_object, vma_node);
		 
		if (!kref_get_unless_zero(&obj->refcount))
			obj = NULL;
	}

	drm_vma_offset_unlock_lookup(dev->vma_offset_manager);

	if (!obj)
		return -EINVAL;

	if (!drm_vma_node_is_allowed(node, priv)) {
		drm_gem_object_put(obj);
		return -EACCES;
	}

	dma_obj = to_drm_gem_dma_obj(obj);

	drm_gem_object_put(obj);

	return dma_obj->vaddr ? (unsigned long)dma_obj->vaddr : -EINVAL;
}
EXPORT_SYMBOL_GPL(drm_gem_dma_get_unmapped_area);
#endif

 
void drm_gem_dma_print_info(const struct drm_gem_dma_object *dma_obj,
			    struct drm_printer *p, unsigned int indent)
{
	drm_printf_indent(p, indent, "dma_addr=%pad\n", &dma_obj->dma_addr);
	drm_printf_indent(p, indent, "vaddr=%p\n", dma_obj->vaddr);
}
EXPORT_SYMBOL(drm_gem_dma_print_info);

 
struct sg_table *drm_gem_dma_get_sg_table(struct drm_gem_dma_object *dma_obj)
{
	struct drm_gem_object *obj = &dma_obj->base;
	struct sg_table *sgt;
	int ret;

	sgt = kzalloc(sizeof(*sgt), GFP_KERNEL);
	if (!sgt)
		return ERR_PTR(-ENOMEM);

	ret = dma_get_sgtable(obj->dev->dev, sgt, dma_obj->vaddr,
			      dma_obj->dma_addr, obj->size);
	if (ret < 0)
		goto out;

	return sgt;

out:
	kfree(sgt);
	return ERR_PTR(ret);
}
EXPORT_SYMBOL_GPL(drm_gem_dma_get_sg_table);

 
struct drm_gem_object *
drm_gem_dma_prime_import_sg_table(struct drm_device *dev,
				  struct dma_buf_attachment *attach,
				  struct sg_table *sgt)
{
	struct drm_gem_dma_object *dma_obj;

	 
	if (drm_prime_get_contiguous_size(sgt) < attach->dmabuf->size)
		return ERR_PTR(-EINVAL);

	 
	dma_obj = __drm_gem_dma_create(dev, attach->dmabuf->size, true);
	if (IS_ERR(dma_obj))
		return ERR_CAST(dma_obj);

	dma_obj->dma_addr = sg_dma_address(sgt->sgl);
	dma_obj->sgt = sgt;

	drm_dbg_prime(dev, "dma_addr = %pad, size = %zu\n", &dma_obj->dma_addr,
		      attach->dmabuf->size);

	return &dma_obj->base;
}
EXPORT_SYMBOL_GPL(drm_gem_dma_prime_import_sg_table);

 
int drm_gem_dma_vmap(struct drm_gem_dma_object *dma_obj,
		     struct iosys_map *map)
{
	iosys_map_set_vaddr(map, dma_obj->vaddr);

	return 0;
}
EXPORT_SYMBOL_GPL(drm_gem_dma_vmap);

 
int drm_gem_dma_mmap(struct drm_gem_dma_object *dma_obj, struct vm_area_struct *vma)
{
	struct drm_gem_object *obj = &dma_obj->base;
	int ret;

	 
	vma->vm_pgoff -= drm_vma_node_start(&obj->vma_node);
	vm_flags_mod(vma, VM_DONTEXPAND, VM_PFNMAP);

	if (dma_obj->map_noncoherent) {
		vma->vm_page_prot = vm_get_page_prot(vma->vm_flags);

		ret = dma_mmap_pages(dma_obj->base.dev->dev,
				     vma, vma->vm_end - vma->vm_start,
				     virt_to_page(dma_obj->vaddr));
	} else {
		ret = dma_mmap_wc(dma_obj->base.dev->dev, vma, dma_obj->vaddr,
				  dma_obj->dma_addr,
				  vma->vm_end - vma->vm_start);
	}
	if (ret)
		drm_gem_vm_close(vma);

	return ret;
}
EXPORT_SYMBOL_GPL(drm_gem_dma_mmap);

 
struct drm_gem_object *
drm_gem_dma_prime_import_sg_table_vmap(struct drm_device *dev,
				       struct dma_buf_attachment *attach,
				       struct sg_table *sgt)
{
	struct drm_gem_dma_object *dma_obj;
	struct drm_gem_object *obj;
	struct iosys_map map;
	int ret;

	ret = dma_buf_vmap_unlocked(attach->dmabuf, &map);
	if (ret) {
		DRM_ERROR("Failed to vmap PRIME buffer\n");
		return ERR_PTR(ret);
	}

	obj = drm_gem_dma_prime_import_sg_table(dev, attach, sgt);
	if (IS_ERR(obj)) {
		dma_buf_vunmap_unlocked(attach->dmabuf, &map);
		return obj;
	}

	dma_obj = to_drm_gem_dma_obj(obj);
	dma_obj->vaddr = map.vaddr;

	return obj;
}
EXPORT_SYMBOL(drm_gem_dma_prime_import_sg_table_vmap);

MODULE_DESCRIPTION("DRM DMA memory-management helpers");
MODULE_IMPORT_NS(DMA_BUF);
MODULE_LICENSE("GPL");
