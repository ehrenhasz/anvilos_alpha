
 

#include <linux/dma-buf.h>
#include <linux/export.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/shmem_fs.h>
#include <linux/slab.h>
#include <linux/vmalloc.h>
#include <linux/module.h>

#ifdef CONFIG_X86
#include <asm/set_memory.h>
#endif

#include <drm/drm.h>
#include <drm/drm_device.h>
#include <drm/drm_drv.h>
#include <drm/drm_gem_shmem_helper.h>
#include <drm/drm_prime.h>
#include <drm/drm_print.h>

MODULE_IMPORT_NS(DMA_BUF);

 

static const struct drm_gem_object_funcs drm_gem_shmem_funcs = {
	.free = drm_gem_shmem_object_free,
	.print_info = drm_gem_shmem_object_print_info,
	.pin = drm_gem_shmem_object_pin,
	.unpin = drm_gem_shmem_object_unpin,
	.get_sg_table = drm_gem_shmem_object_get_sg_table,
	.vmap = drm_gem_shmem_object_vmap,
	.vunmap = drm_gem_shmem_object_vunmap,
	.mmap = drm_gem_shmem_object_mmap,
	.vm_ops = &drm_gem_shmem_vm_ops,
};

static struct drm_gem_shmem_object *
__drm_gem_shmem_create(struct drm_device *dev, size_t size, bool private)
{
	struct drm_gem_shmem_object *shmem;
	struct drm_gem_object *obj;
	int ret = 0;

	size = PAGE_ALIGN(size);

	if (dev->driver->gem_create_object) {
		obj = dev->driver->gem_create_object(dev, size);
		if (IS_ERR(obj))
			return ERR_CAST(obj);
		shmem = to_drm_gem_shmem_obj(obj);
	} else {
		shmem = kzalloc(sizeof(*shmem), GFP_KERNEL);
		if (!shmem)
			return ERR_PTR(-ENOMEM);
		obj = &shmem->base;
	}

	if (!obj->funcs)
		obj->funcs = &drm_gem_shmem_funcs;

	if (private) {
		drm_gem_private_object_init(dev, obj, size);
		shmem->map_wc = false;  
	} else {
		ret = drm_gem_object_init(dev, obj, size);
	}
	if (ret) {
		drm_gem_private_object_fini(obj);
		goto err_free;
	}

	ret = drm_gem_create_mmap_offset(obj);
	if (ret)
		goto err_release;

	INIT_LIST_HEAD(&shmem->madv_list);

	if (!private) {
		 
		mapping_set_gfp_mask(obj->filp->f_mapping, GFP_HIGHUSER |
				     __GFP_RETRY_MAYFAIL | __GFP_NOWARN);
	}

	return shmem;

err_release:
	drm_gem_object_release(obj);
err_free:
	kfree(obj);

	return ERR_PTR(ret);
}
 
struct drm_gem_shmem_object *drm_gem_shmem_create(struct drm_device *dev, size_t size)
{
	return __drm_gem_shmem_create(dev, size, false);
}
EXPORT_SYMBOL_GPL(drm_gem_shmem_create);

 
void drm_gem_shmem_free(struct drm_gem_shmem_object *shmem)
{
	struct drm_gem_object *obj = &shmem->base;

	if (obj->import_attach) {
		drm_prime_gem_destroy(obj, shmem->sgt);
	} else {
		dma_resv_lock(shmem->base.resv, NULL);

		drm_WARN_ON(obj->dev, shmem->vmap_use_count);

		if (shmem->sgt) {
			dma_unmap_sgtable(obj->dev->dev, shmem->sgt,
					  DMA_BIDIRECTIONAL, 0);
			sg_free_table(shmem->sgt);
			kfree(shmem->sgt);
		}
		if (shmem->pages)
			drm_gem_shmem_put_pages(shmem);

		drm_WARN_ON(obj->dev, shmem->pages_use_count);

		dma_resv_unlock(shmem->base.resv);
	}

	drm_gem_object_release(obj);
	kfree(shmem);
}
EXPORT_SYMBOL_GPL(drm_gem_shmem_free);

static int drm_gem_shmem_get_pages(struct drm_gem_shmem_object *shmem)
{
	struct drm_gem_object *obj = &shmem->base;
	struct page **pages;

	dma_resv_assert_held(shmem->base.resv);

	if (shmem->pages_use_count++ > 0)
		return 0;

	pages = drm_gem_get_pages(obj);
	if (IS_ERR(pages)) {
		drm_dbg_kms(obj->dev, "Failed to get pages (%ld)\n",
			    PTR_ERR(pages));
		shmem->pages_use_count = 0;
		return PTR_ERR(pages);
	}

	 
#ifdef CONFIG_X86
	if (shmem->map_wc)
		set_pages_array_wc(pages, obj->size >> PAGE_SHIFT);
#endif

	shmem->pages = pages;

	return 0;
}

 
void drm_gem_shmem_put_pages(struct drm_gem_shmem_object *shmem)
{
	struct drm_gem_object *obj = &shmem->base;

	dma_resv_assert_held(shmem->base.resv);

	if (drm_WARN_ON_ONCE(obj->dev, !shmem->pages_use_count))
		return;

	if (--shmem->pages_use_count > 0)
		return;

#ifdef CONFIG_X86
	if (shmem->map_wc)
		set_pages_array_wb(shmem->pages, obj->size >> PAGE_SHIFT);
#endif

	drm_gem_put_pages(obj, shmem->pages,
			  shmem->pages_mark_dirty_on_put,
			  shmem->pages_mark_accessed_on_put);
	shmem->pages = NULL;
}
EXPORT_SYMBOL(drm_gem_shmem_put_pages);

static int drm_gem_shmem_pin_locked(struct drm_gem_shmem_object *shmem)
{
	int ret;

	dma_resv_assert_held(shmem->base.resv);

	ret = drm_gem_shmem_get_pages(shmem);

	return ret;
}

static void drm_gem_shmem_unpin_locked(struct drm_gem_shmem_object *shmem)
{
	dma_resv_assert_held(shmem->base.resv);

	drm_gem_shmem_put_pages(shmem);
}

 
int drm_gem_shmem_pin(struct drm_gem_shmem_object *shmem)
{
	struct drm_gem_object *obj = &shmem->base;
	int ret;

	drm_WARN_ON(obj->dev, obj->import_attach);

	ret = dma_resv_lock_interruptible(shmem->base.resv, NULL);
	if (ret)
		return ret;
	ret = drm_gem_shmem_pin_locked(shmem);
	dma_resv_unlock(shmem->base.resv);

	return ret;
}
EXPORT_SYMBOL(drm_gem_shmem_pin);

 
void drm_gem_shmem_unpin(struct drm_gem_shmem_object *shmem)
{
	struct drm_gem_object *obj = &shmem->base;

	drm_WARN_ON(obj->dev, obj->import_attach);

	dma_resv_lock(shmem->base.resv, NULL);
	drm_gem_shmem_unpin_locked(shmem);
	dma_resv_unlock(shmem->base.resv);
}
EXPORT_SYMBOL(drm_gem_shmem_unpin);

 
int drm_gem_shmem_vmap(struct drm_gem_shmem_object *shmem,
		       struct iosys_map *map)
{
	struct drm_gem_object *obj = &shmem->base;
	int ret = 0;

	if (obj->import_attach) {
		ret = dma_buf_vmap(obj->import_attach->dmabuf, map);
		if (!ret) {
			if (drm_WARN_ON(obj->dev, map->is_iomem)) {
				dma_buf_vunmap(obj->import_attach->dmabuf, map);
				return -EIO;
			}
		}
	} else {
		pgprot_t prot = PAGE_KERNEL;

		dma_resv_assert_held(shmem->base.resv);

		if (shmem->vmap_use_count++ > 0) {
			iosys_map_set_vaddr(map, shmem->vaddr);
			return 0;
		}

		ret = drm_gem_shmem_get_pages(shmem);
		if (ret)
			goto err_zero_use;

		if (shmem->map_wc)
			prot = pgprot_writecombine(prot);
		shmem->vaddr = vmap(shmem->pages, obj->size >> PAGE_SHIFT,
				    VM_MAP, prot);
		if (!shmem->vaddr)
			ret = -ENOMEM;
		else
			iosys_map_set_vaddr(map, shmem->vaddr);
	}

	if (ret) {
		drm_dbg_kms(obj->dev, "Failed to vmap pages, error %d\n", ret);
		goto err_put_pages;
	}

	return 0;

err_put_pages:
	if (!obj->import_attach)
		drm_gem_shmem_put_pages(shmem);
err_zero_use:
	shmem->vmap_use_count = 0;

	return ret;
}
EXPORT_SYMBOL(drm_gem_shmem_vmap);

 
void drm_gem_shmem_vunmap(struct drm_gem_shmem_object *shmem,
			  struct iosys_map *map)
{
	struct drm_gem_object *obj = &shmem->base;

	if (obj->import_attach) {
		dma_buf_vunmap(obj->import_attach->dmabuf, map);
	} else {
		dma_resv_assert_held(shmem->base.resv);

		if (drm_WARN_ON_ONCE(obj->dev, !shmem->vmap_use_count))
			return;

		if (--shmem->vmap_use_count > 0)
			return;

		vunmap(shmem->vaddr);
		drm_gem_shmem_put_pages(shmem);
	}

	shmem->vaddr = NULL;
}
EXPORT_SYMBOL(drm_gem_shmem_vunmap);

static int
drm_gem_shmem_create_with_handle(struct drm_file *file_priv,
				 struct drm_device *dev, size_t size,
				 uint32_t *handle)
{
	struct drm_gem_shmem_object *shmem;
	int ret;

	shmem = drm_gem_shmem_create(dev, size);
	if (IS_ERR(shmem))
		return PTR_ERR(shmem);

	 
	ret = drm_gem_handle_create(file_priv, &shmem->base, handle);
	 
	drm_gem_object_put(&shmem->base);

	return ret;
}

 
int drm_gem_shmem_madvise(struct drm_gem_shmem_object *shmem, int madv)
{
	dma_resv_assert_held(shmem->base.resv);

	if (shmem->madv >= 0)
		shmem->madv = madv;

	madv = shmem->madv;

	return (madv >= 0);
}
EXPORT_SYMBOL(drm_gem_shmem_madvise);

void drm_gem_shmem_purge(struct drm_gem_shmem_object *shmem)
{
	struct drm_gem_object *obj = &shmem->base;
	struct drm_device *dev = obj->dev;

	dma_resv_assert_held(shmem->base.resv);

	drm_WARN_ON(obj->dev, !drm_gem_shmem_is_purgeable(shmem));

	dma_unmap_sgtable(dev->dev, shmem->sgt, DMA_BIDIRECTIONAL, 0);
	sg_free_table(shmem->sgt);
	kfree(shmem->sgt);
	shmem->sgt = NULL;

	drm_gem_shmem_put_pages(shmem);

	shmem->madv = -1;

	drm_vma_node_unmap(&obj->vma_node, dev->anon_inode->i_mapping);
	drm_gem_free_mmap_offset(obj);

	 
	shmem_truncate_range(file_inode(obj->filp), 0, (loff_t)-1);

	invalidate_mapping_pages(file_inode(obj->filp)->i_mapping, 0, (loff_t)-1);
}
EXPORT_SYMBOL(drm_gem_shmem_purge);

 
int drm_gem_shmem_dumb_create(struct drm_file *file, struct drm_device *dev,
			      struct drm_mode_create_dumb *args)
{
	u32 min_pitch = DIV_ROUND_UP(args->width * args->bpp, 8);

	if (!args->pitch || !args->size) {
		args->pitch = min_pitch;
		args->size = PAGE_ALIGN(args->pitch * args->height);
	} else {
		 
		if (args->pitch < min_pitch)
			args->pitch = min_pitch;
		if (args->size < args->pitch * args->height)
			args->size = PAGE_ALIGN(args->pitch * args->height);
	}

	return drm_gem_shmem_create_with_handle(file, dev, args->size, &args->handle);
}
EXPORT_SYMBOL_GPL(drm_gem_shmem_dumb_create);

static vm_fault_t drm_gem_shmem_fault(struct vm_fault *vmf)
{
	struct vm_area_struct *vma = vmf->vma;
	struct drm_gem_object *obj = vma->vm_private_data;
	struct drm_gem_shmem_object *shmem = to_drm_gem_shmem_obj(obj);
	loff_t num_pages = obj->size >> PAGE_SHIFT;
	vm_fault_t ret;
	struct page *page;
	pgoff_t page_offset;

	 
	page_offset = (vmf->address - vma->vm_start) >> PAGE_SHIFT;

	dma_resv_lock(shmem->base.resv, NULL);

	if (page_offset >= num_pages ||
	    drm_WARN_ON_ONCE(obj->dev, !shmem->pages) ||
	    shmem->madv < 0) {
		ret = VM_FAULT_SIGBUS;
	} else {
		page = shmem->pages[page_offset];

		ret = vmf_insert_pfn(vma, vmf->address, page_to_pfn(page));
	}

	dma_resv_unlock(shmem->base.resv);

	return ret;
}

static void drm_gem_shmem_vm_open(struct vm_area_struct *vma)
{
	struct drm_gem_object *obj = vma->vm_private_data;
	struct drm_gem_shmem_object *shmem = to_drm_gem_shmem_obj(obj);

	drm_WARN_ON(obj->dev, obj->import_attach);

	dma_resv_lock(shmem->base.resv, NULL);

	 
	if (!drm_WARN_ON_ONCE(obj->dev, !shmem->pages_use_count))
		shmem->pages_use_count++;

	dma_resv_unlock(shmem->base.resv);

	drm_gem_vm_open(vma);
}

static void drm_gem_shmem_vm_close(struct vm_area_struct *vma)
{
	struct drm_gem_object *obj = vma->vm_private_data;
	struct drm_gem_shmem_object *shmem = to_drm_gem_shmem_obj(obj);

	dma_resv_lock(shmem->base.resv, NULL);
	drm_gem_shmem_put_pages(shmem);
	dma_resv_unlock(shmem->base.resv);

	drm_gem_vm_close(vma);
}

const struct vm_operations_struct drm_gem_shmem_vm_ops = {
	.fault = drm_gem_shmem_fault,
	.open = drm_gem_shmem_vm_open,
	.close = drm_gem_shmem_vm_close,
};
EXPORT_SYMBOL_GPL(drm_gem_shmem_vm_ops);

 
int drm_gem_shmem_mmap(struct drm_gem_shmem_object *shmem, struct vm_area_struct *vma)
{
	struct drm_gem_object *obj = &shmem->base;
	int ret;

	if (obj->import_attach) {
		 
		vma->vm_private_data = NULL;
		vma->vm_ops = NULL;

		ret = dma_buf_mmap(obj->dma_buf, vma, 0);

		 
		if (!ret)
			drm_gem_object_put(obj);

		return ret;
	}

	dma_resv_lock(shmem->base.resv, NULL);
	ret = drm_gem_shmem_get_pages(shmem);
	dma_resv_unlock(shmem->base.resv);

	if (ret)
		return ret;

	vm_flags_set(vma, VM_PFNMAP | VM_DONTEXPAND | VM_DONTDUMP);
	vma->vm_page_prot = vm_get_page_prot(vma->vm_flags);
	if (shmem->map_wc)
		vma->vm_page_prot = pgprot_writecombine(vma->vm_page_prot);

	return 0;
}
EXPORT_SYMBOL_GPL(drm_gem_shmem_mmap);

 
void drm_gem_shmem_print_info(const struct drm_gem_shmem_object *shmem,
			      struct drm_printer *p, unsigned int indent)
{
	if (shmem->base.import_attach)
		return;

	drm_printf_indent(p, indent, "pages_use_count=%u\n", shmem->pages_use_count);
	drm_printf_indent(p, indent, "vmap_use_count=%u\n", shmem->vmap_use_count);
	drm_printf_indent(p, indent, "vaddr=%p\n", shmem->vaddr);
}
EXPORT_SYMBOL(drm_gem_shmem_print_info);

 
struct sg_table *drm_gem_shmem_get_sg_table(struct drm_gem_shmem_object *shmem)
{
	struct drm_gem_object *obj = &shmem->base;

	drm_WARN_ON(obj->dev, obj->import_attach);

	return drm_prime_pages_to_sg(obj->dev, shmem->pages, obj->size >> PAGE_SHIFT);
}
EXPORT_SYMBOL_GPL(drm_gem_shmem_get_sg_table);

static struct sg_table *drm_gem_shmem_get_pages_sgt_locked(struct drm_gem_shmem_object *shmem)
{
	struct drm_gem_object *obj = &shmem->base;
	int ret;
	struct sg_table *sgt;

	if (shmem->sgt)
		return shmem->sgt;

	drm_WARN_ON(obj->dev, obj->import_attach);

	ret = drm_gem_shmem_get_pages(shmem);
	if (ret)
		return ERR_PTR(ret);

	sgt = drm_gem_shmem_get_sg_table(shmem);
	if (IS_ERR(sgt)) {
		ret = PTR_ERR(sgt);
		goto err_put_pages;
	}
	 
	ret = dma_map_sgtable(obj->dev->dev, sgt, DMA_BIDIRECTIONAL, 0);
	if (ret)
		goto err_free_sgt;

	shmem->sgt = sgt;

	return sgt;

err_free_sgt:
	sg_free_table(sgt);
	kfree(sgt);
err_put_pages:
	drm_gem_shmem_put_pages(shmem);
	return ERR_PTR(ret);
}

 
struct sg_table *drm_gem_shmem_get_pages_sgt(struct drm_gem_shmem_object *shmem)
{
	int ret;
	struct sg_table *sgt;

	ret = dma_resv_lock_interruptible(shmem->base.resv, NULL);
	if (ret)
		return ERR_PTR(ret);
	sgt = drm_gem_shmem_get_pages_sgt_locked(shmem);
	dma_resv_unlock(shmem->base.resv);

	return sgt;
}
EXPORT_SYMBOL_GPL(drm_gem_shmem_get_pages_sgt);

 
struct drm_gem_object *
drm_gem_shmem_prime_import_sg_table(struct drm_device *dev,
				    struct dma_buf_attachment *attach,
				    struct sg_table *sgt)
{
	size_t size = PAGE_ALIGN(attach->dmabuf->size);
	struct drm_gem_shmem_object *shmem;

	shmem = __drm_gem_shmem_create(dev, size, true);
	if (IS_ERR(shmem))
		return ERR_CAST(shmem);

	shmem->sgt = sgt;

	drm_dbg_prime(dev, "size = %zu\n", size);

	return &shmem->base;
}
EXPORT_SYMBOL_GPL(drm_gem_shmem_prime_import_sg_table);

MODULE_DESCRIPTION("DRM SHMEM memory-management helpers");
MODULE_IMPORT_NS(DMA_BUF);
MODULE_LICENSE("GPL v2");
