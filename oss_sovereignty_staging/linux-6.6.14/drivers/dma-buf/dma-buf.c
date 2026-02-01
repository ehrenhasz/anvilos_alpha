
 

#include <linux/fs.h>
#include <linux/slab.h>
#include <linux/dma-buf.h>
#include <linux/dma-fence.h>
#include <linux/dma-fence-unwrap.h>
#include <linux/anon_inodes.h>
#include <linux/export.h>
#include <linux/debugfs.h>
#include <linux/module.h>
#include <linux/seq_file.h>
#include <linux/sync_file.h>
#include <linux/poll.h>
#include <linux/dma-resv.h>
#include <linux/mm.h>
#include <linux/mount.h>
#include <linux/pseudo_fs.h>

#include <uapi/linux/dma-buf.h>
#include <uapi/linux/magic.h>

#include "dma-buf-sysfs-stats.h"

static inline int is_dma_buf_file(struct file *);

struct dma_buf_list {
	struct list_head head;
	struct mutex lock;
};

static struct dma_buf_list db_list;

static char *dmabuffs_dname(struct dentry *dentry, char *buffer, int buflen)
{
	struct dma_buf *dmabuf;
	char name[DMA_BUF_NAME_LEN];
	size_t ret = 0;

	dmabuf = dentry->d_fsdata;
	spin_lock(&dmabuf->name_lock);
	if (dmabuf->name)
		ret = strlcpy(name, dmabuf->name, DMA_BUF_NAME_LEN);
	spin_unlock(&dmabuf->name_lock);

	return dynamic_dname(buffer, buflen, "/%s:%s",
			     dentry->d_name.name, ret > 0 ? name : "");
}

static void dma_buf_release(struct dentry *dentry)
{
	struct dma_buf *dmabuf;

	dmabuf = dentry->d_fsdata;
	if (unlikely(!dmabuf))
		return;

	BUG_ON(dmabuf->vmapping_counter);

	 
	BUG_ON(dmabuf->cb_in.active || dmabuf->cb_out.active);

	dma_buf_stats_teardown(dmabuf);
	dmabuf->ops->release(dmabuf);

	if (dmabuf->resv == (struct dma_resv *)&dmabuf[1])
		dma_resv_fini(dmabuf->resv);

	WARN_ON(!list_empty(&dmabuf->attachments));
	module_put(dmabuf->owner);
	kfree(dmabuf->name);
	kfree(dmabuf);
}

static int dma_buf_file_release(struct inode *inode, struct file *file)
{
	struct dma_buf *dmabuf;

	if (!is_dma_buf_file(file))
		return -EINVAL;

	dmabuf = file->private_data;
	if (dmabuf) {
		mutex_lock(&db_list.lock);
		list_del(&dmabuf->list_node);
		mutex_unlock(&db_list.lock);
	}

	return 0;
}

static const struct dentry_operations dma_buf_dentry_ops = {
	.d_dname = dmabuffs_dname,
	.d_release = dma_buf_release,
};

static struct vfsmount *dma_buf_mnt;

static int dma_buf_fs_init_context(struct fs_context *fc)
{
	struct pseudo_fs_context *ctx;

	ctx = init_pseudo(fc, DMA_BUF_MAGIC);
	if (!ctx)
		return -ENOMEM;
	ctx->dops = &dma_buf_dentry_ops;
	return 0;
}

static struct file_system_type dma_buf_fs_type = {
	.name = "dmabuf",
	.init_fs_context = dma_buf_fs_init_context,
	.kill_sb = kill_anon_super,
};

static int dma_buf_mmap_internal(struct file *file, struct vm_area_struct *vma)
{
	struct dma_buf *dmabuf;

	if (!is_dma_buf_file(file))
		return -EINVAL;

	dmabuf = file->private_data;

	 
	if (!dmabuf->ops->mmap)
		return -EINVAL;

	 
	if (vma->vm_pgoff + vma_pages(vma) >
	    dmabuf->size >> PAGE_SHIFT)
		return -EINVAL;

	return dmabuf->ops->mmap(dmabuf, vma);
}

static loff_t dma_buf_llseek(struct file *file, loff_t offset, int whence)
{
	struct dma_buf *dmabuf;
	loff_t base;

	if (!is_dma_buf_file(file))
		return -EBADF;

	dmabuf = file->private_data;

	 
	if (whence == SEEK_END)
		base = dmabuf->size;
	else if (whence == SEEK_SET)
		base = 0;
	else
		return -EINVAL;

	if (offset != 0)
		return -EINVAL;

	return base + offset;
}

 

static void dma_buf_poll_cb(struct dma_fence *fence, struct dma_fence_cb *cb)
{
	struct dma_buf_poll_cb_t *dcb = (struct dma_buf_poll_cb_t *)cb;
	struct dma_buf *dmabuf = container_of(dcb->poll, struct dma_buf, poll);
	unsigned long flags;

	spin_lock_irqsave(&dcb->poll->lock, flags);
	wake_up_locked_poll(dcb->poll, dcb->active);
	dcb->active = 0;
	spin_unlock_irqrestore(&dcb->poll->lock, flags);
	dma_fence_put(fence);
	 
	fput(dmabuf->file);
}

static bool dma_buf_poll_add_cb(struct dma_resv *resv, bool write,
				struct dma_buf_poll_cb_t *dcb)
{
	struct dma_resv_iter cursor;
	struct dma_fence *fence;
	int r;

	dma_resv_for_each_fence(&cursor, resv, dma_resv_usage_rw(write),
				fence) {
		dma_fence_get(fence);
		r = dma_fence_add_callback(fence, &dcb->cb, dma_buf_poll_cb);
		if (!r)
			return true;
		dma_fence_put(fence);
	}

	return false;
}

static __poll_t dma_buf_poll(struct file *file, poll_table *poll)
{
	struct dma_buf *dmabuf;
	struct dma_resv *resv;
	__poll_t events;

	dmabuf = file->private_data;
	if (!dmabuf || !dmabuf->resv)
		return EPOLLERR;

	resv = dmabuf->resv;

	poll_wait(file, &dmabuf->poll, poll);

	events = poll_requested_events(poll) & (EPOLLIN | EPOLLOUT);
	if (!events)
		return 0;

	dma_resv_lock(resv, NULL);

	if (events & EPOLLOUT) {
		struct dma_buf_poll_cb_t *dcb = &dmabuf->cb_out;

		 
		spin_lock_irq(&dmabuf->poll.lock);
		if (dcb->active)
			events &= ~EPOLLOUT;
		else
			dcb->active = EPOLLOUT;
		spin_unlock_irq(&dmabuf->poll.lock);

		if (events & EPOLLOUT) {
			 
			get_file(dmabuf->file);

			if (!dma_buf_poll_add_cb(resv, true, dcb))
				 
				dma_buf_poll_cb(NULL, &dcb->cb);
			else
				events &= ~EPOLLOUT;
		}
	}

	if (events & EPOLLIN) {
		struct dma_buf_poll_cb_t *dcb = &dmabuf->cb_in;

		 
		spin_lock_irq(&dmabuf->poll.lock);
		if (dcb->active)
			events &= ~EPOLLIN;
		else
			dcb->active = EPOLLIN;
		spin_unlock_irq(&dmabuf->poll.lock);

		if (events & EPOLLIN) {
			 
			get_file(dmabuf->file);

			if (!dma_buf_poll_add_cb(resv, false, dcb))
				 
				dma_buf_poll_cb(NULL, &dcb->cb);
			else
				events &= ~EPOLLIN;
		}
	}

	dma_resv_unlock(resv);
	return events;
}

 
static long dma_buf_set_name(struct dma_buf *dmabuf, const char __user *buf)
{
	char *name = strndup_user(buf, DMA_BUF_NAME_LEN);

	if (IS_ERR(name))
		return PTR_ERR(name);

	spin_lock(&dmabuf->name_lock);
	kfree(dmabuf->name);
	dmabuf->name = name;
	spin_unlock(&dmabuf->name_lock);

	return 0;
}

#if IS_ENABLED(CONFIG_SYNC_FILE)
static long dma_buf_export_sync_file(struct dma_buf *dmabuf,
				     void __user *user_data)
{
	struct dma_buf_export_sync_file arg;
	enum dma_resv_usage usage;
	struct dma_fence *fence = NULL;
	struct sync_file *sync_file;
	int fd, ret;

	if (copy_from_user(&arg, user_data, sizeof(arg)))
		return -EFAULT;

	if (arg.flags & ~DMA_BUF_SYNC_RW)
		return -EINVAL;

	if ((arg.flags & DMA_BUF_SYNC_RW) == 0)
		return -EINVAL;

	fd = get_unused_fd_flags(O_CLOEXEC);
	if (fd < 0)
		return fd;

	usage = dma_resv_usage_rw(arg.flags & DMA_BUF_SYNC_WRITE);
	ret = dma_resv_get_singleton(dmabuf->resv, usage, &fence);
	if (ret)
		goto err_put_fd;

	if (!fence)
		fence = dma_fence_get_stub();

	sync_file = sync_file_create(fence);

	dma_fence_put(fence);

	if (!sync_file) {
		ret = -ENOMEM;
		goto err_put_fd;
	}

	arg.fd = fd;
	if (copy_to_user(user_data, &arg, sizeof(arg))) {
		ret = -EFAULT;
		goto err_put_file;
	}

	fd_install(fd, sync_file->file);

	return 0;

err_put_file:
	fput(sync_file->file);
err_put_fd:
	put_unused_fd(fd);
	return ret;
}

static long dma_buf_import_sync_file(struct dma_buf *dmabuf,
				     const void __user *user_data)
{
	struct dma_buf_import_sync_file arg;
	struct dma_fence *fence, *f;
	enum dma_resv_usage usage;
	struct dma_fence_unwrap iter;
	unsigned int num_fences;
	int ret = 0;

	if (copy_from_user(&arg, user_data, sizeof(arg)))
		return -EFAULT;

	if (arg.flags & ~DMA_BUF_SYNC_RW)
		return -EINVAL;

	if ((arg.flags & DMA_BUF_SYNC_RW) == 0)
		return -EINVAL;

	fence = sync_file_get_fence(arg.fd);
	if (!fence)
		return -EINVAL;

	usage = (arg.flags & DMA_BUF_SYNC_WRITE) ? DMA_RESV_USAGE_WRITE :
						   DMA_RESV_USAGE_READ;

	num_fences = 0;
	dma_fence_unwrap_for_each(f, &iter, fence)
		++num_fences;

	if (num_fences > 0) {
		dma_resv_lock(dmabuf->resv, NULL);

		ret = dma_resv_reserve_fences(dmabuf->resv, num_fences);
		if (!ret) {
			dma_fence_unwrap_for_each(f, &iter, fence)
				dma_resv_add_fence(dmabuf->resv, f, usage);
		}

		dma_resv_unlock(dmabuf->resv);
	}

	dma_fence_put(fence);

	return ret;
}
#endif

static long dma_buf_ioctl(struct file *file,
			  unsigned int cmd, unsigned long arg)
{
	struct dma_buf *dmabuf;
	struct dma_buf_sync sync;
	enum dma_data_direction direction;
	int ret;

	dmabuf = file->private_data;

	switch (cmd) {
	case DMA_BUF_IOCTL_SYNC:
		if (copy_from_user(&sync, (void __user *) arg, sizeof(sync)))
			return -EFAULT;

		if (sync.flags & ~DMA_BUF_SYNC_VALID_FLAGS_MASK)
			return -EINVAL;

		switch (sync.flags & DMA_BUF_SYNC_RW) {
		case DMA_BUF_SYNC_READ:
			direction = DMA_FROM_DEVICE;
			break;
		case DMA_BUF_SYNC_WRITE:
			direction = DMA_TO_DEVICE;
			break;
		case DMA_BUF_SYNC_RW:
			direction = DMA_BIDIRECTIONAL;
			break;
		default:
			return -EINVAL;
		}

		if (sync.flags & DMA_BUF_SYNC_END)
			ret = dma_buf_end_cpu_access(dmabuf, direction);
		else
			ret = dma_buf_begin_cpu_access(dmabuf, direction);

		return ret;

	case DMA_BUF_SET_NAME_A:
	case DMA_BUF_SET_NAME_B:
		return dma_buf_set_name(dmabuf, (const char __user *)arg);

#if IS_ENABLED(CONFIG_SYNC_FILE)
	case DMA_BUF_IOCTL_EXPORT_SYNC_FILE:
		return dma_buf_export_sync_file(dmabuf, (void __user *)arg);
	case DMA_BUF_IOCTL_IMPORT_SYNC_FILE:
		return dma_buf_import_sync_file(dmabuf, (const void __user *)arg);
#endif

	default:
		return -ENOTTY;
	}
}

static void dma_buf_show_fdinfo(struct seq_file *m, struct file *file)
{
	struct dma_buf *dmabuf = file->private_data;

	seq_printf(m, "size:\t%zu\n", dmabuf->size);
	 
	seq_printf(m, "count:\t%ld\n", file_count(dmabuf->file) - 1);
	seq_printf(m, "exp_name:\t%s\n", dmabuf->exp_name);
	spin_lock(&dmabuf->name_lock);
	if (dmabuf->name)
		seq_printf(m, "name:\t%s\n", dmabuf->name);
	spin_unlock(&dmabuf->name_lock);
}

static const struct file_operations dma_buf_fops = {
	.release	= dma_buf_file_release,
	.mmap		= dma_buf_mmap_internal,
	.llseek		= dma_buf_llseek,
	.poll		= dma_buf_poll,
	.unlocked_ioctl	= dma_buf_ioctl,
	.compat_ioctl	= compat_ptr_ioctl,
	.show_fdinfo	= dma_buf_show_fdinfo,
};

 
static inline int is_dma_buf_file(struct file *file)
{
	return file->f_op == &dma_buf_fops;
}

static struct file *dma_buf_getfile(size_t size, int flags)
{
	static atomic64_t dmabuf_inode = ATOMIC64_INIT(0);
	struct inode *inode = alloc_anon_inode(dma_buf_mnt->mnt_sb);
	struct file *file;

	if (IS_ERR(inode))
		return ERR_CAST(inode);

	inode->i_size = size;
	inode_set_bytes(inode, size);

	 
	inode->i_ino = atomic64_add_return(1, &dmabuf_inode);
	flags &= O_ACCMODE | O_NONBLOCK;
	file = alloc_file_pseudo(inode, dma_buf_mnt, "dmabuf",
				 flags, &dma_buf_fops);
	if (IS_ERR(file))
		goto err_alloc_file;

	return file;

err_alloc_file:
	iput(inode);
	return file;
}

 

 
struct dma_buf *dma_buf_export(const struct dma_buf_export_info *exp_info)
{
	struct dma_buf *dmabuf;
	struct dma_resv *resv = exp_info->resv;
	struct file *file;
	size_t alloc_size = sizeof(struct dma_buf);
	int ret;

	if (WARN_ON(!exp_info->priv || !exp_info->ops
		    || !exp_info->ops->map_dma_buf
		    || !exp_info->ops->unmap_dma_buf
		    || !exp_info->ops->release))
		return ERR_PTR(-EINVAL);

	if (WARN_ON(exp_info->ops->cache_sgt_mapping &&
		    (exp_info->ops->pin || exp_info->ops->unpin)))
		return ERR_PTR(-EINVAL);

	if (WARN_ON(!exp_info->ops->pin != !exp_info->ops->unpin))
		return ERR_PTR(-EINVAL);

	if (!try_module_get(exp_info->owner))
		return ERR_PTR(-ENOENT);

	file = dma_buf_getfile(exp_info->size, exp_info->flags);
	if (IS_ERR(file)) {
		ret = PTR_ERR(file);
		goto err_module;
	}

	if (!exp_info->resv)
		alloc_size += sizeof(struct dma_resv);
	else
		 
		alloc_size += 1;
	dmabuf = kzalloc(alloc_size, GFP_KERNEL);
	if (!dmabuf) {
		ret = -ENOMEM;
		goto err_file;
	}

	dmabuf->priv = exp_info->priv;
	dmabuf->ops = exp_info->ops;
	dmabuf->size = exp_info->size;
	dmabuf->exp_name = exp_info->exp_name;
	dmabuf->owner = exp_info->owner;
	spin_lock_init(&dmabuf->name_lock);
	init_waitqueue_head(&dmabuf->poll);
	dmabuf->cb_in.poll = dmabuf->cb_out.poll = &dmabuf->poll;
	dmabuf->cb_in.active = dmabuf->cb_out.active = 0;
	INIT_LIST_HEAD(&dmabuf->attachments);

	if (!resv) {
		dmabuf->resv = (struct dma_resv *)&dmabuf[1];
		dma_resv_init(dmabuf->resv);
	} else {
		dmabuf->resv = resv;
	}

	ret = dma_buf_stats_setup(dmabuf, file);
	if (ret)
		goto err_dmabuf;

	file->private_data = dmabuf;
	file->f_path.dentry->d_fsdata = dmabuf;
	dmabuf->file = file;

	mutex_lock(&db_list.lock);
	list_add(&dmabuf->list_node, &db_list.head);
	mutex_unlock(&db_list.lock);

	return dmabuf;

err_dmabuf:
	if (!resv)
		dma_resv_fini(dmabuf->resv);
	kfree(dmabuf);
err_file:
	fput(file);
err_module:
	module_put(exp_info->owner);
	return ERR_PTR(ret);
}
EXPORT_SYMBOL_NS_GPL(dma_buf_export, DMA_BUF);

 
int dma_buf_fd(struct dma_buf *dmabuf, int flags)
{
	int fd;

	if (!dmabuf || !dmabuf->file)
		return -EINVAL;

	fd = get_unused_fd_flags(flags);
	if (fd < 0)
		return fd;

	fd_install(fd, dmabuf->file);

	return fd;
}
EXPORT_SYMBOL_NS_GPL(dma_buf_fd, DMA_BUF);

 
struct dma_buf *dma_buf_get(int fd)
{
	struct file *file;

	file = fget(fd);

	if (!file)
		return ERR_PTR(-EBADF);

	if (!is_dma_buf_file(file)) {
		fput(file);
		return ERR_PTR(-EINVAL);
	}

	return file->private_data;
}
EXPORT_SYMBOL_NS_GPL(dma_buf_get, DMA_BUF);

 
void dma_buf_put(struct dma_buf *dmabuf)
{
	if (WARN_ON(!dmabuf || !dmabuf->file))
		return;

	fput(dmabuf->file);
}
EXPORT_SYMBOL_NS_GPL(dma_buf_put, DMA_BUF);

static void mangle_sg_table(struct sg_table *sg_table)
{
#ifdef CONFIG_DMABUF_DEBUG
	int i;
	struct scatterlist *sg;

	 
	for_each_sgtable_sg(sg_table, sg, i)
		sg->page_link ^= ~0xffUL;
#endif

}
static struct sg_table * __map_dma_buf(struct dma_buf_attachment *attach,
				       enum dma_data_direction direction)
{
	struct sg_table *sg_table;
	signed long ret;

	sg_table = attach->dmabuf->ops->map_dma_buf(attach, direction);
	if (IS_ERR_OR_NULL(sg_table))
		return sg_table;

	if (!dma_buf_attachment_is_dynamic(attach)) {
		ret = dma_resv_wait_timeout(attach->dmabuf->resv,
					    DMA_RESV_USAGE_KERNEL, true,
					    MAX_SCHEDULE_TIMEOUT);
		if (ret < 0) {
			attach->dmabuf->ops->unmap_dma_buf(attach, sg_table,
							   direction);
			return ERR_PTR(ret);
		}
	}

	mangle_sg_table(sg_table);
	return sg_table;
}

 

 
struct dma_buf_attachment *
dma_buf_dynamic_attach(struct dma_buf *dmabuf, struct device *dev,
		       const struct dma_buf_attach_ops *importer_ops,
		       void *importer_priv)
{
	struct dma_buf_attachment *attach;
	int ret;

	if (WARN_ON(!dmabuf || !dev))
		return ERR_PTR(-EINVAL);

	if (WARN_ON(importer_ops && !importer_ops->move_notify))
		return ERR_PTR(-EINVAL);

	attach = kzalloc(sizeof(*attach), GFP_KERNEL);
	if (!attach)
		return ERR_PTR(-ENOMEM);

	attach->dev = dev;
	attach->dmabuf = dmabuf;
	if (importer_ops)
		attach->peer2peer = importer_ops->allow_peer2peer;
	attach->importer_ops = importer_ops;
	attach->importer_priv = importer_priv;

	if (dmabuf->ops->attach) {
		ret = dmabuf->ops->attach(dmabuf, attach);
		if (ret)
			goto err_attach;
	}
	dma_resv_lock(dmabuf->resv, NULL);
	list_add(&attach->node, &dmabuf->attachments);
	dma_resv_unlock(dmabuf->resv);

	 
	if (dma_buf_attachment_is_dynamic(attach) !=
	    dma_buf_is_dynamic(dmabuf)) {
		struct sg_table *sgt;

		dma_resv_lock(attach->dmabuf->resv, NULL);
		if (dma_buf_is_dynamic(attach->dmabuf)) {
			ret = dmabuf->ops->pin(attach);
			if (ret)
				goto err_unlock;
		}

		sgt = __map_dma_buf(attach, DMA_BIDIRECTIONAL);
		if (!sgt)
			sgt = ERR_PTR(-ENOMEM);
		if (IS_ERR(sgt)) {
			ret = PTR_ERR(sgt);
			goto err_unpin;
		}
		dma_resv_unlock(attach->dmabuf->resv);
		attach->sgt = sgt;
		attach->dir = DMA_BIDIRECTIONAL;
	}

	return attach;

err_attach:
	kfree(attach);
	return ERR_PTR(ret);

err_unpin:
	if (dma_buf_is_dynamic(attach->dmabuf))
		dmabuf->ops->unpin(attach);

err_unlock:
	dma_resv_unlock(attach->dmabuf->resv);

	dma_buf_detach(dmabuf, attach);
	return ERR_PTR(ret);
}
EXPORT_SYMBOL_NS_GPL(dma_buf_dynamic_attach, DMA_BUF);

 
struct dma_buf_attachment *dma_buf_attach(struct dma_buf *dmabuf,
					  struct device *dev)
{
	return dma_buf_dynamic_attach(dmabuf, dev, NULL, NULL);
}
EXPORT_SYMBOL_NS_GPL(dma_buf_attach, DMA_BUF);

static void __unmap_dma_buf(struct dma_buf_attachment *attach,
			    struct sg_table *sg_table,
			    enum dma_data_direction direction)
{
	 
	mangle_sg_table(sg_table);

	attach->dmabuf->ops->unmap_dma_buf(attach, sg_table, direction);
}

 
void dma_buf_detach(struct dma_buf *dmabuf, struct dma_buf_attachment *attach)
{
	if (WARN_ON(!dmabuf || !attach || dmabuf != attach->dmabuf))
		return;

	dma_resv_lock(dmabuf->resv, NULL);

	if (attach->sgt) {

		__unmap_dma_buf(attach, attach->sgt, attach->dir);

		if (dma_buf_is_dynamic(attach->dmabuf))
			dmabuf->ops->unpin(attach);
	}
	list_del(&attach->node);

	dma_resv_unlock(dmabuf->resv);

	if (dmabuf->ops->detach)
		dmabuf->ops->detach(dmabuf, attach);

	kfree(attach);
}
EXPORT_SYMBOL_NS_GPL(dma_buf_detach, DMA_BUF);

 
int dma_buf_pin(struct dma_buf_attachment *attach)
{
	struct dma_buf *dmabuf = attach->dmabuf;
	int ret = 0;

	WARN_ON(!dma_buf_attachment_is_dynamic(attach));

	dma_resv_assert_held(dmabuf->resv);

	if (dmabuf->ops->pin)
		ret = dmabuf->ops->pin(attach);

	return ret;
}
EXPORT_SYMBOL_NS_GPL(dma_buf_pin, DMA_BUF);

 
void dma_buf_unpin(struct dma_buf_attachment *attach)
{
	struct dma_buf *dmabuf = attach->dmabuf;

	WARN_ON(!dma_buf_attachment_is_dynamic(attach));

	dma_resv_assert_held(dmabuf->resv);

	if (dmabuf->ops->unpin)
		dmabuf->ops->unpin(attach);
}
EXPORT_SYMBOL_NS_GPL(dma_buf_unpin, DMA_BUF);

 
struct sg_table *dma_buf_map_attachment(struct dma_buf_attachment *attach,
					enum dma_data_direction direction)
{
	struct sg_table *sg_table;
	int r;

	might_sleep();

	if (WARN_ON(!attach || !attach->dmabuf))
		return ERR_PTR(-EINVAL);

	dma_resv_assert_held(attach->dmabuf->resv);

	if (attach->sgt) {
		 
		if (attach->dir != direction &&
		    attach->dir != DMA_BIDIRECTIONAL)
			return ERR_PTR(-EBUSY);

		return attach->sgt;
	}

	if (dma_buf_is_dynamic(attach->dmabuf)) {
		if (!IS_ENABLED(CONFIG_DMABUF_MOVE_NOTIFY)) {
			r = attach->dmabuf->ops->pin(attach);
			if (r)
				return ERR_PTR(r);
		}
	}

	sg_table = __map_dma_buf(attach, direction);
	if (!sg_table)
		sg_table = ERR_PTR(-ENOMEM);

	if (IS_ERR(sg_table) && dma_buf_is_dynamic(attach->dmabuf) &&
	     !IS_ENABLED(CONFIG_DMABUF_MOVE_NOTIFY))
		attach->dmabuf->ops->unpin(attach);

	if (!IS_ERR(sg_table) && attach->dmabuf->ops->cache_sgt_mapping) {
		attach->sgt = sg_table;
		attach->dir = direction;
	}

#ifdef CONFIG_DMA_API_DEBUG
	if (!IS_ERR(sg_table)) {
		struct scatterlist *sg;
		u64 addr;
		int len;
		int i;

		for_each_sgtable_dma_sg(sg_table, sg, i) {
			addr = sg_dma_address(sg);
			len = sg_dma_len(sg);
			if (!PAGE_ALIGNED(addr) || !PAGE_ALIGNED(len)) {
				pr_debug("%s: addr %llx or len %x is not page aligned!\n",
					 __func__, addr, len);
			}
		}
	}
#endif  
	return sg_table;
}
EXPORT_SYMBOL_NS_GPL(dma_buf_map_attachment, DMA_BUF);

 
struct sg_table *
dma_buf_map_attachment_unlocked(struct dma_buf_attachment *attach,
				enum dma_data_direction direction)
{
	struct sg_table *sg_table;

	might_sleep();

	if (WARN_ON(!attach || !attach->dmabuf))
		return ERR_PTR(-EINVAL);

	dma_resv_lock(attach->dmabuf->resv, NULL);
	sg_table = dma_buf_map_attachment(attach, direction);
	dma_resv_unlock(attach->dmabuf->resv);

	return sg_table;
}
EXPORT_SYMBOL_NS_GPL(dma_buf_map_attachment_unlocked, DMA_BUF);

 
void dma_buf_unmap_attachment(struct dma_buf_attachment *attach,
				struct sg_table *sg_table,
				enum dma_data_direction direction)
{
	might_sleep();

	if (WARN_ON(!attach || !attach->dmabuf || !sg_table))
		return;

	dma_resv_assert_held(attach->dmabuf->resv);

	if (attach->sgt == sg_table)
		return;

	__unmap_dma_buf(attach, sg_table, direction);

	if (dma_buf_is_dynamic(attach->dmabuf) &&
	    !IS_ENABLED(CONFIG_DMABUF_MOVE_NOTIFY))
		dma_buf_unpin(attach);
}
EXPORT_SYMBOL_NS_GPL(dma_buf_unmap_attachment, DMA_BUF);

 
void dma_buf_unmap_attachment_unlocked(struct dma_buf_attachment *attach,
				       struct sg_table *sg_table,
				       enum dma_data_direction direction)
{
	might_sleep();

	if (WARN_ON(!attach || !attach->dmabuf || !sg_table))
		return;

	dma_resv_lock(attach->dmabuf->resv, NULL);
	dma_buf_unmap_attachment(attach, sg_table, direction);
	dma_resv_unlock(attach->dmabuf->resv);
}
EXPORT_SYMBOL_NS_GPL(dma_buf_unmap_attachment_unlocked, DMA_BUF);

 
void dma_buf_move_notify(struct dma_buf *dmabuf)
{
	struct dma_buf_attachment *attach;

	dma_resv_assert_held(dmabuf->resv);

	list_for_each_entry(attach, &dmabuf->attachments, node)
		if (attach->importer_ops)
			attach->importer_ops->move_notify(attach);
}
EXPORT_SYMBOL_NS_GPL(dma_buf_move_notify, DMA_BUF);

 

static int __dma_buf_begin_cpu_access(struct dma_buf *dmabuf,
				      enum dma_data_direction direction)
{
	bool write = (direction == DMA_BIDIRECTIONAL ||
		      direction == DMA_TO_DEVICE);
	struct dma_resv *resv = dmabuf->resv;
	long ret;

	 
	ret = dma_resv_wait_timeout(resv, dma_resv_usage_rw(write),
				    true, MAX_SCHEDULE_TIMEOUT);
	if (ret < 0)
		return ret;

	return 0;
}

 
int dma_buf_begin_cpu_access(struct dma_buf *dmabuf,
			     enum dma_data_direction direction)
{
	int ret = 0;

	if (WARN_ON(!dmabuf))
		return -EINVAL;

	might_lock(&dmabuf->resv->lock.base);

	if (dmabuf->ops->begin_cpu_access)
		ret = dmabuf->ops->begin_cpu_access(dmabuf, direction);

	 
	if (ret == 0)
		ret = __dma_buf_begin_cpu_access(dmabuf, direction);

	return ret;
}
EXPORT_SYMBOL_NS_GPL(dma_buf_begin_cpu_access, DMA_BUF);

 
int dma_buf_end_cpu_access(struct dma_buf *dmabuf,
			   enum dma_data_direction direction)
{
	int ret = 0;

	WARN_ON(!dmabuf);

	might_lock(&dmabuf->resv->lock.base);

	if (dmabuf->ops->end_cpu_access)
		ret = dmabuf->ops->end_cpu_access(dmabuf, direction);

	return ret;
}
EXPORT_SYMBOL_NS_GPL(dma_buf_end_cpu_access, DMA_BUF);


 
int dma_buf_mmap(struct dma_buf *dmabuf, struct vm_area_struct *vma,
		 unsigned long pgoff)
{
	if (WARN_ON(!dmabuf || !vma))
		return -EINVAL;

	 
	if (!dmabuf->ops->mmap)
		return -EINVAL;

	 
	if (pgoff + vma_pages(vma) < pgoff)
		return -EOVERFLOW;

	 
	if (pgoff + vma_pages(vma) >
	    dmabuf->size >> PAGE_SHIFT)
		return -EINVAL;

	 
	vma_set_file(vma, dmabuf->file);
	vma->vm_pgoff = pgoff;

	return dmabuf->ops->mmap(dmabuf, vma);
}
EXPORT_SYMBOL_NS_GPL(dma_buf_mmap, DMA_BUF);

 
int dma_buf_vmap(struct dma_buf *dmabuf, struct iosys_map *map)
{
	struct iosys_map ptr;
	int ret;

	iosys_map_clear(map);

	if (WARN_ON(!dmabuf))
		return -EINVAL;

	dma_resv_assert_held(dmabuf->resv);

	if (!dmabuf->ops->vmap)
		return -EINVAL;

	if (dmabuf->vmapping_counter) {
		dmabuf->vmapping_counter++;
		BUG_ON(iosys_map_is_null(&dmabuf->vmap_ptr));
		*map = dmabuf->vmap_ptr;
		return 0;
	}

	BUG_ON(iosys_map_is_set(&dmabuf->vmap_ptr));

	ret = dmabuf->ops->vmap(dmabuf, &ptr);
	if (WARN_ON_ONCE(ret))
		return ret;

	dmabuf->vmap_ptr = ptr;
	dmabuf->vmapping_counter = 1;

	*map = dmabuf->vmap_ptr;

	return 0;
}
EXPORT_SYMBOL_NS_GPL(dma_buf_vmap, DMA_BUF);

 
int dma_buf_vmap_unlocked(struct dma_buf *dmabuf, struct iosys_map *map)
{
	int ret;

	iosys_map_clear(map);

	if (WARN_ON(!dmabuf))
		return -EINVAL;

	dma_resv_lock(dmabuf->resv, NULL);
	ret = dma_buf_vmap(dmabuf, map);
	dma_resv_unlock(dmabuf->resv);

	return ret;
}
EXPORT_SYMBOL_NS_GPL(dma_buf_vmap_unlocked, DMA_BUF);

 
void dma_buf_vunmap(struct dma_buf *dmabuf, struct iosys_map *map)
{
	if (WARN_ON(!dmabuf))
		return;

	dma_resv_assert_held(dmabuf->resv);

	BUG_ON(iosys_map_is_null(&dmabuf->vmap_ptr));
	BUG_ON(dmabuf->vmapping_counter == 0);
	BUG_ON(!iosys_map_is_equal(&dmabuf->vmap_ptr, map));

	if (--dmabuf->vmapping_counter == 0) {
		if (dmabuf->ops->vunmap)
			dmabuf->ops->vunmap(dmabuf, map);
		iosys_map_clear(&dmabuf->vmap_ptr);
	}
}
EXPORT_SYMBOL_NS_GPL(dma_buf_vunmap, DMA_BUF);

 
void dma_buf_vunmap_unlocked(struct dma_buf *dmabuf, struct iosys_map *map)
{
	if (WARN_ON(!dmabuf))
		return;

	dma_resv_lock(dmabuf->resv, NULL);
	dma_buf_vunmap(dmabuf, map);
	dma_resv_unlock(dmabuf->resv);
}
EXPORT_SYMBOL_NS_GPL(dma_buf_vunmap_unlocked, DMA_BUF);

#ifdef CONFIG_DEBUG_FS
static int dma_buf_debug_show(struct seq_file *s, void *unused)
{
	struct dma_buf *buf_obj;
	struct dma_buf_attachment *attach_obj;
	int count = 0, attach_count;
	size_t size = 0;
	int ret;

	ret = mutex_lock_interruptible(&db_list.lock);

	if (ret)
		return ret;

	seq_puts(s, "\nDma-buf Objects:\n");
	seq_printf(s, "%-8s\t%-8s\t%-8s\t%-8s\texp_name\t%-8s\tname\n",
		   "size", "flags", "mode", "count", "ino");

	list_for_each_entry(buf_obj, &db_list.head, list_node) {

		ret = dma_resv_lock_interruptible(buf_obj->resv, NULL);
		if (ret)
			goto error_unlock;


		spin_lock(&buf_obj->name_lock);
		seq_printf(s, "%08zu\t%08x\t%08x\t%08ld\t%s\t%08lu\t%s\n",
				buf_obj->size,
				buf_obj->file->f_flags, buf_obj->file->f_mode,
				file_count(buf_obj->file),
				buf_obj->exp_name,
				file_inode(buf_obj->file)->i_ino,
				buf_obj->name ?: "<none>");
		spin_unlock(&buf_obj->name_lock);

		dma_resv_describe(buf_obj->resv, s);

		seq_puts(s, "\tAttached Devices:\n");
		attach_count = 0;

		list_for_each_entry(attach_obj, &buf_obj->attachments, node) {
			seq_printf(s, "\t%s\n", dev_name(attach_obj->dev));
			attach_count++;
		}
		dma_resv_unlock(buf_obj->resv);

		seq_printf(s, "Total %d devices attached\n\n",
				attach_count);

		count++;
		size += buf_obj->size;
	}

	seq_printf(s, "\nTotal %d objects, %zu bytes\n", count, size);

	mutex_unlock(&db_list.lock);
	return 0;

error_unlock:
	mutex_unlock(&db_list.lock);
	return ret;
}

DEFINE_SHOW_ATTRIBUTE(dma_buf_debug);

static struct dentry *dma_buf_debugfs_dir;

static int dma_buf_init_debugfs(void)
{
	struct dentry *d;
	int err = 0;

	d = debugfs_create_dir("dma_buf", NULL);
	if (IS_ERR(d))
		return PTR_ERR(d);

	dma_buf_debugfs_dir = d;

	d = debugfs_create_file("bufinfo", S_IRUGO, dma_buf_debugfs_dir,
				NULL, &dma_buf_debug_fops);
	if (IS_ERR(d)) {
		pr_debug("dma_buf: debugfs: failed to create node bufinfo\n");
		debugfs_remove_recursive(dma_buf_debugfs_dir);
		dma_buf_debugfs_dir = NULL;
		err = PTR_ERR(d);
	}

	return err;
}

static void dma_buf_uninit_debugfs(void)
{
	debugfs_remove_recursive(dma_buf_debugfs_dir);
}
#else
static inline int dma_buf_init_debugfs(void)
{
	return 0;
}
static inline void dma_buf_uninit_debugfs(void)
{
}
#endif

static int __init dma_buf_init(void)
{
	int ret;

	ret = dma_buf_init_sysfs_statistics();
	if (ret)
		return ret;

	dma_buf_mnt = kern_mount(&dma_buf_fs_type);
	if (IS_ERR(dma_buf_mnt))
		return PTR_ERR(dma_buf_mnt);

	mutex_init(&db_list.lock);
	INIT_LIST_HEAD(&db_list.head);
	dma_buf_init_debugfs();
	return 0;
}
subsys_initcall(dma_buf_init);

static void __exit dma_buf_deinit(void)
{
	dma_buf_uninit_debugfs();
	kern_unmount(dma_buf_mnt);
	dma_buf_uninit_sysfs_statistics();
}
__exitcall(dma_buf_deinit);
