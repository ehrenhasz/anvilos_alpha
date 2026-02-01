 

#include <linux/export.h>
#include <linux/dma-buf.h>
#include <linux/rbtree.h>
#include <linux/module.h>

#include <drm/drm.h>
#include <drm/drm_drv.h>
#include <drm/drm_file.h>
#include <drm/drm_framebuffer.h>
#include <drm/drm_gem.h>
#include <drm/drm_prime.h>

#include "drm_internal.h"

MODULE_IMPORT_NS(DMA_BUF);

 

struct drm_prime_member {
	struct dma_buf *dma_buf;
	uint32_t handle;

	struct rb_node dmabuf_rb;
	struct rb_node handle_rb;
};

static int drm_prime_add_buf_handle(struct drm_prime_file_private *prime_fpriv,
				    struct dma_buf *dma_buf, uint32_t handle)
{
	struct drm_prime_member *member;
	struct rb_node **p, *rb;

	member = kmalloc(sizeof(*member), GFP_KERNEL);
	if (!member)
		return -ENOMEM;

	get_dma_buf(dma_buf);
	member->dma_buf = dma_buf;
	member->handle = handle;

	rb = NULL;
	p = &prime_fpriv->dmabufs.rb_node;
	while (*p) {
		struct drm_prime_member *pos;

		rb = *p;
		pos = rb_entry(rb, struct drm_prime_member, dmabuf_rb);
		if (dma_buf > pos->dma_buf)
			p = &rb->rb_right;
		else
			p = &rb->rb_left;
	}
	rb_link_node(&member->dmabuf_rb, rb, p);
	rb_insert_color(&member->dmabuf_rb, &prime_fpriv->dmabufs);

	rb = NULL;
	p = &prime_fpriv->handles.rb_node;
	while (*p) {
		struct drm_prime_member *pos;

		rb = *p;
		pos = rb_entry(rb, struct drm_prime_member, handle_rb);
		if (handle > pos->handle)
			p = &rb->rb_right;
		else
			p = &rb->rb_left;
	}
	rb_link_node(&member->handle_rb, rb, p);
	rb_insert_color(&member->handle_rb, &prime_fpriv->handles);

	return 0;
}

static struct dma_buf *drm_prime_lookup_buf_by_handle(struct drm_prime_file_private *prime_fpriv,
						      uint32_t handle)
{
	struct rb_node *rb;

	rb = prime_fpriv->handles.rb_node;
	while (rb) {
		struct drm_prime_member *member;

		member = rb_entry(rb, struct drm_prime_member, handle_rb);
		if (member->handle == handle)
			return member->dma_buf;
		else if (member->handle < handle)
			rb = rb->rb_right;
		else
			rb = rb->rb_left;
	}

	return NULL;
}

static int drm_prime_lookup_buf_handle(struct drm_prime_file_private *prime_fpriv,
				       struct dma_buf *dma_buf,
				       uint32_t *handle)
{
	struct rb_node *rb;

	rb = prime_fpriv->dmabufs.rb_node;
	while (rb) {
		struct drm_prime_member *member;

		member = rb_entry(rb, struct drm_prime_member, dmabuf_rb);
		if (member->dma_buf == dma_buf) {
			*handle = member->handle;
			return 0;
		} else if (member->dma_buf < dma_buf) {
			rb = rb->rb_right;
		} else {
			rb = rb->rb_left;
		}
	}

	return -ENOENT;
}

void drm_prime_remove_buf_handle(struct drm_prime_file_private *prime_fpriv,
				 uint32_t handle)
{
	struct rb_node *rb;

	mutex_lock(&prime_fpriv->lock);

	rb = prime_fpriv->handles.rb_node;
	while (rb) {
		struct drm_prime_member *member;

		member = rb_entry(rb, struct drm_prime_member, handle_rb);
		if (member->handle == handle) {
			rb_erase(&member->handle_rb, &prime_fpriv->handles);
			rb_erase(&member->dmabuf_rb, &prime_fpriv->dmabufs);

			dma_buf_put(member->dma_buf);
			kfree(member);
			break;
		} else if (member->handle < handle) {
			rb = rb->rb_right;
		} else {
			rb = rb->rb_left;
		}
	}

	mutex_unlock(&prime_fpriv->lock);
}

void drm_prime_init_file_private(struct drm_prime_file_private *prime_fpriv)
{
	mutex_init(&prime_fpriv->lock);
	prime_fpriv->dmabufs = RB_ROOT;
	prime_fpriv->handles = RB_ROOT;
}

void drm_prime_destroy_file_private(struct drm_prime_file_private *prime_fpriv)
{
	 
	WARN_ON(!RB_EMPTY_ROOT(&prime_fpriv->dmabufs));
}

 
struct dma_buf *drm_gem_dmabuf_export(struct drm_device *dev,
				      struct dma_buf_export_info *exp_info)
{
	struct drm_gem_object *obj = exp_info->priv;
	struct dma_buf *dma_buf;

	dma_buf = dma_buf_export(exp_info);
	if (IS_ERR(dma_buf))
		return dma_buf;

	drm_dev_get(dev);
	drm_gem_object_get(obj);
	dma_buf->file->f_mapping = obj->dev->anon_inode->i_mapping;

	return dma_buf;
}
EXPORT_SYMBOL(drm_gem_dmabuf_export);

 
void drm_gem_dmabuf_release(struct dma_buf *dma_buf)
{
	struct drm_gem_object *obj = dma_buf->priv;
	struct drm_device *dev = obj->dev;

	 
	drm_gem_object_put(obj);

	drm_dev_put(dev);
}
EXPORT_SYMBOL(drm_gem_dmabuf_release);

 
int drm_gem_prime_fd_to_handle(struct drm_device *dev,
			       struct drm_file *file_priv, int prime_fd,
			       uint32_t *handle)
{
	struct dma_buf *dma_buf;
	struct drm_gem_object *obj;
	int ret;

	dma_buf = dma_buf_get(prime_fd);
	if (IS_ERR(dma_buf))
		return PTR_ERR(dma_buf);

	mutex_lock(&file_priv->prime.lock);

	ret = drm_prime_lookup_buf_handle(&file_priv->prime,
			dma_buf, handle);
	if (ret == 0)
		goto out_put;

	 
	mutex_lock(&dev->object_name_lock);
	if (dev->driver->gem_prime_import)
		obj = dev->driver->gem_prime_import(dev, dma_buf);
	else
		obj = drm_gem_prime_import(dev, dma_buf);
	if (IS_ERR(obj)) {
		ret = PTR_ERR(obj);
		goto out_unlock;
	}

	if (obj->dma_buf) {
		WARN_ON(obj->dma_buf != dma_buf);
	} else {
		obj->dma_buf = dma_buf;
		get_dma_buf(dma_buf);
	}

	 
	ret = drm_gem_handle_create_tail(file_priv, obj, handle);
	drm_gem_object_put(obj);
	if (ret)
		goto out_put;

	ret = drm_prime_add_buf_handle(&file_priv->prime,
			dma_buf, *handle);
	mutex_unlock(&file_priv->prime.lock);
	if (ret)
		goto fail;

	dma_buf_put(dma_buf);

	return 0;

fail:
	 
	drm_gem_handle_delete(file_priv, *handle);
	dma_buf_put(dma_buf);
	return ret;

out_unlock:
	mutex_unlock(&dev->object_name_lock);
out_put:
	mutex_unlock(&file_priv->prime.lock);
	dma_buf_put(dma_buf);
	return ret;
}
EXPORT_SYMBOL(drm_gem_prime_fd_to_handle);

int drm_prime_fd_to_handle_ioctl(struct drm_device *dev, void *data,
				 struct drm_file *file_priv)
{
	struct drm_prime_handle *args = data;

	if (dev->driver->prime_fd_to_handle) {
		return dev->driver->prime_fd_to_handle(dev, file_priv, args->fd,
						       &args->handle);
	}

	return drm_gem_prime_fd_to_handle(dev, file_priv, args->fd, &args->handle);
}

static struct dma_buf *export_and_register_object(struct drm_device *dev,
						  struct drm_gem_object *obj,
						  uint32_t flags)
{
	struct dma_buf *dmabuf;

	 
	if (obj->handle_count == 0) {
		dmabuf = ERR_PTR(-ENOENT);
		return dmabuf;
	}

	if (obj->funcs && obj->funcs->export)
		dmabuf = obj->funcs->export(obj, flags);
	else
		dmabuf = drm_gem_prime_export(obj, flags);
	if (IS_ERR(dmabuf)) {
		 
		return dmabuf;
	}

	 
	obj->dma_buf = dmabuf;
	get_dma_buf(obj->dma_buf);

	return dmabuf;
}

 
int drm_gem_prime_handle_to_fd(struct drm_device *dev,
			       struct drm_file *file_priv, uint32_t handle,
			       uint32_t flags,
			       int *prime_fd)
{
	struct drm_gem_object *obj;
	int ret = 0;
	struct dma_buf *dmabuf;

	mutex_lock(&file_priv->prime.lock);
	obj = drm_gem_object_lookup(file_priv, handle);
	if (!obj)  {
		ret = -ENOENT;
		goto out_unlock;
	}

	dmabuf = drm_prime_lookup_buf_by_handle(&file_priv->prime, handle);
	if (dmabuf) {
		get_dma_buf(dmabuf);
		goto out_have_handle;
	}

	mutex_lock(&dev->object_name_lock);
	 
	if (obj->import_attach) {
		dmabuf = obj->import_attach->dmabuf;
		get_dma_buf(dmabuf);
		goto out_have_obj;
	}

	if (obj->dma_buf) {
		get_dma_buf(obj->dma_buf);
		dmabuf = obj->dma_buf;
		goto out_have_obj;
	}

	dmabuf = export_and_register_object(dev, obj, flags);
	if (IS_ERR(dmabuf)) {
		 
		ret = PTR_ERR(dmabuf);
		mutex_unlock(&dev->object_name_lock);
		goto out;
	}

out_have_obj:
	 
	ret = drm_prime_add_buf_handle(&file_priv->prime,
				       dmabuf, handle);
	mutex_unlock(&dev->object_name_lock);
	if (ret)
		goto fail_put_dmabuf;

out_have_handle:
	ret = dma_buf_fd(dmabuf, flags);
	 
	if (ret < 0) {
		goto fail_put_dmabuf;
	} else {
		*prime_fd = ret;
		ret = 0;
	}

	goto out;

fail_put_dmabuf:
	dma_buf_put(dmabuf);
out:
	drm_gem_object_put(obj);
out_unlock:
	mutex_unlock(&file_priv->prime.lock);

	return ret;
}
EXPORT_SYMBOL(drm_gem_prime_handle_to_fd);

int drm_prime_handle_to_fd_ioctl(struct drm_device *dev, void *data,
				 struct drm_file *file_priv)
{
	struct drm_prime_handle *args = data;

	 
	if (args->flags & ~(DRM_CLOEXEC | DRM_RDWR))
		return -EINVAL;

	if (dev->driver->prime_handle_to_fd) {
		return dev->driver->prime_handle_to_fd(dev, file_priv,
						       args->handle, args->flags,
						       &args->fd);
	}
	return drm_gem_prime_handle_to_fd(dev, file_priv, args->handle,
					  args->flags, &args->fd);
}

 

 
int drm_gem_map_attach(struct dma_buf *dma_buf,
		       struct dma_buf_attachment *attach)
{
	struct drm_gem_object *obj = dma_buf->priv;

	if (!obj->funcs->get_sg_table)
		return -ENOSYS;

	return drm_gem_pin(obj);
}
EXPORT_SYMBOL(drm_gem_map_attach);

 
void drm_gem_map_detach(struct dma_buf *dma_buf,
			struct dma_buf_attachment *attach)
{
	struct drm_gem_object *obj = dma_buf->priv;

	drm_gem_unpin(obj);
}
EXPORT_SYMBOL(drm_gem_map_detach);

 
struct sg_table *drm_gem_map_dma_buf(struct dma_buf_attachment *attach,
				     enum dma_data_direction dir)
{
	struct drm_gem_object *obj = attach->dmabuf->priv;
	struct sg_table *sgt;
	int ret;

	if (WARN_ON(dir == DMA_NONE))
		return ERR_PTR(-EINVAL);

	if (WARN_ON(!obj->funcs->get_sg_table))
		return ERR_PTR(-ENOSYS);

	sgt = obj->funcs->get_sg_table(obj);
	if (IS_ERR(sgt))
		return sgt;

	ret = dma_map_sgtable(attach->dev, sgt, dir,
			      DMA_ATTR_SKIP_CPU_SYNC);
	if (ret) {
		sg_free_table(sgt);
		kfree(sgt);
		sgt = ERR_PTR(ret);
	}

	return sgt;
}
EXPORT_SYMBOL(drm_gem_map_dma_buf);

 
void drm_gem_unmap_dma_buf(struct dma_buf_attachment *attach,
			   struct sg_table *sgt,
			   enum dma_data_direction dir)
{
	if (!sgt)
		return;

	dma_unmap_sgtable(attach->dev, sgt, dir, DMA_ATTR_SKIP_CPU_SYNC);
	sg_free_table(sgt);
	kfree(sgt);
}
EXPORT_SYMBOL(drm_gem_unmap_dma_buf);

 
int drm_gem_dmabuf_vmap(struct dma_buf *dma_buf, struct iosys_map *map)
{
	struct drm_gem_object *obj = dma_buf->priv;

	return drm_gem_vmap(obj, map);
}
EXPORT_SYMBOL(drm_gem_dmabuf_vmap);

 
void drm_gem_dmabuf_vunmap(struct dma_buf *dma_buf, struct iosys_map *map)
{
	struct drm_gem_object *obj = dma_buf->priv;

	drm_gem_vunmap(obj, map);
}
EXPORT_SYMBOL(drm_gem_dmabuf_vunmap);

 
int drm_gem_prime_mmap(struct drm_gem_object *obj, struct vm_area_struct *vma)
{
	struct drm_file *priv;
	struct file *fil;
	int ret;

	 
	vma->vm_pgoff += drm_vma_node_start(&obj->vma_node);

	if (obj->funcs && obj->funcs->mmap) {
		vma->vm_ops = obj->funcs->vm_ops;

		drm_gem_object_get(obj);
		ret = obj->funcs->mmap(obj, vma);
		if (ret) {
			drm_gem_object_put(obj);
			return ret;
		}
		vma->vm_private_data = obj;
		return 0;
	}

	priv = kzalloc(sizeof(*priv), GFP_KERNEL);
	fil = kzalloc(sizeof(*fil), GFP_KERNEL);
	if (!priv || !fil) {
		ret = -ENOMEM;
		goto out;
	}

	 
	priv->minor = obj->dev->primary;
	fil->private_data = priv;

	ret = drm_vma_node_allow(&obj->vma_node, priv);
	if (ret)
		goto out;

	ret = obj->dev->driver->fops->mmap(fil, vma);

	drm_vma_node_revoke(&obj->vma_node, priv);
out:
	kfree(priv);
	kfree(fil);

	return ret;
}
EXPORT_SYMBOL(drm_gem_prime_mmap);

 
int drm_gem_dmabuf_mmap(struct dma_buf *dma_buf, struct vm_area_struct *vma)
{
	struct drm_gem_object *obj = dma_buf->priv;

	return drm_gem_prime_mmap(obj, vma);
}
EXPORT_SYMBOL(drm_gem_dmabuf_mmap);

static const struct dma_buf_ops drm_gem_prime_dmabuf_ops =  {
	.cache_sgt_mapping = true,
	.attach = drm_gem_map_attach,
	.detach = drm_gem_map_detach,
	.map_dma_buf = drm_gem_map_dma_buf,
	.unmap_dma_buf = drm_gem_unmap_dma_buf,
	.release = drm_gem_dmabuf_release,
	.mmap = drm_gem_dmabuf_mmap,
	.vmap = drm_gem_dmabuf_vmap,
	.vunmap = drm_gem_dmabuf_vunmap,
};

 
struct sg_table *drm_prime_pages_to_sg(struct drm_device *dev,
				       struct page **pages, unsigned int nr_pages)
{
	struct sg_table *sg;
	size_t max_segment = 0;
	int err;

	sg = kmalloc(sizeof(struct sg_table), GFP_KERNEL);
	if (!sg)
		return ERR_PTR(-ENOMEM);

	if (dev)
		max_segment = dma_max_mapping_size(dev->dev);
	if (max_segment == 0)
		max_segment = UINT_MAX;
	err = sg_alloc_table_from_pages_segment(sg, pages, nr_pages, 0,
						nr_pages << PAGE_SHIFT,
						max_segment, GFP_KERNEL);
	if (err) {
		kfree(sg);
		sg = ERR_PTR(err);
	}
	return sg;
}
EXPORT_SYMBOL(drm_prime_pages_to_sg);

 
unsigned long drm_prime_get_contiguous_size(struct sg_table *sgt)
{
	dma_addr_t expected = sg_dma_address(sgt->sgl);
	struct scatterlist *sg;
	unsigned long size = 0;
	int i;

	for_each_sgtable_dma_sg(sgt, sg, i) {
		unsigned int len = sg_dma_len(sg);

		if (!len)
			break;
		if (sg_dma_address(sg) != expected)
			break;
		expected += len;
		size += len;
	}
	return size;
}
EXPORT_SYMBOL(drm_prime_get_contiguous_size);

 
struct dma_buf *drm_gem_prime_export(struct drm_gem_object *obj,
				     int flags)
{
	struct drm_device *dev = obj->dev;
	struct dma_buf_export_info exp_info = {
		.exp_name = KBUILD_MODNAME,  
		.owner = dev->driver->fops->owner,
		.ops = &drm_gem_prime_dmabuf_ops,
		.size = obj->size,
		.flags = flags,
		.priv = obj,
		.resv = obj->resv,
	};

	return drm_gem_dmabuf_export(dev, &exp_info);
}
EXPORT_SYMBOL(drm_gem_prime_export);

 
struct drm_gem_object *drm_gem_prime_import_dev(struct drm_device *dev,
					    struct dma_buf *dma_buf,
					    struct device *attach_dev)
{
	struct dma_buf_attachment *attach;
	struct sg_table *sgt;
	struct drm_gem_object *obj;
	int ret;

	if (dma_buf->ops == &drm_gem_prime_dmabuf_ops) {
		obj = dma_buf->priv;
		if (obj->dev == dev) {
			 
			drm_gem_object_get(obj);
			return obj;
		}
	}

	if (!dev->driver->gem_prime_import_sg_table)
		return ERR_PTR(-EINVAL);

	attach = dma_buf_attach(dma_buf, attach_dev);
	if (IS_ERR(attach))
		return ERR_CAST(attach);

	get_dma_buf(dma_buf);

	sgt = dma_buf_map_attachment_unlocked(attach, DMA_BIDIRECTIONAL);
	if (IS_ERR(sgt)) {
		ret = PTR_ERR(sgt);
		goto fail_detach;
	}

	obj = dev->driver->gem_prime_import_sg_table(dev, attach, sgt);
	if (IS_ERR(obj)) {
		ret = PTR_ERR(obj);
		goto fail_unmap;
	}

	obj->import_attach = attach;
	obj->resv = dma_buf->resv;

	return obj;

fail_unmap:
	dma_buf_unmap_attachment_unlocked(attach, sgt, DMA_BIDIRECTIONAL);
fail_detach:
	dma_buf_detach(dma_buf, attach);
	dma_buf_put(dma_buf);

	return ERR_PTR(ret);
}
EXPORT_SYMBOL(drm_gem_prime_import_dev);

 
struct drm_gem_object *drm_gem_prime_import(struct drm_device *dev,
					    struct dma_buf *dma_buf)
{
	return drm_gem_prime_import_dev(dev, dma_buf, dev->dev);
}
EXPORT_SYMBOL(drm_gem_prime_import);

 
int __deprecated drm_prime_sg_to_page_array(struct sg_table *sgt,
					    struct page **pages,
					    int max_entries)
{
	struct sg_page_iter page_iter;
	struct page **p = pages;

	for_each_sgtable_page(sgt, &page_iter, 0) {
		if (WARN_ON(p - pages >= max_entries))
			return -1;
		*p++ = sg_page_iter_page(&page_iter);
	}
	return 0;
}
EXPORT_SYMBOL(drm_prime_sg_to_page_array);

 
int drm_prime_sg_to_dma_addr_array(struct sg_table *sgt, dma_addr_t *addrs,
				   int max_entries)
{
	struct sg_dma_page_iter dma_iter;
	dma_addr_t *a = addrs;

	for_each_sgtable_dma_page(sgt, &dma_iter, 0) {
		if (WARN_ON(a - addrs >= max_entries))
			return -1;
		*a++ = sg_page_iter_dma_address(&dma_iter);
	}
	return 0;
}
EXPORT_SYMBOL(drm_prime_sg_to_dma_addr_array);

 
void drm_prime_gem_destroy(struct drm_gem_object *obj, struct sg_table *sg)
{
	struct dma_buf_attachment *attach;
	struct dma_buf *dma_buf;

	attach = obj->import_attach;
	if (sg)
		dma_buf_unmap_attachment_unlocked(attach, sg, DMA_BIDIRECTIONAL);
	dma_buf = attach->dmabuf;
	dma_buf_detach(attach->dmabuf, attach);
	 
	dma_buf_put(dma_buf);
}
EXPORT_SYMBOL(drm_prime_gem_destroy);
