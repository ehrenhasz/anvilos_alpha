
 

#include <linux/fs.h>
#include <linux/seq_file.h>
#include <linux/slab.h>
#include <linux/poll.h>
#include <linux/pagemap.h>
#include <linux/sched/mm.h>
#include <linux/fsnotify.h>
#include <linux/uio.h>

#include "kernfs-internal.h"

struct kernfs_open_node {
	struct rcu_head		rcu_head;
	atomic_t		event;
	wait_queue_head_t	poll;
	struct list_head	files;  
	unsigned int		nr_mmapped;
	unsigned int		nr_to_release;
};

 
#define KERNFS_NOTIFY_EOL			((void *)&kernfs_notify_list)

static DEFINE_SPINLOCK(kernfs_notify_lock);
static struct kernfs_node *kernfs_notify_list = KERNFS_NOTIFY_EOL;

static inline struct mutex *kernfs_open_file_mutex_ptr(struct kernfs_node *kn)
{
	int idx = hash_ptr(kn, NR_KERNFS_LOCK_BITS);

	return &kernfs_locks->open_file_mutex[idx];
}

static inline struct mutex *kernfs_open_file_mutex_lock(struct kernfs_node *kn)
{
	struct mutex *lock;

	lock = kernfs_open_file_mutex_ptr(kn);

	mutex_lock(lock);

	return lock;
}

 
static struct kernfs_open_node *of_on(struct kernfs_open_file *of)
{
	return rcu_dereference_protected(of->kn->attr.open,
					 !list_empty(&of->list));
}

 
static struct kernfs_open_node *
kernfs_deref_open_node_locked(struct kernfs_node *kn)
{
	return rcu_dereference_protected(kn->attr.open,
				lockdep_is_held(kernfs_open_file_mutex_ptr(kn)));
}

static struct kernfs_open_file *kernfs_of(struct file *file)
{
	return ((struct seq_file *)file->private_data)->private;
}

 
static const struct kernfs_ops *kernfs_ops(struct kernfs_node *kn)
{
	if (kn->flags & KERNFS_LOCKDEP)
		lockdep_assert_held(kn);
	return kn->attr.ops;
}

 
static void kernfs_seq_stop_active(struct seq_file *sf, void *v)
{
	struct kernfs_open_file *of = sf->private;
	const struct kernfs_ops *ops = kernfs_ops(of->kn);

	if (ops->seq_stop)
		ops->seq_stop(sf, v);
	kernfs_put_active(of->kn);
}

static void *kernfs_seq_start(struct seq_file *sf, loff_t *ppos)
{
	struct kernfs_open_file *of = sf->private;
	const struct kernfs_ops *ops;

	 
	mutex_lock(&of->mutex);
	if (!kernfs_get_active(of->kn))
		return ERR_PTR(-ENODEV);

	ops = kernfs_ops(of->kn);
	if (ops->seq_start) {
		void *next = ops->seq_start(sf, ppos);
		 
		if (next == ERR_PTR(-ENODEV))
			kernfs_seq_stop_active(sf, next);
		return next;
	}
	return single_start(sf, ppos);
}

static void *kernfs_seq_next(struct seq_file *sf, void *v, loff_t *ppos)
{
	struct kernfs_open_file *of = sf->private;
	const struct kernfs_ops *ops = kernfs_ops(of->kn);

	if (ops->seq_next) {
		void *next = ops->seq_next(sf, v, ppos);
		 
		if (next == ERR_PTR(-ENODEV))
			kernfs_seq_stop_active(sf, next);
		return next;
	} else {
		 
		++*ppos;
		return NULL;
	}
}

static void kernfs_seq_stop(struct seq_file *sf, void *v)
{
	struct kernfs_open_file *of = sf->private;

	if (v != ERR_PTR(-ENODEV))
		kernfs_seq_stop_active(sf, v);
	mutex_unlock(&of->mutex);
}

static int kernfs_seq_show(struct seq_file *sf, void *v)
{
	struct kernfs_open_file *of = sf->private;

	of->event = atomic_read(&of_on(of)->event);

	return of->kn->attr.ops->seq_show(sf, v);
}

static const struct seq_operations kernfs_seq_ops = {
	.start = kernfs_seq_start,
	.next = kernfs_seq_next,
	.stop = kernfs_seq_stop,
	.show = kernfs_seq_show,
};

 
static ssize_t kernfs_file_read_iter(struct kiocb *iocb, struct iov_iter *iter)
{
	struct kernfs_open_file *of = kernfs_of(iocb->ki_filp);
	ssize_t len = min_t(size_t, iov_iter_count(iter), PAGE_SIZE);
	const struct kernfs_ops *ops;
	char *buf;

	buf = of->prealloc_buf;
	if (buf)
		mutex_lock(&of->prealloc_mutex);
	else
		buf = kmalloc(len, GFP_KERNEL);
	if (!buf)
		return -ENOMEM;

	 
	mutex_lock(&of->mutex);
	if (!kernfs_get_active(of->kn)) {
		len = -ENODEV;
		mutex_unlock(&of->mutex);
		goto out_free;
	}

	of->event = atomic_read(&of_on(of)->event);

	ops = kernfs_ops(of->kn);
	if (ops->read)
		len = ops->read(of, buf, len, iocb->ki_pos);
	else
		len = -EINVAL;

	kernfs_put_active(of->kn);
	mutex_unlock(&of->mutex);

	if (len < 0)
		goto out_free;

	if (copy_to_iter(buf, len, iter) != len) {
		len = -EFAULT;
		goto out_free;
	}

	iocb->ki_pos += len;

 out_free:
	if (buf == of->prealloc_buf)
		mutex_unlock(&of->prealloc_mutex);
	else
		kfree(buf);
	return len;
}

static ssize_t kernfs_fop_read_iter(struct kiocb *iocb, struct iov_iter *iter)
{
	if (kernfs_of(iocb->ki_filp)->kn->flags & KERNFS_HAS_SEQ_SHOW)
		return seq_read_iter(iocb, iter);
	return kernfs_file_read_iter(iocb, iter);
}

 
static ssize_t kernfs_fop_write_iter(struct kiocb *iocb, struct iov_iter *iter)
{
	struct kernfs_open_file *of = kernfs_of(iocb->ki_filp);
	ssize_t len = iov_iter_count(iter);
	const struct kernfs_ops *ops;
	char *buf;

	if (of->atomic_write_len) {
		if (len > of->atomic_write_len)
			return -E2BIG;
	} else {
		len = min_t(size_t, len, PAGE_SIZE);
	}

	buf = of->prealloc_buf;
	if (buf)
		mutex_lock(&of->prealloc_mutex);
	else
		buf = kmalloc(len + 1, GFP_KERNEL);
	if (!buf)
		return -ENOMEM;

	if (copy_from_iter(buf, len, iter) != len) {
		len = -EFAULT;
		goto out_free;
	}
	buf[len] = '\0';	 

	 
	mutex_lock(&of->mutex);
	if (!kernfs_get_active(of->kn)) {
		mutex_unlock(&of->mutex);
		len = -ENODEV;
		goto out_free;
	}

	ops = kernfs_ops(of->kn);
	if (ops->write)
		len = ops->write(of, buf, len, iocb->ki_pos);
	else
		len = -EINVAL;

	kernfs_put_active(of->kn);
	mutex_unlock(&of->mutex);

	if (len > 0)
		iocb->ki_pos += len;

out_free:
	if (buf == of->prealloc_buf)
		mutex_unlock(&of->prealloc_mutex);
	else
		kfree(buf);
	return len;
}

static void kernfs_vma_open(struct vm_area_struct *vma)
{
	struct file *file = vma->vm_file;
	struct kernfs_open_file *of = kernfs_of(file);

	if (!of->vm_ops)
		return;

	if (!kernfs_get_active(of->kn))
		return;

	if (of->vm_ops->open)
		of->vm_ops->open(vma);

	kernfs_put_active(of->kn);
}

static vm_fault_t kernfs_vma_fault(struct vm_fault *vmf)
{
	struct file *file = vmf->vma->vm_file;
	struct kernfs_open_file *of = kernfs_of(file);
	vm_fault_t ret;

	if (!of->vm_ops)
		return VM_FAULT_SIGBUS;

	if (!kernfs_get_active(of->kn))
		return VM_FAULT_SIGBUS;

	ret = VM_FAULT_SIGBUS;
	if (of->vm_ops->fault)
		ret = of->vm_ops->fault(vmf);

	kernfs_put_active(of->kn);
	return ret;
}

static vm_fault_t kernfs_vma_page_mkwrite(struct vm_fault *vmf)
{
	struct file *file = vmf->vma->vm_file;
	struct kernfs_open_file *of = kernfs_of(file);
	vm_fault_t ret;

	if (!of->vm_ops)
		return VM_FAULT_SIGBUS;

	if (!kernfs_get_active(of->kn))
		return VM_FAULT_SIGBUS;

	ret = 0;
	if (of->vm_ops->page_mkwrite)
		ret = of->vm_ops->page_mkwrite(vmf);
	else
		file_update_time(file);

	kernfs_put_active(of->kn);
	return ret;
}

static int kernfs_vma_access(struct vm_area_struct *vma, unsigned long addr,
			     void *buf, int len, int write)
{
	struct file *file = vma->vm_file;
	struct kernfs_open_file *of = kernfs_of(file);
	int ret;

	if (!of->vm_ops)
		return -EINVAL;

	if (!kernfs_get_active(of->kn))
		return -EINVAL;

	ret = -EINVAL;
	if (of->vm_ops->access)
		ret = of->vm_ops->access(vma, addr, buf, len, write);

	kernfs_put_active(of->kn);
	return ret;
}

#ifdef CONFIG_NUMA
static int kernfs_vma_set_policy(struct vm_area_struct *vma,
				 struct mempolicy *new)
{
	struct file *file = vma->vm_file;
	struct kernfs_open_file *of = kernfs_of(file);
	int ret;

	if (!of->vm_ops)
		return 0;

	if (!kernfs_get_active(of->kn))
		return -EINVAL;

	ret = 0;
	if (of->vm_ops->set_policy)
		ret = of->vm_ops->set_policy(vma, new);

	kernfs_put_active(of->kn);
	return ret;
}

static struct mempolicy *kernfs_vma_get_policy(struct vm_area_struct *vma,
					       unsigned long addr)
{
	struct file *file = vma->vm_file;
	struct kernfs_open_file *of = kernfs_of(file);
	struct mempolicy *pol;

	if (!of->vm_ops)
		return vma->vm_policy;

	if (!kernfs_get_active(of->kn))
		return vma->vm_policy;

	pol = vma->vm_policy;
	if (of->vm_ops->get_policy)
		pol = of->vm_ops->get_policy(vma, addr);

	kernfs_put_active(of->kn);
	return pol;
}

#endif

static const struct vm_operations_struct kernfs_vm_ops = {
	.open		= kernfs_vma_open,
	.fault		= kernfs_vma_fault,
	.page_mkwrite	= kernfs_vma_page_mkwrite,
	.access		= kernfs_vma_access,
#ifdef CONFIG_NUMA
	.set_policy	= kernfs_vma_set_policy,
	.get_policy	= kernfs_vma_get_policy,
#endif
};

static int kernfs_fop_mmap(struct file *file, struct vm_area_struct *vma)
{
	struct kernfs_open_file *of = kernfs_of(file);
	const struct kernfs_ops *ops;
	int rc;

	 
	if (!(of->kn->flags & KERNFS_HAS_MMAP))
		return -ENODEV;

	mutex_lock(&of->mutex);

	rc = -ENODEV;
	if (!kernfs_get_active(of->kn))
		goto out_unlock;

	ops = kernfs_ops(of->kn);
	rc = ops->mmap(of, vma);
	if (rc)
		goto out_put;

	 
	if (vma->vm_file != file)
		goto out_put;

	rc = -EINVAL;
	if (of->mmapped && of->vm_ops != vma->vm_ops)
		goto out_put;

	 
	if (vma->vm_ops && vma->vm_ops->close)
		goto out_put;

	rc = 0;
	of->mmapped = true;
	of_on(of)->nr_mmapped++;
	of->vm_ops = vma->vm_ops;
	vma->vm_ops = &kernfs_vm_ops;
out_put:
	kernfs_put_active(of->kn);
out_unlock:
	mutex_unlock(&of->mutex);

	return rc;
}

 
static int kernfs_get_open_node(struct kernfs_node *kn,
				struct kernfs_open_file *of)
{
	struct kernfs_open_node *on;
	struct mutex *mutex;

	mutex = kernfs_open_file_mutex_lock(kn);
	on = kernfs_deref_open_node_locked(kn);

	if (!on) {
		 
		on = kzalloc(sizeof(*on), GFP_KERNEL);
		if (!on) {
			mutex_unlock(mutex);
			return -ENOMEM;
		}
		atomic_set(&on->event, 1);
		init_waitqueue_head(&on->poll);
		INIT_LIST_HEAD(&on->files);
		rcu_assign_pointer(kn->attr.open, on);
	}

	list_add_tail(&of->list, &on->files);
	if (kn->flags & KERNFS_HAS_RELEASE)
		on->nr_to_release++;

	mutex_unlock(mutex);
	return 0;
}

 
static void kernfs_unlink_open_file(struct kernfs_node *kn,
				    struct kernfs_open_file *of,
				    bool open_failed)
{
	struct kernfs_open_node *on;
	struct mutex *mutex;

	mutex = kernfs_open_file_mutex_lock(kn);

	on = kernfs_deref_open_node_locked(kn);
	if (!on) {
		mutex_unlock(mutex);
		return;
	}

	if (of) {
		if (kn->flags & KERNFS_HAS_RELEASE) {
			WARN_ON_ONCE(of->released == open_failed);
			if (open_failed)
				on->nr_to_release--;
		}
		if (of->mmapped)
			on->nr_mmapped--;
		list_del(&of->list);
	}

	if (list_empty(&on->files)) {
		rcu_assign_pointer(kn->attr.open, NULL);
		kfree_rcu(on, rcu_head);
	}

	mutex_unlock(mutex);
}

static int kernfs_fop_open(struct inode *inode, struct file *file)
{
	struct kernfs_node *kn = inode->i_private;
	struct kernfs_root *root = kernfs_root(kn);
	const struct kernfs_ops *ops;
	struct kernfs_open_file *of;
	bool has_read, has_write, has_mmap;
	int error = -EACCES;

	if (!kernfs_get_active(kn))
		return -ENODEV;

	ops = kernfs_ops(kn);

	has_read = ops->seq_show || ops->read || ops->mmap;
	has_write = ops->write || ops->mmap;
	has_mmap = ops->mmap;

	 
	if (root->flags & KERNFS_ROOT_EXTRA_OPEN_PERM_CHECK) {
		if ((file->f_mode & FMODE_WRITE) &&
		    (!(inode->i_mode & S_IWUGO) || !has_write))
			goto err_out;

		if ((file->f_mode & FMODE_READ) &&
		    (!(inode->i_mode & S_IRUGO) || !has_read))
			goto err_out;
	}

	 
	error = -ENOMEM;
	of = kzalloc(sizeof(struct kernfs_open_file), GFP_KERNEL);
	if (!of)
		goto err_out;

	 
	if (has_mmap)
		mutex_init(&of->mutex);
	else
		mutex_init(&of->mutex);

	of->kn = kn;
	of->file = file;

	 
	of->atomic_write_len = ops->atomic_write_len;

	error = -EINVAL;
	 
	if (ops->prealloc && ops->seq_show)
		goto err_free;
	if (ops->prealloc) {
		int len = of->atomic_write_len ?: PAGE_SIZE;
		of->prealloc_buf = kmalloc(len + 1, GFP_KERNEL);
		error = -ENOMEM;
		if (!of->prealloc_buf)
			goto err_free;
		mutex_init(&of->prealloc_mutex);
	}

	 
	if (ops->seq_show)
		error = seq_open(file, &kernfs_seq_ops);
	else
		error = seq_open(file, NULL);
	if (error)
		goto err_free;

	of->seq_file = file->private_data;
	of->seq_file->private = of;

	 
	if (file->f_mode & FMODE_WRITE)
		file->f_mode |= FMODE_PWRITE;

	 
	error = kernfs_get_open_node(kn, of);
	if (error)
		goto err_seq_release;

	if (ops->open) {
		 
		error = ops->open(of);
		if (error)
			goto err_put_node;
	}

	 
	kernfs_put_active(kn);
	return 0;

err_put_node:
	kernfs_unlink_open_file(kn, of, true);
err_seq_release:
	seq_release(inode, file);
err_free:
	kfree(of->prealloc_buf);
	kfree(of);
err_out:
	kernfs_put_active(kn);
	return error;
}

 
static void kernfs_release_file(struct kernfs_node *kn,
				struct kernfs_open_file *of)
{
	 
	lockdep_assert_held(kernfs_open_file_mutex_ptr(kn));

	if (!of->released) {
		 
		kn->attr.ops->release(of);
		of->released = true;
		of_on(of)->nr_to_release--;
	}
}

static int kernfs_fop_release(struct inode *inode, struct file *filp)
{
	struct kernfs_node *kn = inode->i_private;
	struct kernfs_open_file *of = kernfs_of(filp);

	if (kn->flags & KERNFS_HAS_RELEASE) {
		struct mutex *mutex;

		mutex = kernfs_open_file_mutex_lock(kn);
		kernfs_release_file(kn, of);
		mutex_unlock(mutex);
	}

	kernfs_unlink_open_file(kn, of, false);
	seq_release(inode, filp);
	kfree(of->prealloc_buf);
	kfree(of);

	return 0;
}

bool kernfs_should_drain_open_files(struct kernfs_node *kn)
{
	struct kernfs_open_node *on;
	bool ret;

	 
	WARN_ON_ONCE(atomic_read(&kn->active) != KN_DEACTIVATED_BIAS);

	rcu_read_lock();
	on = rcu_dereference(kn->attr.open);
	ret = on && (on->nr_mmapped || on->nr_to_release);
	rcu_read_unlock();

	return ret;
}

void kernfs_drain_open_files(struct kernfs_node *kn)
{
	struct kernfs_open_node *on;
	struct kernfs_open_file *of;
	struct mutex *mutex;

	mutex = kernfs_open_file_mutex_lock(kn);
	on = kernfs_deref_open_node_locked(kn);
	if (!on) {
		mutex_unlock(mutex);
		return;
	}

	list_for_each_entry(of, &on->files, list) {
		struct inode *inode = file_inode(of->file);

		if (of->mmapped) {
			unmap_mapping_range(inode->i_mapping, 0, 0, 1);
			of->mmapped = false;
			on->nr_mmapped--;
		}

		if (kn->flags & KERNFS_HAS_RELEASE)
			kernfs_release_file(kn, of);
	}

	WARN_ON_ONCE(on->nr_mmapped || on->nr_to_release);
	mutex_unlock(mutex);
}

 
__poll_t kernfs_generic_poll(struct kernfs_open_file *of, poll_table *wait)
{
	struct kernfs_open_node *on = of_on(of);

	poll_wait(of->file, &on->poll, wait);

	if (of->event != atomic_read(&on->event))
		return DEFAULT_POLLMASK|EPOLLERR|EPOLLPRI;

	return DEFAULT_POLLMASK;
}

static __poll_t kernfs_fop_poll(struct file *filp, poll_table *wait)
{
	struct kernfs_open_file *of = kernfs_of(filp);
	struct kernfs_node *kn = kernfs_dentry_node(filp->f_path.dentry);
	__poll_t ret;

	if (!kernfs_get_active(kn))
		return DEFAULT_POLLMASK|EPOLLERR|EPOLLPRI;

	if (kn->attr.ops->poll)
		ret = kn->attr.ops->poll(of, wait);
	else
		ret = kernfs_generic_poll(of, wait);

	kernfs_put_active(kn);
	return ret;
}

static void kernfs_notify_workfn(struct work_struct *work)
{
	struct kernfs_node *kn;
	struct kernfs_super_info *info;
	struct kernfs_root *root;
repeat:
	 
	spin_lock_irq(&kernfs_notify_lock);
	kn = kernfs_notify_list;
	if (kn == KERNFS_NOTIFY_EOL) {
		spin_unlock_irq(&kernfs_notify_lock);
		return;
	}
	kernfs_notify_list = kn->attr.notify_next;
	kn->attr.notify_next = NULL;
	spin_unlock_irq(&kernfs_notify_lock);

	root = kernfs_root(kn);
	 

	down_read(&root->kernfs_supers_rwsem);
	list_for_each_entry(info, &kernfs_root(kn)->supers, node) {
		struct kernfs_node *parent;
		struct inode *p_inode = NULL;
		struct inode *inode;
		struct qstr name;

		 
		inode = ilookup(info->sb, kernfs_ino(kn));
		if (!inode)
			continue;

		name = (struct qstr)QSTR_INIT(kn->name, strlen(kn->name));
		parent = kernfs_get_parent(kn);
		if (parent) {
			p_inode = ilookup(info->sb, kernfs_ino(parent));
			if (p_inode) {
				fsnotify(FS_MODIFY | FS_EVENT_ON_CHILD,
					 inode, FSNOTIFY_EVENT_INODE,
					 p_inode, &name, inode, 0);
				iput(p_inode);
			}

			kernfs_put(parent);
		}

		if (!p_inode)
			fsnotify_inode(inode, FS_MODIFY);

		iput(inode);
	}

	up_read(&root->kernfs_supers_rwsem);
	kernfs_put(kn);
	goto repeat;
}

 
void kernfs_notify(struct kernfs_node *kn)
{
	static DECLARE_WORK(kernfs_notify_work, kernfs_notify_workfn);
	unsigned long flags;
	struct kernfs_open_node *on;

	if (WARN_ON(kernfs_type(kn) != KERNFS_FILE))
		return;

	 
	rcu_read_lock();
	on = rcu_dereference(kn->attr.open);
	if (on) {
		atomic_inc(&on->event);
		wake_up_interruptible(&on->poll);
	}
	rcu_read_unlock();

	 
	spin_lock_irqsave(&kernfs_notify_lock, flags);
	if (!kn->attr.notify_next) {
		kernfs_get(kn);
		kn->attr.notify_next = kernfs_notify_list;
		kernfs_notify_list = kn;
		schedule_work(&kernfs_notify_work);
	}
	spin_unlock_irqrestore(&kernfs_notify_lock, flags);
}
EXPORT_SYMBOL_GPL(kernfs_notify);

const struct file_operations kernfs_file_fops = {
	.read_iter	= kernfs_fop_read_iter,
	.write_iter	= kernfs_fop_write_iter,
	.llseek		= generic_file_llseek,
	.mmap		= kernfs_fop_mmap,
	.open		= kernfs_fop_open,
	.release	= kernfs_fop_release,
	.poll		= kernfs_fop_poll,
	.fsync		= noop_fsync,
	.splice_read	= copy_splice_read,
	.splice_write	= iter_file_splice_write,
};

 
struct kernfs_node *__kernfs_create_file(struct kernfs_node *parent,
					 const char *name,
					 umode_t mode, kuid_t uid, kgid_t gid,
					 loff_t size,
					 const struct kernfs_ops *ops,
					 void *priv, const void *ns,
					 struct lock_class_key *key)
{
	struct kernfs_node *kn;
	unsigned flags;
	int rc;

	flags = KERNFS_FILE;

	kn = kernfs_new_node(parent, name, (mode & S_IALLUGO) | S_IFREG,
			     uid, gid, flags);
	if (!kn)
		return ERR_PTR(-ENOMEM);

	kn->attr.ops = ops;
	kn->attr.size = size;
	kn->ns = ns;
	kn->priv = priv;

#ifdef CONFIG_DEBUG_LOCK_ALLOC
	if (key) {
		lockdep_init_map(&kn->dep_map, "kn->active", key, 0);
		kn->flags |= KERNFS_LOCKDEP;
	}
#endif

	 
	if (ops->seq_show)
		kn->flags |= KERNFS_HAS_SEQ_SHOW;
	if (ops->mmap)
		kn->flags |= KERNFS_HAS_MMAP;
	if (ops->release)
		kn->flags |= KERNFS_HAS_RELEASE;

	rc = kernfs_add_one(kn);
	if (rc) {
		kernfs_put(kn);
		return ERR_PTR(rc);
	}
	return kn;
}
