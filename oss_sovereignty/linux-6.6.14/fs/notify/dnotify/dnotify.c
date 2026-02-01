
 
#include <linux/fs.h>
#include <linux/module.h>
#include <linux/sched.h>
#include <linux/sched/signal.h>
#include <linux/dnotify.h>
#include <linux/init.h>
#include <linux/security.h>
#include <linux/spinlock.h>
#include <linux/slab.h>
#include <linux/fdtable.h>
#include <linux/fsnotify_backend.h>

static int dir_notify_enable __read_mostly = 1;
#ifdef CONFIG_SYSCTL
static struct ctl_table dnotify_sysctls[] = {
	{
		.procname	= "dir-notify-enable",
		.data		= &dir_notify_enable,
		.maxlen		= sizeof(int),
		.mode		= 0644,
		.proc_handler	= proc_dointvec,
	},
	{}
};
static void __init dnotify_sysctl_init(void)
{
	register_sysctl_init("fs", dnotify_sysctls);
}
#else
#define dnotify_sysctl_init() do { } while (0)
#endif

static struct kmem_cache *dnotify_struct_cache __read_mostly;
static struct kmem_cache *dnotify_mark_cache __read_mostly;
static struct fsnotify_group *dnotify_group __read_mostly;

 
struct dnotify_mark {
	struct fsnotify_mark fsn_mark;
	struct dnotify_struct *dn;
};

 
static void dnotify_recalc_inode_mask(struct fsnotify_mark *fsn_mark)
{
	__u32 new_mask = 0;
	struct dnotify_struct *dn;
	struct dnotify_mark *dn_mark  = container_of(fsn_mark,
						     struct dnotify_mark,
						     fsn_mark);

	assert_spin_locked(&fsn_mark->lock);

	for (dn = dn_mark->dn; dn != NULL; dn = dn->dn_next)
		new_mask |= (dn->dn_mask & ~FS_DN_MULTISHOT);
	if (fsn_mark->mask == new_mask)
		return;
	fsn_mark->mask = new_mask;

	fsnotify_recalc_mask(fsn_mark->connector);
}

 
static int dnotify_handle_event(struct fsnotify_mark *inode_mark, u32 mask,
				struct inode *inode, struct inode *dir,
				const struct qstr *name, u32 cookie)
{
	struct dnotify_mark *dn_mark;
	struct dnotify_struct *dn;
	struct dnotify_struct **prev;
	struct fown_struct *fown;
	__u32 test_mask = mask & ~FS_EVENT_ON_CHILD;

	 
	if (!dir && !(mask & FS_ISDIR))
		return 0;

	dn_mark = container_of(inode_mark, struct dnotify_mark, fsn_mark);

	spin_lock(&inode_mark->lock);
	prev = &dn_mark->dn;
	while ((dn = *prev) != NULL) {
		if ((dn->dn_mask & test_mask) == 0) {
			prev = &dn->dn_next;
			continue;
		}
		fown = &dn->dn_filp->f_owner;
		send_sigio(fown, dn->dn_fd, POLL_MSG);
		if (dn->dn_mask & FS_DN_MULTISHOT)
			prev = &dn->dn_next;
		else {
			*prev = dn->dn_next;
			kmem_cache_free(dnotify_struct_cache, dn);
			dnotify_recalc_inode_mask(inode_mark);
		}
	}

	spin_unlock(&inode_mark->lock);

	return 0;
}

static void dnotify_free_mark(struct fsnotify_mark *fsn_mark)
{
	struct dnotify_mark *dn_mark = container_of(fsn_mark,
						    struct dnotify_mark,
						    fsn_mark);

	BUG_ON(dn_mark->dn);

	kmem_cache_free(dnotify_mark_cache, dn_mark);
}

static const struct fsnotify_ops dnotify_fsnotify_ops = {
	.handle_inode_event = dnotify_handle_event,
	.free_mark = dnotify_free_mark,
};

 
void dnotify_flush(struct file *filp, fl_owner_t id)
{
	struct fsnotify_mark *fsn_mark;
	struct dnotify_mark *dn_mark;
	struct dnotify_struct *dn;
	struct dnotify_struct **prev;
	struct inode *inode;
	bool free = false;

	inode = file_inode(filp);
	if (!S_ISDIR(inode->i_mode))
		return;

	fsn_mark = fsnotify_find_mark(&inode->i_fsnotify_marks, dnotify_group);
	if (!fsn_mark)
		return;
	dn_mark = container_of(fsn_mark, struct dnotify_mark, fsn_mark);

	fsnotify_group_lock(dnotify_group);

	spin_lock(&fsn_mark->lock);
	prev = &dn_mark->dn;
	while ((dn = *prev) != NULL) {
		if ((dn->dn_owner == id) && (dn->dn_filp == filp)) {
			*prev = dn->dn_next;
			kmem_cache_free(dnotify_struct_cache, dn);
			dnotify_recalc_inode_mask(fsn_mark);
			break;
		}
		prev = &dn->dn_next;
	}

	spin_unlock(&fsn_mark->lock);

	 
	if (dn_mark->dn == NULL) {
		fsnotify_detach_mark(fsn_mark);
		free = true;
	}

	fsnotify_group_unlock(dnotify_group);

	if (free)
		fsnotify_free_mark(fsn_mark);
	fsnotify_put_mark(fsn_mark);
}

 
static __u32 convert_arg(unsigned int arg)
{
	__u32 new_mask = FS_EVENT_ON_CHILD;

	if (arg & DN_MULTISHOT)
		new_mask |= FS_DN_MULTISHOT;
	if (arg & DN_DELETE)
		new_mask |= (FS_DELETE | FS_MOVED_FROM);
	if (arg & DN_MODIFY)
		new_mask |= FS_MODIFY;
	if (arg & DN_ACCESS)
		new_mask |= FS_ACCESS;
	if (arg & DN_ATTRIB)
		new_mask |= FS_ATTRIB;
	if (arg & DN_RENAME)
		new_mask |= FS_RENAME;
	if (arg & DN_CREATE)
		new_mask |= (FS_CREATE | FS_MOVED_TO);

	return new_mask;
}

 
static int attach_dn(struct dnotify_struct *dn, struct dnotify_mark *dn_mark,
		     fl_owner_t id, int fd, struct file *filp, __u32 mask)
{
	struct dnotify_struct *odn;

	odn = dn_mark->dn;
	while (odn != NULL) {
		 
		if ((odn->dn_owner == id) && (odn->dn_filp == filp)) {
			odn->dn_fd = fd;
			odn->dn_mask |= mask;
			return -EEXIST;
		}
		odn = odn->dn_next;
	}

	dn->dn_mask = mask;
	dn->dn_fd = fd;
	dn->dn_filp = filp;
	dn->dn_owner = id;
	dn->dn_next = dn_mark->dn;
	dn_mark->dn = dn;

	return 0;
}

 
int fcntl_dirnotify(int fd, struct file *filp, unsigned int arg)
{
	struct dnotify_mark *new_dn_mark, *dn_mark;
	struct fsnotify_mark *new_fsn_mark, *fsn_mark;
	struct dnotify_struct *dn;
	struct inode *inode;
	fl_owner_t id = current->files;
	struct file *f;
	int destroy = 0, error = 0;
	__u32 mask;

	 
	new_fsn_mark = NULL;
	dn = NULL;

	if (!dir_notify_enable) {
		error = -EINVAL;
		goto out_err;
	}

	 
	if ((arg & ~DN_MULTISHOT) == 0) {
		dnotify_flush(filp, id);
		error = 0;
		goto out_err;
	}

	 
	inode = file_inode(filp);
	if (!S_ISDIR(inode->i_mode)) {
		error = -ENOTDIR;
		goto out_err;
	}

	 
	mask = convert_arg(arg);

	error = security_path_notify(&filp->f_path, mask,
			FSNOTIFY_OBJ_TYPE_INODE);
	if (error)
		goto out_err;

	 
	dn = kmem_cache_alloc(dnotify_struct_cache, GFP_KERNEL);
	if (!dn) {
		error = -ENOMEM;
		goto out_err;
	}

	 
	new_dn_mark = kmem_cache_alloc(dnotify_mark_cache, GFP_KERNEL);
	if (!new_dn_mark) {
		error = -ENOMEM;
		goto out_err;
	}

	 
	new_fsn_mark = &new_dn_mark->fsn_mark;
	fsnotify_init_mark(new_fsn_mark, dnotify_group);
	new_fsn_mark->mask = mask;
	new_dn_mark->dn = NULL;

	 
	fsnotify_group_lock(dnotify_group);

	 
	fsn_mark = fsnotify_find_mark(&inode->i_fsnotify_marks, dnotify_group);
	if (fsn_mark) {
		dn_mark = container_of(fsn_mark, struct dnotify_mark, fsn_mark);
		spin_lock(&fsn_mark->lock);
	} else {
		error = fsnotify_add_inode_mark_locked(new_fsn_mark, inode, 0);
		if (error) {
			fsnotify_group_unlock(dnotify_group);
			goto out_err;
		}
		spin_lock(&new_fsn_mark->lock);
		fsn_mark = new_fsn_mark;
		dn_mark = new_dn_mark;
		 
		new_fsn_mark = NULL;
	}

	rcu_read_lock();
	f = lookup_fd_rcu(fd);
	rcu_read_unlock();

	 
	if (f != filp) {
		 
		if (dn_mark == new_dn_mark)
			destroy = 1;
		error = 0;
		goto out;
	}

	__f_setown(filp, task_pid(current), PIDTYPE_TGID, 0);

	error = attach_dn(dn, dn_mark, id, fd, filp, mask);
	 
	if (!error)
		dn = NULL;
	 
	else if (error == -EEXIST)
		error = 0;

	dnotify_recalc_inode_mask(fsn_mark);
out:
	spin_unlock(&fsn_mark->lock);

	if (destroy)
		fsnotify_detach_mark(fsn_mark);
	fsnotify_group_unlock(dnotify_group);
	if (destroy)
		fsnotify_free_mark(fsn_mark);
	fsnotify_put_mark(fsn_mark);
out_err:
	if (new_fsn_mark)
		fsnotify_put_mark(new_fsn_mark);
	if (dn)
		kmem_cache_free(dnotify_struct_cache, dn);
	return error;
}

static int __init dnotify_init(void)
{
	dnotify_struct_cache = KMEM_CACHE(dnotify_struct,
					  SLAB_PANIC|SLAB_ACCOUNT);
	dnotify_mark_cache = KMEM_CACHE(dnotify_mark, SLAB_PANIC|SLAB_ACCOUNT);

	dnotify_group = fsnotify_alloc_group(&dnotify_fsnotify_ops,
					     FSNOTIFY_GROUP_NOFS);
	if (IS_ERR(dnotify_group))
		panic("unable to allocate fsnotify group for dnotify\n");
	dnotify_sysctl_init();
	return 0;
}

module_init(dnotify_init)
