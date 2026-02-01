
 

#include <linux/capability.h>
#include <linux/compat.h>

#include "autofs_i.h"

static int autofs_dir_permission(struct mnt_idmap *, struct inode *, int);
static int autofs_dir_symlink(struct mnt_idmap *, struct inode *,
			      struct dentry *, const char *);
static int autofs_dir_unlink(struct inode *, struct dentry *);
static int autofs_dir_rmdir(struct inode *, struct dentry *);
static int autofs_dir_mkdir(struct mnt_idmap *, struct inode *,
			    struct dentry *, umode_t);
static long autofs_root_ioctl(struct file *, unsigned int, unsigned long);
#ifdef CONFIG_COMPAT
static long autofs_root_compat_ioctl(struct file *,
				     unsigned int, unsigned long);
#endif
static int autofs_dir_open(struct inode *inode, struct file *file);
static struct dentry *autofs_lookup(struct inode *,
				    struct dentry *, unsigned int);
static struct vfsmount *autofs_d_automount(struct path *);
static int autofs_d_manage(const struct path *, bool);
static void autofs_dentry_release(struct dentry *);

const struct file_operations autofs_root_operations = {
	.open		= dcache_dir_open,
	.release	= dcache_dir_close,
	.read		= generic_read_dir,
	.iterate_shared	= dcache_readdir,
	.llseek		= dcache_dir_lseek,
	.unlocked_ioctl	= autofs_root_ioctl,
#ifdef CONFIG_COMPAT
	.compat_ioctl	= autofs_root_compat_ioctl,
#endif
};

const struct file_operations autofs_dir_operations = {
	.open		= autofs_dir_open,
	.release	= dcache_dir_close,
	.read		= generic_read_dir,
	.iterate_shared	= dcache_readdir,
	.llseek		= dcache_dir_lseek,
};

const struct inode_operations autofs_dir_inode_operations = {
	.lookup		= autofs_lookup,
	.permission	= autofs_dir_permission,
	.unlink		= autofs_dir_unlink,
	.symlink	= autofs_dir_symlink,
	.mkdir		= autofs_dir_mkdir,
	.rmdir		= autofs_dir_rmdir,
};

const struct dentry_operations autofs_dentry_operations = {
	.d_automount	= autofs_d_automount,
	.d_manage	= autofs_d_manage,
	.d_release	= autofs_dentry_release,
};

static void autofs_del_active(struct dentry *dentry)
{
	struct autofs_sb_info *sbi = autofs_sbi(dentry->d_sb);
	struct autofs_info *ino;

	ino = autofs_dentry_ino(dentry);
	spin_lock(&sbi->lookup_lock);
	list_del_init(&ino->active);
	spin_unlock(&sbi->lookup_lock);
}

static int autofs_dir_open(struct inode *inode, struct file *file)
{
	struct dentry *dentry = file->f_path.dentry;
	struct autofs_sb_info *sbi = autofs_sbi(dentry->d_sb);
	struct autofs_info *ino = autofs_dentry_ino(dentry);

	pr_debug("file=%p dentry=%p %pd\n", file, dentry, dentry);

	if (autofs_oz_mode(sbi))
		goto out;

	 
	spin_lock(&sbi->lookup_lock);
	if (!path_is_mountpoint(&file->f_path) && autofs_empty(ino)) {
		spin_unlock(&sbi->lookup_lock);
		return -ENOENT;
	}
	spin_unlock(&sbi->lookup_lock);

out:
	return dcache_dir_open(inode, file);
}

static void autofs_dentry_release(struct dentry *de)
{
	struct autofs_info *ino = autofs_dentry_ino(de);
	struct autofs_sb_info *sbi = autofs_sbi(de->d_sb);

	pr_debug("releasing %p\n", de);

	if (!ino)
		return;

	if (sbi) {
		spin_lock(&sbi->lookup_lock);
		if (!list_empty(&ino->active))
			list_del(&ino->active);
		if (!list_empty(&ino->expiring))
			list_del(&ino->expiring);
		spin_unlock(&sbi->lookup_lock);
	}

	autofs_free_ino(ino);
}

static struct dentry *autofs_lookup_active(struct dentry *dentry)
{
	struct autofs_sb_info *sbi = autofs_sbi(dentry->d_sb);
	struct dentry *parent = dentry->d_parent;
	const struct qstr *name = &dentry->d_name;
	unsigned int len = name->len;
	unsigned int hash = name->hash;
	const unsigned char *str = name->name;
	struct list_head *p, *head;

	head = &sbi->active_list;
	if (list_empty(head))
		return NULL;
	spin_lock(&sbi->lookup_lock);
	list_for_each(p, head) {
		struct autofs_info *ino;
		struct dentry *active;
		const struct qstr *qstr;

		ino = list_entry(p, struct autofs_info, active);
		active = ino->dentry;

		spin_lock(&active->d_lock);

		 
		if ((int) d_count(active) <= 0)
			goto next;

		qstr = &active->d_name;

		if (active->d_name.hash != hash)
			goto next;
		if (active->d_parent != parent)
			goto next;

		if (qstr->len != len)
			goto next;
		if (memcmp(qstr->name, str, len))
			goto next;

		if (d_unhashed(active)) {
			dget_dlock(active);
			spin_unlock(&active->d_lock);
			spin_unlock(&sbi->lookup_lock);
			return active;
		}
next:
		spin_unlock(&active->d_lock);
	}
	spin_unlock(&sbi->lookup_lock);

	return NULL;
}

static struct dentry *autofs_lookup_expiring(struct dentry *dentry,
					     bool rcu_walk)
{
	struct autofs_sb_info *sbi = autofs_sbi(dentry->d_sb);
	struct dentry *parent = dentry->d_parent;
	const struct qstr *name = &dentry->d_name;
	unsigned int len = name->len;
	unsigned int hash = name->hash;
	const unsigned char *str = name->name;
	struct list_head *p, *head;

	head = &sbi->expiring_list;
	if (list_empty(head))
		return NULL;
	spin_lock(&sbi->lookup_lock);
	list_for_each(p, head) {
		struct autofs_info *ino;
		struct dentry *expiring;
		const struct qstr *qstr;

		if (rcu_walk) {
			spin_unlock(&sbi->lookup_lock);
			return ERR_PTR(-ECHILD);
		}

		ino = list_entry(p, struct autofs_info, expiring);
		expiring = ino->dentry;

		spin_lock(&expiring->d_lock);

		 
		if (d_really_is_negative(expiring))
			goto next;

		qstr = &expiring->d_name;

		if (expiring->d_name.hash != hash)
			goto next;
		if (expiring->d_parent != parent)
			goto next;

		if (qstr->len != len)
			goto next;
		if (memcmp(qstr->name, str, len))
			goto next;

		if (d_unhashed(expiring)) {
			dget_dlock(expiring);
			spin_unlock(&expiring->d_lock);
			spin_unlock(&sbi->lookup_lock);
			return expiring;
		}
next:
		spin_unlock(&expiring->d_lock);
	}
	spin_unlock(&sbi->lookup_lock);

	return NULL;
}

static int autofs_mount_wait(const struct path *path, bool rcu_walk)
{
	struct autofs_sb_info *sbi = autofs_sbi(path->dentry->d_sb);
	struct autofs_info *ino = autofs_dentry_ino(path->dentry);
	int status = 0;

	if (ino->flags & AUTOFS_INF_PENDING) {
		if (rcu_walk)
			return -ECHILD;
		pr_debug("waiting for mount name=%pd\n", path->dentry);
		status = autofs_wait(sbi, path, NFY_MOUNT);
		pr_debug("mount wait done status=%d\n", status);
		ino->last_used = jiffies;
		return status;
	}
	if (!(sbi->flags & AUTOFS_SBI_STRICTEXPIRE))
		ino->last_used = jiffies;
	return status;
}

static int do_expire_wait(const struct path *path, bool rcu_walk)
{
	struct dentry *dentry = path->dentry;
	struct dentry *expiring;

	expiring = autofs_lookup_expiring(dentry, rcu_walk);
	if (IS_ERR(expiring))
		return PTR_ERR(expiring);
	if (!expiring)
		return autofs_expire_wait(path, rcu_walk);
	else {
		const struct path this = { .mnt = path->mnt, .dentry = expiring };
		 
		autofs_expire_wait(&this, 0);
		autofs_del_expiring(expiring);
		dput(expiring);
	}
	return 0;
}

static struct dentry *autofs_mountpoint_changed(struct path *path)
{
	struct dentry *dentry = path->dentry;
	struct autofs_sb_info *sbi = autofs_sbi(dentry->d_sb);

	 
	if (autofs_type_indirect(sbi->type) && d_unhashed(dentry)) {
		struct dentry *parent = dentry->d_parent;
		struct autofs_info *ino;
		struct dentry *new;

		new = d_lookup(parent, &dentry->d_name);
		if (!new)
			return NULL;
		ino = autofs_dentry_ino(new);
		ino->last_used = jiffies;
		dput(path->dentry);
		path->dentry = new;
	}
	return path->dentry;
}

static struct vfsmount *autofs_d_automount(struct path *path)
{
	struct dentry *dentry = path->dentry;
	struct autofs_sb_info *sbi = autofs_sbi(dentry->d_sb);
	struct autofs_info *ino = autofs_dentry_ino(dentry);
	int status;

	pr_debug("dentry=%p %pd\n", dentry, dentry);

	 
	if (autofs_oz_mode(sbi))
		return NULL;

	 
	status = do_expire_wait(path, 0);
	if (status && status != -EAGAIN)
		return NULL;

	 
	spin_lock(&sbi->fs_lock);
	if (ino->flags & AUTOFS_INF_PENDING) {
		spin_unlock(&sbi->fs_lock);
		status = autofs_mount_wait(path, 0);
		if (status)
			return ERR_PTR(status);
		goto done;
	}

	 
	if (d_really_is_positive(dentry) && d_is_symlink(dentry)) {
		spin_unlock(&sbi->fs_lock);
		goto done;
	}

	if (!path_is_mountpoint(path)) {
		 
		if (sbi->version > 4) {
			if (path_has_submounts(path)) {
				spin_unlock(&sbi->fs_lock);
				goto done;
			}
		} else {
			if (!autofs_empty(ino)) {
				spin_unlock(&sbi->fs_lock);
				goto done;
			}
		}
		ino->flags |= AUTOFS_INF_PENDING;
		spin_unlock(&sbi->fs_lock);
		status = autofs_mount_wait(path, 0);
		spin_lock(&sbi->fs_lock);
		ino->flags &= ~AUTOFS_INF_PENDING;
		if (status) {
			spin_unlock(&sbi->fs_lock);
			return ERR_PTR(status);
		}
	}
	spin_unlock(&sbi->fs_lock);
done:
	 
	dentry = autofs_mountpoint_changed(path);
	if (!dentry)
		return ERR_PTR(-ENOENT);

	return NULL;
}

static int autofs_d_manage(const struct path *path, bool rcu_walk)
{
	struct dentry *dentry = path->dentry;
	struct autofs_sb_info *sbi = autofs_sbi(dentry->d_sb);
	struct autofs_info *ino = autofs_dentry_ino(dentry);
	int status;

	pr_debug("dentry=%p %pd\n", dentry, dentry);

	 
	if (autofs_oz_mode(sbi)) {
		if (!path_is_mountpoint(path))
			return -EISDIR;
		return 0;
	}

	 
	if (do_expire_wait(path, rcu_walk) == -ECHILD)
		return -ECHILD;

	 
	status = autofs_mount_wait(path, rcu_walk);
	if (status)
		return status;

	if (rcu_walk) {
		 
		struct inode *inode;

		if (ino->flags & AUTOFS_INF_WANT_EXPIRE)
			return 0;
		if (path_is_mountpoint(path))
			return 0;
		inode = d_inode_rcu(dentry);
		if (inode && S_ISLNK(inode->i_mode))
			return -EISDIR;
		if (!autofs_empty(ino))
			return -EISDIR;
		return 0;
	}

	spin_lock(&sbi->fs_lock);
	 
	if (!(ino->flags & AUTOFS_INF_EXPIRING)) {
		 
		if ((!path_is_mountpoint(path) && !autofs_empty(ino)) ||
		    (d_really_is_positive(dentry) && d_is_symlink(dentry)))
			status = -EISDIR;
	}
	spin_unlock(&sbi->fs_lock);

	return status;
}

 
static struct dentry *autofs_lookup(struct inode *dir,
				    struct dentry *dentry, unsigned int flags)
{
	struct autofs_sb_info *sbi;
	struct autofs_info *ino;
	struct dentry *active;

	pr_debug("name = %pd\n", dentry);

	 
	if (dentry->d_name.len > NAME_MAX)
		return ERR_PTR(-ENAMETOOLONG);

	sbi = autofs_sbi(dir->i_sb);

	pr_debug("pid = %u, pgrp = %u, catatonic = %d, oz_mode = %d\n",
		 current->pid, task_pgrp_nr(current),
		 sbi->flags & AUTOFS_SBI_CATATONIC,
		 autofs_oz_mode(sbi));

	active = autofs_lookup_active(dentry);
	if (active)
		return active;
	else {
		 
		if (!autofs_oz_mode(sbi) && !IS_ROOT(dentry->d_parent))
			return ERR_PTR(-ENOENT);

		ino = autofs_new_ino(sbi);
		if (!ino)
			return ERR_PTR(-ENOMEM);

		spin_lock(&sbi->lookup_lock);
		spin_lock(&dentry->d_lock);
		 
		if (IS_ROOT(dentry->d_parent) &&
		    autofs_type_indirect(sbi->type))
			__managed_dentry_set_managed(dentry);
		dentry->d_fsdata = ino;
		ino->dentry = dentry;

		list_add(&ino->active, &sbi->active_list);
		spin_unlock(&sbi->lookup_lock);
		spin_unlock(&dentry->d_lock);
	}
	return NULL;
}

static int autofs_dir_permission(struct mnt_idmap *idmap,
				 struct inode *inode, int mask)
{
	if (mask & MAY_WRITE) {
		struct autofs_sb_info *sbi = autofs_sbi(inode->i_sb);

		if (!autofs_oz_mode(sbi))
			return -EACCES;

		 
		if (sbi->flags & AUTOFS_SBI_CATATONIC)
			return -EACCES;
	}

	return generic_permission(idmap, inode, mask);
}

static int autofs_dir_symlink(struct mnt_idmap *idmap,
			      struct inode *dir, struct dentry *dentry,
			      const char *symname)
{
	struct autofs_info *ino = autofs_dentry_ino(dentry);
	struct autofs_info *p_ino;
	struct inode *inode;
	size_t size = strlen(symname);
	char *cp;

	pr_debug("%s <- %pd\n", symname, dentry);

	BUG_ON(!ino);

	autofs_clean_ino(ino);

	autofs_del_active(dentry);

	cp = kmalloc(size + 1, GFP_KERNEL);
	if (!cp)
		return -ENOMEM;

	strcpy(cp, symname);

	inode = autofs_get_inode(dir->i_sb, S_IFLNK | 0555);
	if (!inode) {
		kfree(cp);
		return -ENOMEM;
	}
	inode->i_private = cp;
	inode->i_size = size;
	d_add(dentry, inode);

	dget(dentry);
	p_ino = autofs_dentry_ino(dentry->d_parent);
	p_ino->count++;

	dir->i_mtime = inode_set_ctime_current(dir);

	return 0;
}

 
static int autofs_dir_unlink(struct inode *dir, struct dentry *dentry)
{
	struct autofs_sb_info *sbi = autofs_sbi(dir->i_sb);
	struct autofs_info *ino = autofs_dentry_ino(dentry);
	struct autofs_info *p_ino;

	p_ino = autofs_dentry_ino(dentry->d_parent);
	p_ino->count--;
	dput(ino->dentry);

	d_inode(dentry)->i_size = 0;
	clear_nlink(d_inode(dentry));

	dir->i_mtime = inode_set_ctime_current(dir);

	spin_lock(&sbi->lookup_lock);
	__autofs_add_expiring(dentry);
	d_drop(dentry);
	spin_unlock(&sbi->lookup_lock);

	return 0;
}

 
static void autofs_set_leaf_automount_flags(struct dentry *dentry)
{
	struct dentry *parent;

	 
	if (IS_ROOT(dentry->d_parent))
		return;

	managed_dentry_set_managed(dentry);

	parent = dentry->d_parent;
	 
	if (IS_ROOT(parent->d_parent))
		return;
	managed_dentry_clear_managed(parent);
}

static void autofs_clear_leaf_automount_flags(struct dentry *dentry)
{
	struct dentry *parent;

	 
	if (IS_ROOT(dentry->d_parent))
		return;

	managed_dentry_clear_managed(dentry);

	parent = dentry->d_parent;
	 
	if (IS_ROOT(parent->d_parent))
		return;
	if (autofs_dentry_ino(parent)->count == 2)
		managed_dentry_set_managed(parent);
}

static int autofs_dir_rmdir(struct inode *dir, struct dentry *dentry)
{
	struct autofs_sb_info *sbi = autofs_sbi(dir->i_sb);
	struct autofs_info *ino = autofs_dentry_ino(dentry);
	struct autofs_info *p_ino;

	pr_debug("dentry %p, removing %pd\n", dentry, dentry);

	if (ino->count != 1)
		return -ENOTEMPTY;

	spin_lock(&sbi->lookup_lock);
	__autofs_add_expiring(dentry);
	d_drop(dentry);
	spin_unlock(&sbi->lookup_lock);

	if (sbi->version < 5)
		autofs_clear_leaf_automount_flags(dentry);

	p_ino = autofs_dentry_ino(dentry->d_parent);
	p_ino->count--;
	dput(ino->dentry);
	d_inode(dentry)->i_size = 0;
	clear_nlink(d_inode(dentry));

	if (dir->i_nlink)
		drop_nlink(dir);

	return 0;
}

static int autofs_dir_mkdir(struct mnt_idmap *idmap,
			    struct inode *dir, struct dentry *dentry,
			    umode_t mode)
{
	struct autofs_sb_info *sbi = autofs_sbi(dir->i_sb);
	struct autofs_info *ino = autofs_dentry_ino(dentry);
	struct autofs_info *p_ino;
	struct inode *inode;

	pr_debug("dentry %p, creating %pd\n", dentry, dentry);

	BUG_ON(!ino);

	autofs_clean_ino(ino);

	autofs_del_active(dentry);

	inode = autofs_get_inode(dir->i_sb, S_IFDIR | mode);
	if (!inode)
		return -ENOMEM;
	d_add(dentry, inode);

	if (sbi->version < 5)
		autofs_set_leaf_automount_flags(dentry);

	dget(dentry);
	p_ino = autofs_dentry_ino(dentry->d_parent);
	p_ino->count++;
	inc_nlink(dir);
	dir->i_mtime = inode_set_ctime_current(dir);

	return 0;
}

 
#ifdef CONFIG_COMPAT
static inline int autofs_compat_get_set_timeout(struct autofs_sb_info *sbi,
						 compat_ulong_t __user *p)
{
	unsigned long ntimeout;
	int rv;

	rv = get_user(ntimeout, p);
	if (rv)
		goto error;

	rv = put_user(sbi->exp_timeout/HZ, p);
	if (rv)
		goto error;

	if (ntimeout > UINT_MAX/HZ)
		sbi->exp_timeout = 0;
	else
		sbi->exp_timeout = ntimeout * HZ;

	return 0;
error:
	return rv;
}
#endif

static inline int autofs_get_set_timeout(struct autofs_sb_info *sbi,
					  unsigned long __user *p)
{
	unsigned long ntimeout;
	int rv;

	rv = get_user(ntimeout, p);
	if (rv)
		goto error;

	rv = put_user(sbi->exp_timeout/HZ, p);
	if (rv)
		goto error;

	if (ntimeout > ULONG_MAX/HZ)
		sbi->exp_timeout = 0;
	else
		sbi->exp_timeout = ntimeout * HZ;

	return 0;
error:
	return rv;
}

 
static inline int autofs_get_protover(struct autofs_sb_info *sbi,
				       int __user *p)
{
	return put_user(sbi->version, p);
}

 
static inline int autofs_get_protosubver(struct autofs_sb_info *sbi,
					  int __user *p)
{
	return put_user(sbi->sub_version, p);
}

 
static inline int autofs_ask_umount(struct vfsmount *mnt, int __user *p)
{
	int status = 0;

	if (may_umount(mnt))
		status = 1;

	pr_debug("may umount %d\n", status);

	status = put_user(status, p);

	return status;
}

 
int is_autofs_dentry(struct dentry *dentry)
{
	return dentry && d_really_is_positive(dentry) &&
		dentry->d_op == &autofs_dentry_operations &&
		dentry->d_fsdata != NULL;
}

 
static int autofs_root_ioctl_unlocked(struct inode *inode, struct file *filp,
				       unsigned int cmd, unsigned long arg)
{
	struct autofs_sb_info *sbi = autofs_sbi(inode->i_sb);
	void __user *p = (void __user *)arg;

	pr_debug("cmd = 0x%08x, arg = 0x%08lx, sbi = %p, pgrp = %u\n",
		 cmd, arg, sbi, task_pgrp_nr(current));

	if (_IOC_TYPE(cmd) != _IOC_TYPE(AUTOFS_IOC_FIRST) ||
	     _IOC_NR(cmd) - _IOC_NR(AUTOFS_IOC_FIRST) >= AUTOFS_IOC_COUNT)
		return -ENOTTY;

	if (!autofs_oz_mode(sbi) && !capable(CAP_SYS_ADMIN))
		return -EPERM;

	switch (cmd) {
	case AUTOFS_IOC_READY:	 
		return autofs_wait_release(sbi, (autofs_wqt_t) arg, 0);
	case AUTOFS_IOC_FAIL:	 
		return autofs_wait_release(sbi, (autofs_wqt_t) arg, -ENOENT);
	case AUTOFS_IOC_CATATONIC:  
		autofs_catatonic_mode(sbi);
		return 0;
	case AUTOFS_IOC_PROTOVER:  
		return autofs_get_protover(sbi, p);
	case AUTOFS_IOC_PROTOSUBVER:  
		return autofs_get_protosubver(sbi, p);
	case AUTOFS_IOC_SETTIMEOUT:
		return autofs_get_set_timeout(sbi, p);
#ifdef CONFIG_COMPAT
	case AUTOFS_IOC_SETTIMEOUT32:
		return autofs_compat_get_set_timeout(sbi, p);
#endif

	case AUTOFS_IOC_ASKUMOUNT:
		return autofs_ask_umount(filp->f_path.mnt, p);

	 
	case AUTOFS_IOC_EXPIRE:
		return autofs_expire_run(inode->i_sb, filp->f_path.mnt, sbi, p);
	 
	case AUTOFS_IOC_EXPIRE_MULTI:
		return autofs_expire_multi(inode->i_sb,
					   filp->f_path.mnt, sbi, p);

	default:
		return -EINVAL;
	}
}

static long autofs_root_ioctl(struct file *filp,
			       unsigned int cmd, unsigned long arg)
{
	struct inode *inode = file_inode(filp);

	return autofs_root_ioctl_unlocked(inode, filp, cmd, arg);
}

#ifdef CONFIG_COMPAT
static long autofs_root_compat_ioctl(struct file *filp,
				      unsigned int cmd, unsigned long arg)
{
	struct inode *inode = file_inode(filp);
	int ret;

	if (cmd == AUTOFS_IOC_READY || cmd == AUTOFS_IOC_FAIL)
		ret = autofs_root_ioctl_unlocked(inode, filp, cmd, arg);
	else
		ret = autofs_root_ioctl_unlocked(inode, filp, cmd,
					      (unsigned long) compat_ptr(arg));

	return ret;
}
#endif
