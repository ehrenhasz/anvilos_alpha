 

#include <linux/dma-buf.h>
#include <linux/file.h>
#include <linux/fs.h>
#include <linux/iosys-map.h>
#include <linux/mem_encrypt.h>
#include <linux/mm.h>
#include <linux/mman.h>
#include <linux/module.h>
#include <linux/pagemap.h>
#include <linux/pagevec.h>
#include <linux/shmem_fs.h>
#include <linux/slab.h>
#include <linux/string_helpers.h>
#include <linux/types.h>
#include <linux/uaccess.h>

#include <drm/drm.h>
#include <drm/drm_device.h>
#include <drm/drm_drv.h>
#include <drm/drm_file.h>
#include <drm/drm_gem.h>
#include <drm/drm_managed.h>
#include <drm/drm_print.h>
#include <drm/drm_vma_manager.h>

#include "drm_internal.h"

 

static void
drm_gem_init_release(struct drm_device *dev, void *ptr)
{
	drm_vma_offset_manager_destroy(dev->vma_offset_manager);
}

 
int
drm_gem_init(struct drm_device *dev)
{
	struct drm_vma_offset_manager *vma_offset_manager;

	mutex_init(&dev->object_name_lock);
	idr_init_base(&dev->object_name_idr, 1);

	vma_offset_manager = drmm_kzalloc(dev, sizeof(*vma_offset_manager),
					  GFP_KERNEL);
	if (!vma_offset_manager) {
		DRM_ERROR("out of memory\n");
		return -ENOMEM;
	}

	dev->vma_offset_manager = vma_offset_manager;
	drm_vma_offset_manager_init(vma_offset_manager,
				    DRM_FILE_PAGE_OFFSET_START,
				    DRM_FILE_PAGE_OFFSET_SIZE);

	return drmm_add_action(dev, drm_gem_init_release, NULL);
}

 
int drm_gem_object_init(struct drm_device *dev,
			struct drm_gem_object *obj, size_t size)
{
	struct file *filp;

	drm_gem_private_object_init(dev, obj, size);

	filp = shmem_file_setup("drm mm object", size, VM_NORESERVE);
	if (IS_ERR(filp))
		return PTR_ERR(filp);

	obj->filp = filp;

	return 0;
}
EXPORT_SYMBOL(drm_gem_object_init);

 
void drm_gem_private_object_init(struct drm_device *dev,
				 struct drm_gem_object *obj, size_t size)
{
	BUG_ON((size & (PAGE_SIZE - 1)) != 0);

	obj->dev = dev;
	obj->filp = NULL;

	kref_init(&obj->refcount);
	obj->handle_count = 0;
	obj->size = size;
	dma_resv_init(&obj->_resv);
	if (!obj->resv)
		obj->resv = &obj->_resv;

	if (drm_core_check_feature(dev, DRIVER_GEM_GPUVA))
		drm_gem_gpuva_init(obj);

	drm_vma_node_reset(&obj->vma_node);
	INIT_LIST_HEAD(&obj->lru_node);
}
EXPORT_SYMBOL(drm_gem_private_object_init);

 
void drm_gem_private_object_fini(struct drm_gem_object *obj)
{
	WARN_ON(obj->dma_buf);

	dma_resv_fini(&obj->_resv);
}
EXPORT_SYMBOL(drm_gem_private_object_fini);

 
static void drm_gem_object_handle_free(struct drm_gem_object *obj)
{
	struct drm_device *dev = obj->dev;

	 
	if (obj->name) {
		idr_remove(&dev->object_name_idr, obj->name);
		obj->name = 0;
	}
}

static void drm_gem_object_exported_dma_buf_free(struct drm_gem_object *obj)
{
	 
	if (obj->dma_buf) {
		dma_buf_put(obj->dma_buf);
		obj->dma_buf = NULL;
	}
}

static void
drm_gem_object_handle_put_unlocked(struct drm_gem_object *obj)
{
	struct drm_device *dev = obj->dev;
	bool final = false;

	if (WARN_ON(READ_ONCE(obj->handle_count) == 0))
		return;

	 

	mutex_lock(&dev->object_name_lock);
	if (--obj->handle_count == 0) {
		drm_gem_object_handle_free(obj);
		drm_gem_object_exported_dma_buf_free(obj);
		final = true;
	}
	mutex_unlock(&dev->object_name_lock);

	if (final)
		drm_gem_object_put(obj);
}

 
static int
drm_gem_object_release_handle(int id, void *ptr, void *data)
{
	struct drm_file *file_priv = data;
	struct drm_gem_object *obj = ptr;

	if (obj->funcs->close)
		obj->funcs->close(obj, file_priv);

	drm_prime_remove_buf_handle(&file_priv->prime, id);
	drm_vma_node_revoke(&obj->vma_node, file_priv);

	drm_gem_object_handle_put_unlocked(obj);

	return 0;
}

 
int
drm_gem_handle_delete(struct drm_file *filp, u32 handle)
{
	struct drm_gem_object *obj;

	spin_lock(&filp->table_lock);

	 
	obj = idr_replace(&filp->object_idr, NULL, handle);
	spin_unlock(&filp->table_lock);
	if (IS_ERR_OR_NULL(obj))
		return -EINVAL;

	 
	drm_gem_object_release_handle(handle, obj, filp);

	 
	spin_lock(&filp->table_lock);
	idr_remove(&filp->object_idr, handle);
	spin_unlock(&filp->table_lock);

	return 0;
}
EXPORT_SYMBOL(drm_gem_handle_delete);

 
int drm_gem_dumb_map_offset(struct drm_file *file, struct drm_device *dev,
			    u32 handle, u64 *offset)
{
	struct drm_gem_object *obj;
	int ret;

	obj = drm_gem_object_lookup(file, handle);
	if (!obj)
		return -ENOENT;

	 
	if (obj->import_attach) {
		ret = -EINVAL;
		goto out;
	}

	ret = drm_gem_create_mmap_offset(obj);
	if (ret)
		goto out;

	*offset = drm_vma_node_offset_addr(&obj->vma_node);
out:
	drm_gem_object_put(obj);

	return ret;
}
EXPORT_SYMBOL_GPL(drm_gem_dumb_map_offset);

 
int
drm_gem_handle_create_tail(struct drm_file *file_priv,
			   struct drm_gem_object *obj,
			   u32 *handlep)
{
	struct drm_device *dev = obj->dev;
	u32 handle;
	int ret;

	WARN_ON(!mutex_is_locked(&dev->object_name_lock));
	if (obj->handle_count++ == 0)
		drm_gem_object_get(obj);

	 
	idr_preload(GFP_KERNEL);
	spin_lock(&file_priv->table_lock);

	ret = idr_alloc(&file_priv->object_idr, obj, 1, 0, GFP_NOWAIT);

	spin_unlock(&file_priv->table_lock);
	idr_preload_end();

	mutex_unlock(&dev->object_name_lock);
	if (ret < 0)
		goto err_unref;

	handle = ret;

	ret = drm_vma_node_allow(&obj->vma_node, file_priv);
	if (ret)
		goto err_remove;

	if (obj->funcs->open) {
		ret = obj->funcs->open(obj, file_priv);
		if (ret)
			goto err_revoke;
	}

	*handlep = handle;
	return 0;

err_revoke:
	drm_vma_node_revoke(&obj->vma_node, file_priv);
err_remove:
	spin_lock(&file_priv->table_lock);
	idr_remove(&file_priv->object_idr, handle);
	spin_unlock(&file_priv->table_lock);
err_unref:
	drm_gem_object_handle_put_unlocked(obj);
	return ret;
}

 
int drm_gem_handle_create(struct drm_file *file_priv,
			  struct drm_gem_object *obj,
			  u32 *handlep)
{
	mutex_lock(&obj->dev->object_name_lock);

	return drm_gem_handle_create_tail(file_priv, obj, handlep);
}
EXPORT_SYMBOL(drm_gem_handle_create);


 
void
drm_gem_free_mmap_offset(struct drm_gem_object *obj)
{
	struct drm_device *dev = obj->dev;

	drm_vma_offset_remove(dev->vma_offset_manager, &obj->vma_node);
}
EXPORT_SYMBOL(drm_gem_free_mmap_offset);

 
int
drm_gem_create_mmap_offset_size(struct drm_gem_object *obj, size_t size)
{
	struct drm_device *dev = obj->dev;

	return drm_vma_offset_add(dev->vma_offset_manager, &obj->vma_node,
				  size / PAGE_SIZE);
}
EXPORT_SYMBOL(drm_gem_create_mmap_offset_size);

 
int drm_gem_create_mmap_offset(struct drm_gem_object *obj)
{
	return drm_gem_create_mmap_offset_size(obj, obj->size);
}
EXPORT_SYMBOL(drm_gem_create_mmap_offset);

 
static void drm_gem_check_release_batch(struct folio_batch *fbatch)
{
	check_move_unevictable_folios(fbatch);
	__folio_batch_release(fbatch);
	cond_resched();
}

 
struct page **drm_gem_get_pages(struct drm_gem_object *obj)
{
	struct address_space *mapping;
	struct page **pages;
	struct folio *folio;
	struct folio_batch fbatch;
	long i, j, npages;

	if (WARN_ON(!obj->filp))
		return ERR_PTR(-EINVAL);

	 
	mapping = obj->filp->f_mapping;

	 
	WARN_ON((obj->size & (PAGE_SIZE - 1)) != 0);

	npages = obj->size >> PAGE_SHIFT;

	pages = kvmalloc_array(npages, sizeof(struct page *), GFP_KERNEL);
	if (pages == NULL)
		return ERR_PTR(-ENOMEM);

	mapping_set_unevictable(mapping);

	i = 0;
	while (i < npages) {
		long nr;
		folio = shmem_read_folio_gfp(mapping, i,
				mapping_gfp_mask(mapping));
		if (IS_ERR(folio))
			goto fail;
		nr = min(npages - i, folio_nr_pages(folio));
		for (j = 0; j < nr; j++, i++)
			pages[i] = folio_file_page(folio, i);

		 
		BUG_ON(mapping_gfp_constraint(mapping, __GFP_DMA32) &&
				(folio_pfn(folio) >= 0x00100000UL));
	}

	return pages;

fail:
	mapping_clear_unevictable(mapping);
	folio_batch_init(&fbatch);
	j = 0;
	while (j < i) {
		struct folio *f = page_folio(pages[j]);
		if (!folio_batch_add(&fbatch, f))
			drm_gem_check_release_batch(&fbatch);
		j += folio_nr_pages(f);
	}
	if (fbatch.nr)
		drm_gem_check_release_batch(&fbatch);

	kvfree(pages);
	return ERR_CAST(folio);
}
EXPORT_SYMBOL(drm_gem_get_pages);

 
void drm_gem_put_pages(struct drm_gem_object *obj, struct page **pages,
		bool dirty, bool accessed)
{
	int i, npages;
	struct address_space *mapping;
	struct folio_batch fbatch;

	mapping = file_inode(obj->filp)->i_mapping;
	mapping_clear_unevictable(mapping);

	 
	WARN_ON((obj->size & (PAGE_SIZE - 1)) != 0);

	npages = obj->size >> PAGE_SHIFT;

	folio_batch_init(&fbatch);
	for (i = 0; i < npages; i++) {
		struct folio *folio;

		if (!pages[i])
			continue;
		folio = page_folio(pages[i]);

		if (dirty)
			folio_mark_dirty(folio);

		if (accessed)
			folio_mark_accessed(folio);

		 
		if (!folio_batch_add(&fbatch, folio))
			drm_gem_check_release_batch(&fbatch);
		i += folio_nr_pages(folio) - 1;
	}
	if (folio_batch_count(&fbatch))
		drm_gem_check_release_batch(&fbatch);

	kvfree(pages);
}
EXPORT_SYMBOL(drm_gem_put_pages);

static int objects_lookup(struct drm_file *filp, u32 *handle, int count,
			  struct drm_gem_object **objs)
{
	int i, ret = 0;
	struct drm_gem_object *obj;

	spin_lock(&filp->table_lock);

	for (i = 0; i < count; i++) {
		 
		obj = idr_find(&filp->object_idr, handle[i]);
		if (!obj) {
			ret = -ENOENT;
			break;
		}
		drm_gem_object_get(obj);
		objs[i] = obj;
	}
	spin_unlock(&filp->table_lock);

	return ret;
}

 
int drm_gem_objects_lookup(struct drm_file *filp, void __user *bo_handles,
			   int count, struct drm_gem_object ***objs_out)
{
	int ret;
	u32 *handles;
	struct drm_gem_object **objs;

	if (!count)
		return 0;

	objs = kvmalloc_array(count, sizeof(struct drm_gem_object *),
			     GFP_KERNEL | __GFP_ZERO);
	if (!objs)
		return -ENOMEM;

	*objs_out = objs;

	handles = kvmalloc_array(count, sizeof(u32), GFP_KERNEL);
	if (!handles) {
		ret = -ENOMEM;
		goto out;
	}

	if (copy_from_user(handles, bo_handles, count * sizeof(u32))) {
		ret = -EFAULT;
		DRM_DEBUG("Failed to copy in GEM handles\n");
		goto out;
	}

	ret = objects_lookup(filp, handles, count, objs);
out:
	kvfree(handles);
	return ret;

}
EXPORT_SYMBOL(drm_gem_objects_lookup);

 
struct drm_gem_object *
drm_gem_object_lookup(struct drm_file *filp, u32 handle)
{
	struct drm_gem_object *obj = NULL;

	objects_lookup(filp, &handle, 1, &obj);
	return obj;
}
EXPORT_SYMBOL(drm_gem_object_lookup);

 
long drm_gem_dma_resv_wait(struct drm_file *filep, u32 handle,
				    bool wait_all, unsigned long timeout)
{
	long ret;
	struct drm_gem_object *obj;

	obj = drm_gem_object_lookup(filep, handle);
	if (!obj) {
		DRM_DEBUG("Failed to look up GEM BO %d\n", handle);
		return -EINVAL;
	}

	ret = dma_resv_wait_timeout(obj->resv, dma_resv_usage_rw(wait_all),
				    true, timeout);
	if (ret == 0)
		ret = -ETIME;
	else if (ret > 0)
		ret = 0;

	drm_gem_object_put(obj);

	return ret;
}
EXPORT_SYMBOL(drm_gem_dma_resv_wait);

 
int
drm_gem_close_ioctl(struct drm_device *dev, void *data,
		    struct drm_file *file_priv)
{
	struct drm_gem_close *args = data;
	int ret;

	if (!drm_core_check_feature(dev, DRIVER_GEM))
		return -EOPNOTSUPP;

	ret = drm_gem_handle_delete(file_priv, args->handle);

	return ret;
}

 
int
drm_gem_flink_ioctl(struct drm_device *dev, void *data,
		    struct drm_file *file_priv)
{
	struct drm_gem_flink *args = data;
	struct drm_gem_object *obj;
	int ret;

	if (!drm_core_check_feature(dev, DRIVER_GEM))
		return -EOPNOTSUPP;

	obj = drm_gem_object_lookup(file_priv, args->handle);
	if (obj == NULL)
		return -ENOENT;

	mutex_lock(&dev->object_name_lock);
	 
	if (obj->handle_count == 0) {
		ret = -ENOENT;
		goto err;
	}

	if (!obj->name) {
		ret = idr_alloc(&dev->object_name_idr, obj, 1, 0, GFP_KERNEL);
		if (ret < 0)
			goto err;

		obj->name = ret;
	}

	args->name = (uint64_t) obj->name;
	ret = 0;

err:
	mutex_unlock(&dev->object_name_lock);
	drm_gem_object_put(obj);
	return ret;
}

 
int
drm_gem_open_ioctl(struct drm_device *dev, void *data,
		   struct drm_file *file_priv)
{
	struct drm_gem_open *args = data;
	struct drm_gem_object *obj;
	int ret;
	u32 handle;

	if (!drm_core_check_feature(dev, DRIVER_GEM))
		return -EOPNOTSUPP;

	mutex_lock(&dev->object_name_lock);
	obj = idr_find(&dev->object_name_idr, (int) args->name);
	if (obj) {
		drm_gem_object_get(obj);
	} else {
		mutex_unlock(&dev->object_name_lock);
		return -ENOENT;
	}

	 
	ret = drm_gem_handle_create_tail(file_priv, obj, &handle);
	if (ret)
		goto err;

	args->handle = handle;
	args->size = obj->size;

err:
	drm_gem_object_put(obj);
	return ret;
}

 
void
drm_gem_open(struct drm_device *dev, struct drm_file *file_private)
{
	idr_init_base(&file_private->object_idr, 1);
	spin_lock_init(&file_private->table_lock);
}

 
void
drm_gem_release(struct drm_device *dev, struct drm_file *file_private)
{
	idr_for_each(&file_private->object_idr,
		     &drm_gem_object_release_handle, file_private);
	idr_destroy(&file_private->object_idr);
}

 
void
drm_gem_object_release(struct drm_gem_object *obj)
{
	if (obj->filp)
		fput(obj->filp);

	drm_gem_private_object_fini(obj);

	drm_gem_free_mmap_offset(obj);
	drm_gem_lru_remove(obj);
}
EXPORT_SYMBOL(drm_gem_object_release);

 
void
drm_gem_object_free(struct kref *kref)
{
	struct drm_gem_object *obj =
		container_of(kref, struct drm_gem_object, refcount);

	if (WARN_ON(!obj->funcs->free))
		return;

	obj->funcs->free(obj);
}
EXPORT_SYMBOL(drm_gem_object_free);

 
void drm_gem_vm_open(struct vm_area_struct *vma)
{
	struct drm_gem_object *obj = vma->vm_private_data;

	drm_gem_object_get(obj);
}
EXPORT_SYMBOL(drm_gem_vm_open);

 
void drm_gem_vm_close(struct vm_area_struct *vma)
{
	struct drm_gem_object *obj = vma->vm_private_data;

	drm_gem_object_put(obj);
}
EXPORT_SYMBOL(drm_gem_vm_close);

 
int drm_gem_mmap_obj(struct drm_gem_object *obj, unsigned long obj_size,
		     struct vm_area_struct *vma)
{
	int ret;

	 
	if (obj_size < vma->vm_end - vma->vm_start)
		return -EINVAL;

	 
	drm_gem_object_get(obj);

	vma->vm_private_data = obj;
	vma->vm_ops = obj->funcs->vm_ops;

	if (obj->funcs->mmap) {
		ret = obj->funcs->mmap(obj, vma);
		if (ret)
			goto err_drm_gem_object_put;
		WARN_ON(!(vma->vm_flags & VM_DONTEXPAND));
	} else {
		if (!vma->vm_ops) {
			ret = -EINVAL;
			goto err_drm_gem_object_put;
		}

		vm_flags_set(vma, VM_IO | VM_PFNMAP | VM_DONTEXPAND | VM_DONTDUMP);
		vma->vm_page_prot = pgprot_writecombine(vm_get_page_prot(vma->vm_flags));
		vma->vm_page_prot = pgprot_decrypted(vma->vm_page_prot);
	}

	return 0;

err_drm_gem_object_put:
	drm_gem_object_put(obj);
	return ret;
}
EXPORT_SYMBOL(drm_gem_mmap_obj);

 
int drm_gem_mmap(struct file *filp, struct vm_area_struct *vma)
{
	struct drm_file *priv = filp->private_data;
	struct drm_device *dev = priv->minor->dev;
	struct drm_gem_object *obj = NULL;
	struct drm_vma_offset_node *node;
	int ret;

	if (drm_dev_is_unplugged(dev))
		return -ENODEV;

	drm_vma_offset_lock_lookup(dev->vma_offset_manager);
	node = drm_vma_offset_exact_lookup_locked(dev->vma_offset_manager,
						  vma->vm_pgoff,
						  vma_pages(vma));
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

	ret = drm_gem_mmap_obj(obj, drm_vma_node_size(node) << PAGE_SHIFT,
			       vma);

	drm_gem_object_put(obj);

	return ret;
}
EXPORT_SYMBOL(drm_gem_mmap);

void drm_gem_print_info(struct drm_printer *p, unsigned int indent,
			const struct drm_gem_object *obj)
{
	drm_printf_indent(p, indent, "name=%d\n", obj->name);
	drm_printf_indent(p, indent, "refcount=%u\n",
			  kref_read(&obj->refcount));
	drm_printf_indent(p, indent, "start=%08lx\n",
			  drm_vma_node_start(&obj->vma_node));
	drm_printf_indent(p, indent, "size=%zu\n", obj->size);
	drm_printf_indent(p, indent, "imported=%s\n",
			  str_yes_no(obj->import_attach));

	if (obj->funcs->print_info)
		obj->funcs->print_info(p, indent, obj);
}

int drm_gem_pin(struct drm_gem_object *obj)
{
	if (obj->funcs->pin)
		return obj->funcs->pin(obj);

	return 0;
}

void drm_gem_unpin(struct drm_gem_object *obj)
{
	if (obj->funcs->unpin)
		obj->funcs->unpin(obj);
}

int drm_gem_vmap(struct drm_gem_object *obj, struct iosys_map *map)
{
	int ret;

	dma_resv_assert_held(obj->resv);

	if (!obj->funcs->vmap)
		return -EOPNOTSUPP;

	ret = obj->funcs->vmap(obj, map);
	if (ret)
		return ret;
	else if (iosys_map_is_null(map))
		return -ENOMEM;

	return 0;
}
EXPORT_SYMBOL(drm_gem_vmap);

void drm_gem_vunmap(struct drm_gem_object *obj, struct iosys_map *map)
{
	dma_resv_assert_held(obj->resv);

	if (iosys_map_is_null(map))
		return;

	if (obj->funcs->vunmap)
		obj->funcs->vunmap(obj, map);

	 
	iosys_map_clear(map);
}
EXPORT_SYMBOL(drm_gem_vunmap);

int drm_gem_vmap_unlocked(struct drm_gem_object *obj, struct iosys_map *map)
{
	int ret;

	dma_resv_lock(obj->resv, NULL);
	ret = drm_gem_vmap(obj, map);
	dma_resv_unlock(obj->resv);

	return ret;
}
EXPORT_SYMBOL(drm_gem_vmap_unlocked);

void drm_gem_vunmap_unlocked(struct drm_gem_object *obj, struct iosys_map *map)
{
	dma_resv_lock(obj->resv, NULL);
	drm_gem_vunmap(obj, map);
	dma_resv_unlock(obj->resv);
}
EXPORT_SYMBOL(drm_gem_vunmap_unlocked);

 
int
drm_gem_lock_reservations(struct drm_gem_object **objs, int count,
			  struct ww_acquire_ctx *acquire_ctx)
{
	int contended = -1;
	int i, ret;

	ww_acquire_init(acquire_ctx, &reservation_ww_class);

retry:
	if (contended != -1) {
		struct drm_gem_object *obj = objs[contended];

		ret = dma_resv_lock_slow_interruptible(obj->resv,
								 acquire_ctx);
		if (ret) {
			ww_acquire_fini(acquire_ctx);
			return ret;
		}
	}

	for (i = 0; i < count; i++) {
		if (i == contended)
			continue;

		ret = dma_resv_lock_interruptible(objs[i]->resv,
							    acquire_ctx);
		if (ret) {
			int j;

			for (j = 0; j < i; j++)
				dma_resv_unlock(objs[j]->resv);

			if (contended != -1 && contended >= i)
				dma_resv_unlock(objs[contended]->resv);

			if (ret == -EDEADLK) {
				contended = i;
				goto retry;
			}

			ww_acquire_fini(acquire_ctx);
			return ret;
		}
	}

	ww_acquire_done(acquire_ctx);

	return 0;
}
EXPORT_SYMBOL(drm_gem_lock_reservations);

void
drm_gem_unlock_reservations(struct drm_gem_object **objs, int count,
			    struct ww_acquire_ctx *acquire_ctx)
{
	int i;

	for (i = 0; i < count; i++)
		dma_resv_unlock(objs[i]->resv);

	ww_acquire_fini(acquire_ctx);
}
EXPORT_SYMBOL(drm_gem_unlock_reservations);

 
void
drm_gem_lru_init(struct drm_gem_lru *lru, struct mutex *lock)
{
	lru->lock = lock;
	lru->count = 0;
	INIT_LIST_HEAD(&lru->list);
}
EXPORT_SYMBOL(drm_gem_lru_init);

static void
drm_gem_lru_remove_locked(struct drm_gem_object *obj)
{
	obj->lru->count -= obj->size >> PAGE_SHIFT;
	WARN_ON(obj->lru->count < 0);
	list_del(&obj->lru_node);
	obj->lru = NULL;
}

 
void
drm_gem_lru_remove(struct drm_gem_object *obj)
{
	struct drm_gem_lru *lru = obj->lru;

	if (!lru)
		return;

	mutex_lock(lru->lock);
	drm_gem_lru_remove_locked(obj);
	mutex_unlock(lru->lock);
}
EXPORT_SYMBOL(drm_gem_lru_remove);

 
void
drm_gem_lru_move_tail_locked(struct drm_gem_lru *lru, struct drm_gem_object *obj)
{
	lockdep_assert_held_once(lru->lock);

	if (obj->lru)
		drm_gem_lru_remove_locked(obj);

	lru->count += obj->size >> PAGE_SHIFT;
	list_add_tail(&obj->lru_node, &lru->list);
	obj->lru = lru;
}
EXPORT_SYMBOL(drm_gem_lru_move_tail_locked);

 
void
drm_gem_lru_move_tail(struct drm_gem_lru *lru, struct drm_gem_object *obj)
{
	mutex_lock(lru->lock);
	drm_gem_lru_move_tail_locked(lru, obj);
	mutex_unlock(lru->lock);
}
EXPORT_SYMBOL(drm_gem_lru_move_tail);

 
unsigned long
drm_gem_lru_scan(struct drm_gem_lru *lru,
		 unsigned int nr_to_scan,
		 unsigned long *remaining,
		 bool (*shrink)(struct drm_gem_object *obj))
{
	struct drm_gem_lru still_in_lru;
	struct drm_gem_object *obj;
	unsigned freed = 0;

	drm_gem_lru_init(&still_in_lru, lru->lock);

	mutex_lock(lru->lock);

	while (freed < nr_to_scan) {
		obj = list_first_entry_or_null(&lru->list, typeof(*obj), lru_node);

		if (!obj)
			break;

		drm_gem_lru_move_tail_locked(&still_in_lru, obj);

		 
		if (!kref_get_unless_zero(&obj->refcount))
			continue;

		 
		mutex_unlock(lru->lock);

		 
		if (!dma_resv_trylock(obj->resv)) {
			*remaining += obj->size >> PAGE_SHIFT;
			goto tail;
		}

		if (shrink(obj)) {
			freed += obj->size >> PAGE_SHIFT;

			 
			WARN_ON(obj->lru == &still_in_lru);
			WARN_ON(obj->lru == lru);
		}

		dma_resv_unlock(obj->resv);

tail:
		drm_gem_object_put(obj);
		mutex_lock(lru->lock);
	}

	 
	list_for_each_entry (obj, &still_in_lru.list, lru_node)
		obj->lru = lru;
	list_splice_tail(&still_in_lru.list, &lru->list);
	lru->count += still_in_lru.count;

	mutex_unlock(lru->lock);

	return freed;
}
EXPORT_SYMBOL(drm_gem_lru_scan);

 
int drm_gem_evict(struct drm_gem_object *obj)
{
	dma_resv_assert_held(obj->resv);

	if (!dma_resv_test_signaled(obj->resv, DMA_RESV_USAGE_READ))
		return -EBUSY;

	if (obj->funcs->evict)
		return obj->funcs->evict(obj);

	return 0;
}
EXPORT_SYMBOL(drm_gem_evict);
