
 
#include <linux/anon_inodes.h>
#include <linux/device.h>
#include <linux/idr.h>
#include <linux/mm.h>
#include <linux/sched.h>
#include <linux/slab.h>
#include <linux/tee_drv.h>
#include <linux/uaccess.h>
#include <linux/uio.h>
#include <linux/highmem.h>
#include "tee_private.h"

static void shm_put_kernel_pages(struct page **pages, size_t page_count)
{
	size_t n;

	for (n = 0; n < page_count; n++)
		put_page(pages[n]);
}

static int shm_get_kernel_pages(unsigned long start, size_t page_count,
				struct page **pages)
{
	struct page *page;
	size_t n;

	if (WARN_ON_ONCE(is_vmalloc_addr((void *)start) ||
			 is_kmap_addr((void *)start)))
		return -EINVAL;

	page = virt_to_page((void *)start);
	for (n = 0; n < page_count; n++) {
		pages[n] = page + n;
		get_page(pages[n]);
	}

	return page_count;
}

static void release_registered_pages(struct tee_shm *shm)
{
	if (shm->pages) {
		if (shm->flags & TEE_SHM_USER_MAPPED)
			unpin_user_pages(shm->pages, shm->num_pages);
		else
			shm_put_kernel_pages(shm->pages, shm->num_pages);

		kfree(shm->pages);
	}
}

static void tee_shm_release(struct tee_device *teedev, struct tee_shm *shm)
{
	if (shm->flags & TEE_SHM_POOL) {
		teedev->pool->ops->free(teedev->pool, shm);
	} else if (shm->flags & TEE_SHM_DYNAMIC) {
		int rc = teedev->desc->ops->shm_unregister(shm->ctx, shm);

		if (rc)
			dev_err(teedev->dev.parent,
				"unregister shm %p failed: %d", shm, rc);

		release_registered_pages(shm);
	}

	teedev_ctx_put(shm->ctx);

	kfree(shm);

	tee_device_put(teedev);
}

static struct tee_shm *shm_alloc_helper(struct tee_context *ctx, size_t size,
					size_t align, u32 flags, int id)
{
	struct tee_device *teedev = ctx->teedev;
	struct tee_shm *shm;
	void *ret;
	int rc;

	if (!tee_device_get(teedev))
		return ERR_PTR(-EINVAL);

	if (!teedev->pool) {
		 
		ret = ERR_PTR(-EINVAL);
		goto err_dev_put;
	}

	shm = kzalloc(sizeof(*shm), GFP_KERNEL);
	if (!shm) {
		ret = ERR_PTR(-ENOMEM);
		goto err_dev_put;
	}

	refcount_set(&shm->refcount, 1);
	shm->flags = flags;
	shm->id = id;

	 
	shm->ctx = ctx;

	rc = teedev->pool->ops->alloc(teedev->pool, shm, size, align);
	if (rc) {
		ret = ERR_PTR(rc);
		goto err_kfree;
	}

	teedev_ctx_get(ctx);
	return shm;
err_kfree:
	kfree(shm);
err_dev_put:
	tee_device_put(teedev);
	return ret;
}

 
struct tee_shm *tee_shm_alloc_user_buf(struct tee_context *ctx, size_t size)
{
	u32 flags = TEE_SHM_DYNAMIC | TEE_SHM_POOL;
	struct tee_device *teedev = ctx->teedev;
	struct tee_shm *shm;
	void *ret;
	int id;

	mutex_lock(&teedev->mutex);
	id = idr_alloc(&teedev->idr, NULL, 1, 0, GFP_KERNEL);
	mutex_unlock(&teedev->mutex);
	if (id < 0)
		return ERR_PTR(id);

	shm = shm_alloc_helper(ctx, size, PAGE_SIZE, flags, id);
	if (IS_ERR(shm)) {
		mutex_lock(&teedev->mutex);
		idr_remove(&teedev->idr, id);
		mutex_unlock(&teedev->mutex);
		return shm;
	}

	mutex_lock(&teedev->mutex);
	ret = idr_replace(&teedev->idr, shm, id);
	mutex_unlock(&teedev->mutex);
	if (IS_ERR(ret)) {
		tee_shm_free(shm);
		return ret;
	}

	return shm;
}

 
struct tee_shm *tee_shm_alloc_kernel_buf(struct tee_context *ctx, size_t size)
{
	u32 flags = TEE_SHM_DYNAMIC | TEE_SHM_POOL;

	return shm_alloc_helper(ctx, size, PAGE_SIZE, flags, -1);
}
EXPORT_SYMBOL_GPL(tee_shm_alloc_kernel_buf);

 
struct tee_shm *tee_shm_alloc_priv_buf(struct tee_context *ctx, size_t size)
{
	u32 flags = TEE_SHM_PRIV | TEE_SHM_POOL;

	return shm_alloc_helper(ctx, size, sizeof(long) * 2, flags, -1);
}
EXPORT_SYMBOL_GPL(tee_shm_alloc_priv_buf);

static struct tee_shm *
register_shm_helper(struct tee_context *ctx, unsigned long addr,
		    size_t length, u32 flags, int id)
{
	struct tee_device *teedev = ctx->teedev;
	struct tee_shm *shm;
	unsigned long start;
	size_t num_pages;
	void *ret;
	int rc;

	if (!tee_device_get(teedev))
		return ERR_PTR(-EINVAL);

	if (!teedev->desc->ops->shm_register ||
	    !teedev->desc->ops->shm_unregister) {
		ret = ERR_PTR(-ENOTSUPP);
		goto err_dev_put;
	}

	teedev_ctx_get(ctx);

	shm = kzalloc(sizeof(*shm), GFP_KERNEL);
	if (!shm) {
		ret = ERR_PTR(-ENOMEM);
		goto err_ctx_put;
	}

	refcount_set(&shm->refcount, 1);
	shm->flags = flags;
	shm->ctx = ctx;
	shm->id = id;
	addr = untagged_addr(addr);
	start = rounddown(addr, PAGE_SIZE);
	shm->offset = addr - start;
	shm->size = length;
	num_pages = (roundup(addr + length, PAGE_SIZE) - start) / PAGE_SIZE;
	shm->pages = kcalloc(num_pages, sizeof(*shm->pages), GFP_KERNEL);
	if (!shm->pages) {
		ret = ERR_PTR(-ENOMEM);
		goto err_free_shm;
	}

	if (flags & TEE_SHM_USER_MAPPED)
		rc = pin_user_pages_fast(start, num_pages, FOLL_WRITE,
					 shm->pages);
	else
		rc = shm_get_kernel_pages(start, num_pages, shm->pages);
	if (rc > 0)
		shm->num_pages = rc;
	if (rc != num_pages) {
		if (rc >= 0)
			rc = -ENOMEM;
		ret = ERR_PTR(rc);
		goto err_put_shm_pages;
	}

	rc = teedev->desc->ops->shm_register(ctx, shm, shm->pages,
					     shm->num_pages, start);
	if (rc) {
		ret = ERR_PTR(rc);
		goto err_put_shm_pages;
	}

	return shm;
err_put_shm_pages:
	if (flags & TEE_SHM_USER_MAPPED)
		unpin_user_pages(shm->pages, shm->num_pages);
	else
		shm_put_kernel_pages(shm->pages, shm->num_pages);
	kfree(shm->pages);
err_free_shm:
	kfree(shm);
err_ctx_put:
	teedev_ctx_put(ctx);
err_dev_put:
	tee_device_put(teedev);
	return ret;
}

 
struct tee_shm *tee_shm_register_user_buf(struct tee_context *ctx,
					  unsigned long addr, size_t length)
{
	u32 flags = TEE_SHM_USER_MAPPED | TEE_SHM_DYNAMIC;
	struct tee_device *teedev = ctx->teedev;
	struct tee_shm *shm;
	void *ret;
	int id;

	if (!access_ok((void __user *)addr, length))
		return ERR_PTR(-EFAULT);

	mutex_lock(&teedev->mutex);
	id = idr_alloc(&teedev->idr, NULL, 1, 0, GFP_KERNEL);
	mutex_unlock(&teedev->mutex);
	if (id < 0)
		return ERR_PTR(id);

	shm = register_shm_helper(ctx, addr, length, flags, id);
	if (IS_ERR(shm)) {
		mutex_lock(&teedev->mutex);
		idr_remove(&teedev->idr, id);
		mutex_unlock(&teedev->mutex);
		return shm;
	}

	mutex_lock(&teedev->mutex);
	ret = idr_replace(&teedev->idr, shm, id);
	mutex_unlock(&teedev->mutex);
	if (IS_ERR(ret)) {
		tee_shm_free(shm);
		return ret;
	}

	return shm;
}

 

struct tee_shm *tee_shm_register_kernel_buf(struct tee_context *ctx,
					    void *addr, size_t length)
{
	u32 flags = TEE_SHM_DYNAMIC;

	return register_shm_helper(ctx, (unsigned long)addr, length, flags, -1);
}
EXPORT_SYMBOL_GPL(tee_shm_register_kernel_buf);

static int tee_shm_fop_release(struct inode *inode, struct file *filp)
{
	tee_shm_put(filp->private_data);
	return 0;
}

static int tee_shm_fop_mmap(struct file *filp, struct vm_area_struct *vma)
{
	struct tee_shm *shm = filp->private_data;
	size_t size = vma->vm_end - vma->vm_start;

	 
	if (shm->flags & TEE_SHM_USER_MAPPED)
		return -EINVAL;

	 
	if (vma->vm_pgoff + vma_pages(vma) > shm->size >> PAGE_SHIFT)
		return -EINVAL;

	return remap_pfn_range(vma, vma->vm_start, shm->paddr >> PAGE_SHIFT,
			       size, vma->vm_page_prot);
}

static const struct file_operations tee_shm_fops = {
	.owner = THIS_MODULE,
	.release = tee_shm_fop_release,
	.mmap = tee_shm_fop_mmap,
};

 
int tee_shm_get_fd(struct tee_shm *shm)
{
	int fd;

	if (shm->id < 0)
		return -EINVAL;

	 
	refcount_inc(&shm->refcount);
	fd = anon_inode_getfd("tee_shm", &tee_shm_fops, shm, O_RDWR);
	if (fd < 0)
		tee_shm_put(shm);
	return fd;
}

 
void tee_shm_free(struct tee_shm *shm)
{
	tee_shm_put(shm);
}
EXPORT_SYMBOL_GPL(tee_shm_free);

 
void *tee_shm_get_va(struct tee_shm *shm, size_t offs)
{
	if (!shm->kaddr)
		return ERR_PTR(-EINVAL);
	if (offs >= shm->size)
		return ERR_PTR(-EINVAL);
	return (char *)shm->kaddr + offs;
}
EXPORT_SYMBOL_GPL(tee_shm_get_va);

 
int tee_shm_get_pa(struct tee_shm *shm, size_t offs, phys_addr_t *pa)
{
	if (offs >= shm->size)
		return -EINVAL;
	if (pa)
		*pa = shm->paddr + offs;
	return 0;
}
EXPORT_SYMBOL_GPL(tee_shm_get_pa);

 
struct tee_shm *tee_shm_get_from_id(struct tee_context *ctx, int id)
{
	struct tee_device *teedev;
	struct tee_shm *shm;

	if (!ctx)
		return ERR_PTR(-EINVAL);

	teedev = ctx->teedev;
	mutex_lock(&teedev->mutex);
	shm = idr_find(&teedev->idr, id);
	 
	if (!shm || shm->ctx != ctx)
		shm = ERR_PTR(-EINVAL);
	else
		refcount_inc(&shm->refcount);
	mutex_unlock(&teedev->mutex);
	return shm;
}
EXPORT_SYMBOL_GPL(tee_shm_get_from_id);

 
void tee_shm_put(struct tee_shm *shm)
{
	struct tee_device *teedev = shm->ctx->teedev;
	bool do_release = false;

	mutex_lock(&teedev->mutex);
	if (refcount_dec_and_test(&shm->refcount)) {
		 
		if (shm->id >= 0)
			idr_remove(&teedev->idr, shm->id);
		do_release = true;
	}
	mutex_unlock(&teedev->mutex);

	if (do_release)
		tee_shm_release(teedev, shm);
}
EXPORT_SYMBOL_GPL(tee_shm_put);
