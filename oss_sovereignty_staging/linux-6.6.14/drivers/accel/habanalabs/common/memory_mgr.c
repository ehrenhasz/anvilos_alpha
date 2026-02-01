

 

#include "habanalabs.h"

 
struct hl_mmap_mem_buf *hl_mmap_mem_buf_get(struct hl_mem_mgr *mmg, u64 handle)
{
	struct hl_mmap_mem_buf *buf;

	spin_lock(&mmg->lock);
	buf = idr_find(&mmg->handles, lower_32_bits(handle >> PAGE_SHIFT));
	if (!buf) {
		spin_unlock(&mmg->lock);
		dev_dbg(mmg->dev, "Buff get failed, no match to handle %#llx\n", handle);
		return NULL;
	}
	kref_get(&buf->refcount);
	spin_unlock(&mmg->lock);
	return buf;
}

 
static void hl_mmap_mem_buf_destroy(struct hl_mmap_mem_buf *buf)
{
	if (buf->behavior->release)
		buf->behavior->release(buf);

	kfree(buf);
}

 
static void hl_mmap_mem_buf_release(struct kref *kref)
{
	struct hl_mmap_mem_buf *buf =
		container_of(kref, struct hl_mmap_mem_buf, refcount);

	spin_lock(&buf->mmg->lock);
	idr_remove(&buf->mmg->handles, lower_32_bits(buf->handle >> PAGE_SHIFT));
	spin_unlock(&buf->mmg->lock);

	hl_mmap_mem_buf_destroy(buf);
}

 
static void hl_mmap_mem_buf_remove_idr_locked(struct kref *kref)
{
	struct hl_mmap_mem_buf *buf =
		container_of(kref, struct hl_mmap_mem_buf, refcount);

	idr_remove(&buf->mmg->handles, lower_32_bits(buf->handle >> PAGE_SHIFT));
}

 
int hl_mmap_mem_buf_put(struct hl_mmap_mem_buf *buf)
{
	return kref_put(&buf->refcount, hl_mmap_mem_buf_release);
}

 
int hl_mmap_mem_buf_put_handle(struct hl_mem_mgr *mmg, u64 handle)
{
	struct hl_mmap_mem_buf *buf;

	spin_lock(&mmg->lock);
	buf = idr_find(&mmg->handles, lower_32_bits(handle >> PAGE_SHIFT));
	if (!buf) {
		spin_unlock(&mmg->lock);
		dev_dbg(mmg->dev,
			 "Buff put failed, no match to handle %#llx\n", handle);
		return -EINVAL;
	}

	if (kref_put(&buf->refcount, hl_mmap_mem_buf_remove_idr_locked)) {
		spin_unlock(&mmg->lock);
		hl_mmap_mem_buf_destroy(buf);
		return 1;
	}

	spin_unlock(&mmg->lock);
	return 0;
}

 
struct hl_mmap_mem_buf *
hl_mmap_mem_buf_alloc(struct hl_mem_mgr *mmg,
		      struct hl_mmap_mem_buf_behavior *behavior, gfp_t gfp,
		      void *args)
{
	struct hl_mmap_mem_buf *buf;
	int rc;

	buf = kzalloc(sizeof(*buf), gfp);
	if (!buf)
		return NULL;

	spin_lock(&mmg->lock);
	rc = idr_alloc(&mmg->handles, buf, 1, 0, GFP_ATOMIC);
	spin_unlock(&mmg->lock);
	if (rc < 0) {
		dev_err(mmg->dev,
			"%s: Failed to allocate IDR for a new buffer, rc=%d\n",
			behavior->topic, rc);
		goto free_buf;
	}

	buf->mmg = mmg;
	buf->behavior = behavior;
	buf->handle = (((u64)rc | buf->behavior->mem_id) << PAGE_SHIFT);
	kref_init(&buf->refcount);

	rc = buf->behavior->alloc(buf, gfp, args);
	if (rc) {
		dev_err(mmg->dev, "%s: Failure in buffer alloc callback %d\n",
			behavior->topic, rc);
		goto remove_idr;
	}

	return buf;

remove_idr:
	spin_lock(&mmg->lock);
	idr_remove(&mmg->handles, lower_32_bits(buf->handle >> PAGE_SHIFT));
	spin_unlock(&mmg->lock);
free_buf:
	kfree(buf);
	return NULL;
}

 
static void hl_mmap_mem_buf_vm_close(struct vm_area_struct *vma)
{
	struct hl_mmap_mem_buf *buf =
		(struct hl_mmap_mem_buf *)vma->vm_private_data;
	long new_mmap_size;

	new_mmap_size = buf->real_mapped_size - (vma->vm_end - vma->vm_start);

	if (new_mmap_size > 0) {
		buf->real_mapped_size = new_mmap_size;
		return;
	}

	atomic_set(&buf->mmap, 0);
	hl_mmap_mem_buf_put(buf);
	vma->vm_private_data = NULL;
}

static const struct vm_operations_struct hl_mmap_mem_buf_vm_ops = {
	.close = hl_mmap_mem_buf_vm_close
};

 
int hl_mem_mgr_mmap(struct hl_mem_mgr *mmg, struct vm_area_struct *vma,
		    void *args)
{
	struct hl_mmap_mem_buf *buf;
	u64 user_mem_size;
	u64 handle;
	int rc;

	 
	handle = vma->vm_pgoff << PAGE_SHIFT;
	vma->vm_pgoff = 0;

	 
	buf = hl_mmap_mem_buf_get(mmg, handle);
	if (!buf) {
		dev_err(mmg->dev,
			"Memory mmap failed, no match to handle %#llx\n", handle);
		return -EINVAL;
	}

	 
	user_mem_size = vma->vm_end - vma->vm_start;
	if (user_mem_size != ALIGN(buf->mappable_size, PAGE_SIZE)) {
		dev_err(mmg->dev,
			"%s: Memory mmap failed, mmap VM size 0x%llx != 0x%llx allocated physical mem size\n",
			buf->behavior->topic, user_mem_size, buf->mappable_size);
		rc = -EINVAL;
		goto put_mem;
	}

#ifdef _HAS_TYPE_ARG_IN_ACCESS_OK
	if (!access_ok(VERIFY_WRITE, (void __user *)(uintptr_t)vma->vm_start,
		       user_mem_size)) {
#else
	if (!access_ok((void __user *)(uintptr_t)vma->vm_start,
		       user_mem_size)) {
#endif
		dev_err(mmg->dev, "%s: User pointer is invalid - 0x%lx\n",
			buf->behavior->topic, vma->vm_start);

		rc = -EINVAL;
		goto put_mem;
	}

	if (atomic_cmpxchg(&buf->mmap, 0, 1)) {
		dev_err(mmg->dev,
			"%s, Memory mmap failed, already mapped to user\n",
			buf->behavior->topic);
		rc = -EINVAL;
		goto put_mem;
	}

	vma->vm_ops = &hl_mmap_mem_buf_vm_ops;

	 

	vma->vm_private_data = buf;

	rc = buf->behavior->mmap(buf, vma, args);
	if (rc) {
		atomic_set(&buf->mmap, 0);
		goto put_mem;
	}

	buf->real_mapped_size = buf->mappable_size;
	vma->vm_pgoff = handle >> PAGE_SHIFT;

	return 0;

put_mem:
	hl_mmap_mem_buf_put(buf);
	return rc;
}

 
void hl_mem_mgr_init(struct device *dev, struct hl_mem_mgr *mmg)
{
	mmg->dev = dev;
	spin_lock_init(&mmg->lock);
	idr_init(&mmg->handles);
}

 
void hl_mem_mgr_fini(struct hl_mem_mgr *mmg)
{
	struct hl_mmap_mem_buf *buf;
	struct idr *idp;
	const char *topic;
	u32 id;

	idp = &mmg->handles;

	idr_for_each_entry(idp, buf, id) {
		topic = buf->behavior->topic;
		if (hl_mmap_mem_buf_put(buf) != 1)
			dev_err(mmg->dev,
				"%s: Buff handle %u for CTX is still alive\n",
				topic, id);
	}
}

 
void hl_mem_mgr_idr_destroy(struct hl_mem_mgr *mmg)
{
	if (!idr_is_empty(&mmg->handles))
		dev_crit(mmg->dev, "memory manager IDR is destroyed while it is not empty!\n");

	idr_destroy(&mmg->handles);
}
