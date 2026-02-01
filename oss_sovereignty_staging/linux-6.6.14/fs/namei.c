
 

 

 
 

#include <linux/init.h>
#include <linux/export.h>
#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/fs.h>
#include <linux/filelock.h>
#include <linux/namei.h>
#include <linux/pagemap.h>
#include <linux/sched/mm.h>
#include <linux/fsnotify.h>
#include <linux/personality.h>
#include <linux/security.h>
#include <linux/ima.h>
#include <linux/syscalls.h>
#include <linux/mount.h>
#include <linux/audit.h>
#include <linux/capability.h>
#include <linux/file.h>
#include <linux/fcntl.h>
#include <linux/device_cgroup.h>
#include <linux/fs_struct.h>
#include <linux/posix_acl.h>
#include <linux/hash.h>
#include <linux/bitops.h>
#include <linux/init_task.h>
#include <linux/uaccess.h>

#include "internal.h"
#include "mount.h"

 

 

 

 
 

 

#define EMBEDDED_NAME_MAX	(PATH_MAX - offsetof(struct filename, iname))

struct filename *
getname_flags(const char __user *filename, int flags, int *empty)
{
	struct filename *result;
	char *kname;
	int len;

	result = audit_reusename(filename);
	if (result)
		return result;

	result = __getname();
	if (unlikely(!result))
		return ERR_PTR(-ENOMEM);

	 
	kname = (char *)result->iname;
	result->name = kname;

	len = strncpy_from_user(kname, filename, EMBEDDED_NAME_MAX);
	if (unlikely(len < 0)) {
		__putname(result);
		return ERR_PTR(len);
	}

	 
	if (unlikely(len == EMBEDDED_NAME_MAX)) {
		const size_t size = offsetof(struct filename, iname[1]);
		kname = (char *)result;

		 
		result = kzalloc(size, GFP_KERNEL);
		if (unlikely(!result)) {
			__putname(kname);
			return ERR_PTR(-ENOMEM);
		}
		result->name = kname;
		len = strncpy_from_user(kname, filename, PATH_MAX);
		if (unlikely(len < 0)) {
			__putname(kname);
			kfree(result);
			return ERR_PTR(len);
		}
		if (unlikely(len == PATH_MAX)) {
			__putname(kname);
			kfree(result);
			return ERR_PTR(-ENAMETOOLONG);
		}
	}

	atomic_set(&result->refcnt, 1);
	 
	if (unlikely(!len)) {
		if (empty)
			*empty = 1;
		if (!(flags & LOOKUP_EMPTY)) {
			putname(result);
			return ERR_PTR(-ENOENT);
		}
	}

	result->uptr = filename;
	result->aname = NULL;
	audit_getname(result);
	return result;
}

struct filename *
getname_uflags(const char __user *filename, int uflags)
{
	int flags = (uflags & AT_EMPTY_PATH) ? LOOKUP_EMPTY : 0;

	return getname_flags(filename, flags, NULL);
}

struct filename *
getname(const char __user * filename)
{
	return getname_flags(filename, 0, NULL);
}

struct filename *
getname_kernel(const char * filename)
{
	struct filename *result;
	int len = strlen(filename) + 1;

	result = __getname();
	if (unlikely(!result))
		return ERR_PTR(-ENOMEM);

	if (len <= EMBEDDED_NAME_MAX) {
		result->name = (char *)result->iname;
	} else if (len <= PATH_MAX) {
		const size_t size = offsetof(struct filename, iname[1]);
		struct filename *tmp;

		tmp = kmalloc(size, GFP_KERNEL);
		if (unlikely(!tmp)) {
			__putname(result);
			return ERR_PTR(-ENOMEM);
		}
		tmp->name = (char *)result;
		result = tmp;
	} else {
		__putname(result);
		return ERR_PTR(-ENAMETOOLONG);
	}
	memcpy((char *)result->name, filename, len);
	result->uptr = NULL;
	result->aname = NULL;
	atomic_set(&result->refcnt, 1);
	audit_getname(result);

	return result;
}
EXPORT_SYMBOL(getname_kernel);

void putname(struct filename *name)
{
	if (IS_ERR(name))
		return;

	if (WARN_ON_ONCE(!atomic_read(&name->refcnt)))
		return;

	if (!atomic_dec_and_test(&name->refcnt))
		return;

	if (name->name != name->iname) {
		__putname(name->name);
		kfree(name);
	} else
		__putname(name);
}
EXPORT_SYMBOL(putname);

 
static int check_acl(struct mnt_idmap *idmap,
		     struct inode *inode, int mask)
{
#ifdef CONFIG_FS_POSIX_ACL
	struct posix_acl *acl;

	if (mask & MAY_NOT_BLOCK) {
		acl = get_cached_acl_rcu(inode, ACL_TYPE_ACCESS);
	        if (!acl)
	                return -EAGAIN;
		 
		if (is_uncached_acl(acl))
			return -ECHILD;
	        return posix_acl_permission(idmap, inode, acl, mask);
	}

	acl = get_inode_acl(inode, ACL_TYPE_ACCESS);
	if (IS_ERR(acl))
		return PTR_ERR(acl);
	if (acl) {
	        int error = posix_acl_permission(idmap, inode, acl, mask);
	        posix_acl_release(acl);
	        return error;
	}
#endif

	return -EAGAIN;
}

 
static int acl_permission_check(struct mnt_idmap *idmap,
				struct inode *inode, int mask)
{
	unsigned int mode = inode->i_mode;
	vfsuid_t vfsuid;

	 
	vfsuid = i_uid_into_vfsuid(idmap, inode);
	if (likely(vfsuid_eq_kuid(vfsuid, current_fsuid()))) {
		mask &= 7;
		mode >>= 6;
		return (mask & ~mode) ? -EACCES : 0;
	}

	 
	if (IS_POSIXACL(inode) && (mode & S_IRWXG)) {
		int error = check_acl(idmap, inode, mask);
		if (error != -EAGAIN)
			return error;
	}

	 
	mask &= 7;

	 
	if (mask & (mode ^ (mode >> 3))) {
		vfsgid_t vfsgid = i_gid_into_vfsgid(idmap, inode);
		if (vfsgid_in_group_p(vfsgid))
			mode >>= 3;
	}

	 
	return (mask & ~mode) ? -EACCES : 0;
}

 
int generic_permission(struct mnt_idmap *idmap, struct inode *inode,
		       int mask)
{
	int ret;

	 
	ret = acl_permission_check(idmap, inode, mask);
	if (ret != -EACCES)
		return ret;

	if (S_ISDIR(inode->i_mode)) {
		 
		if (!(mask & MAY_WRITE))
			if (capable_wrt_inode_uidgid(idmap, inode,
						     CAP_DAC_READ_SEARCH))
				return 0;
		if (capable_wrt_inode_uidgid(idmap, inode,
					     CAP_DAC_OVERRIDE))
			return 0;
		return -EACCES;
	}

	 
	mask &= MAY_READ | MAY_WRITE | MAY_EXEC;
	if (mask == MAY_READ)
		if (capable_wrt_inode_uidgid(idmap, inode,
					     CAP_DAC_READ_SEARCH))
			return 0;
	 
	if (!(mask & MAY_EXEC) || (inode->i_mode & S_IXUGO))
		if (capable_wrt_inode_uidgid(idmap, inode,
					     CAP_DAC_OVERRIDE))
			return 0;

	return -EACCES;
}
EXPORT_SYMBOL(generic_permission);

 
static inline int do_inode_permission(struct mnt_idmap *idmap,
				      struct inode *inode, int mask)
{
	if (unlikely(!(inode->i_opflags & IOP_FASTPERM))) {
		if (likely(inode->i_op->permission))
			return inode->i_op->permission(idmap, inode, mask);

		 
		spin_lock(&inode->i_lock);
		inode->i_opflags |= IOP_FASTPERM;
		spin_unlock(&inode->i_lock);
	}
	return generic_permission(idmap, inode, mask);
}

 
static int sb_permission(struct super_block *sb, struct inode *inode, int mask)
{
	if (unlikely(mask & MAY_WRITE)) {
		umode_t mode = inode->i_mode;

		 
		if (sb_rdonly(sb) && (S_ISREG(mode) || S_ISDIR(mode) || S_ISLNK(mode)))
			return -EROFS;
	}
	return 0;
}

 
int inode_permission(struct mnt_idmap *idmap,
		     struct inode *inode, int mask)
{
	int retval;

	retval = sb_permission(inode->i_sb, inode, mask);
	if (retval)
		return retval;

	if (unlikely(mask & MAY_WRITE)) {
		 
		if (IS_IMMUTABLE(inode))
			return -EPERM;

		 
		if (HAS_UNMAPPED_ID(idmap, inode))
			return -EACCES;
	}

	retval = do_inode_permission(idmap, inode, mask);
	if (retval)
		return retval;

	retval = devcgroup_inode_permission(inode, mask);
	if (retval)
		return retval;

	return security_inode_permission(inode, mask);
}
EXPORT_SYMBOL(inode_permission);

 
void path_get(const struct path *path)
{
	mntget(path->mnt);
	dget(path->dentry);
}
EXPORT_SYMBOL(path_get);

 
void path_put(const struct path *path)
{
	dput(path->dentry);
	mntput(path->mnt);
}
EXPORT_SYMBOL(path_put);

#define EMBEDDED_LEVELS 2
struct nameidata {
	struct path	path;
	struct qstr	last;
	struct path	root;
	struct inode	*inode;  
	unsigned int	flags, state;
	unsigned	seq, next_seq, m_seq, r_seq;
	int		last_type;
	unsigned	depth;
	int		total_link_count;
	struct saved {
		struct path link;
		struct delayed_call done;
		const char *name;
		unsigned seq;
	} *stack, internal[EMBEDDED_LEVELS];
	struct filename	*name;
	struct nameidata *saved;
	unsigned	root_seq;
	int		dfd;
	vfsuid_t	dir_vfsuid;
	umode_t		dir_mode;
} __randomize_layout;

#define ND_ROOT_PRESET 1
#define ND_ROOT_GRABBED 2
#define ND_JUMPED 4

static void __set_nameidata(struct nameidata *p, int dfd, struct filename *name)
{
	struct nameidata *old = current->nameidata;
	p->stack = p->internal;
	p->depth = 0;
	p->dfd = dfd;
	p->name = name;
	p->path.mnt = NULL;
	p->path.dentry = NULL;
	p->total_link_count = old ? old->total_link_count : 0;
	p->saved = old;
	current->nameidata = p;
}

static inline void set_nameidata(struct nameidata *p, int dfd, struct filename *name,
			  const struct path *root)
{
	__set_nameidata(p, dfd, name);
	p->state = 0;
	if (unlikely(root)) {
		p->state = ND_ROOT_PRESET;
		p->root = *root;
	}
}

static void restore_nameidata(void)
{
	struct nameidata *now = current->nameidata, *old = now->saved;

	current->nameidata = old;
	if (old)
		old->total_link_count = now->total_link_count;
	if (now->stack != now->internal)
		kfree(now->stack);
}

static bool nd_alloc_stack(struct nameidata *nd)
{
	struct saved *p;

	p= kmalloc_array(MAXSYMLINKS, sizeof(struct saved),
			 nd->flags & LOOKUP_RCU ? GFP_ATOMIC : GFP_KERNEL);
	if (unlikely(!p))
		return false;
	memcpy(p, nd->internal, sizeof(nd->internal));
	nd->stack = p;
	return true;
}

 
static bool path_connected(struct vfsmount *mnt, struct dentry *dentry)
{
	struct super_block *sb = mnt->mnt_sb;

	 
	if (mnt->mnt_root == sb->s_root)
		return true;

	return is_subdir(dentry, mnt->mnt_root);
}

static void drop_links(struct nameidata *nd)
{
	int i = nd->depth;
	while (i--) {
		struct saved *last = nd->stack + i;
		do_delayed_call(&last->done);
		clear_delayed_call(&last->done);
	}
}

static void leave_rcu(struct nameidata *nd)
{
	nd->flags &= ~LOOKUP_RCU;
	nd->seq = nd->next_seq = 0;
	rcu_read_unlock();
}

static void terminate_walk(struct nameidata *nd)
{
	drop_links(nd);
	if (!(nd->flags & LOOKUP_RCU)) {
		int i;
		path_put(&nd->path);
		for (i = 0; i < nd->depth; i++)
			path_put(&nd->stack[i].link);
		if (nd->state & ND_ROOT_GRABBED) {
			path_put(&nd->root);
			nd->state &= ~ND_ROOT_GRABBED;
		}
	} else {
		leave_rcu(nd);
	}
	nd->depth = 0;
	nd->path.mnt = NULL;
	nd->path.dentry = NULL;
}

 
static bool __legitimize_path(struct path *path, unsigned seq, unsigned mseq)
{
	int res = __legitimize_mnt(path->mnt, mseq);
	if (unlikely(res)) {
		if (res > 0)
			path->mnt = NULL;
		path->dentry = NULL;
		return false;
	}
	if (unlikely(!lockref_get_not_dead(&path->dentry->d_lockref))) {
		path->dentry = NULL;
		return false;
	}
	return !read_seqcount_retry(&path->dentry->d_seq, seq);
}

static inline bool legitimize_path(struct nameidata *nd,
			    struct path *path, unsigned seq)
{
	return __legitimize_path(path, seq, nd->m_seq);
}

static bool legitimize_links(struct nameidata *nd)
{
	int i;
	if (unlikely(nd->flags & LOOKUP_CACHED)) {
		drop_links(nd);
		nd->depth = 0;
		return false;
	}
	for (i = 0; i < nd->depth; i++) {
		struct saved *last = nd->stack + i;
		if (unlikely(!legitimize_path(nd, &last->link, last->seq))) {
			drop_links(nd);
			nd->depth = i + 1;
			return false;
		}
	}
	return true;
}

static bool legitimize_root(struct nameidata *nd)
{
	 
	if (!nd->root.mnt || (nd->state & ND_ROOT_PRESET))
		return true;
	nd->state |= ND_ROOT_GRABBED;
	return legitimize_path(nd, &nd->root, nd->root_seq);
}

 

 
static bool try_to_unlazy(struct nameidata *nd)
{
	struct dentry *parent = nd->path.dentry;

	BUG_ON(!(nd->flags & LOOKUP_RCU));

	if (unlikely(!legitimize_links(nd)))
		goto out1;
	if (unlikely(!legitimize_path(nd, &nd->path, nd->seq)))
		goto out;
	if (unlikely(!legitimize_root(nd)))
		goto out;
	leave_rcu(nd);
	BUG_ON(nd->inode != parent->d_inode);
	return true;

out1:
	nd->path.mnt = NULL;
	nd->path.dentry = NULL;
out:
	leave_rcu(nd);
	return false;
}

 
static bool try_to_unlazy_next(struct nameidata *nd, struct dentry *dentry)
{
	int res;
	BUG_ON(!(nd->flags & LOOKUP_RCU));

	if (unlikely(!legitimize_links(nd)))
		goto out2;
	res = __legitimize_mnt(nd->path.mnt, nd->m_seq);
	if (unlikely(res)) {
		if (res > 0)
			goto out2;
		goto out1;
	}
	if (unlikely(!lockref_get_not_dead(&nd->path.dentry->d_lockref)))
		goto out1;

	 
	if (unlikely(!lockref_get_not_dead(&dentry->d_lockref)))
		goto out;
	if (read_seqcount_retry(&dentry->d_seq, nd->next_seq))
		goto out_dput;
	 
	if (unlikely(!legitimize_root(nd)))
		goto out_dput;
	leave_rcu(nd);
	return true;

out2:
	nd->path.mnt = NULL;
out1:
	nd->path.dentry = NULL;
out:
	leave_rcu(nd);
	return false;
out_dput:
	leave_rcu(nd);
	dput(dentry);
	return false;
}

static inline int d_revalidate(struct dentry *dentry, unsigned int flags)
{
	if (unlikely(dentry->d_flags & DCACHE_OP_REVALIDATE))
		return dentry->d_op->d_revalidate(dentry, flags);
	else
		return 1;
}

 
static int complete_walk(struct nameidata *nd)
{
	struct dentry *dentry = nd->path.dentry;
	int status;

	if (nd->flags & LOOKUP_RCU) {
		 
		if (!(nd->state & ND_ROOT_PRESET))
			if (!(nd->flags & LOOKUP_IS_SCOPED))
				nd->root.mnt = NULL;
		nd->flags &= ~LOOKUP_CACHED;
		if (!try_to_unlazy(nd))
			return -ECHILD;
	}

	if (unlikely(nd->flags & LOOKUP_IS_SCOPED)) {
		 
		if (!path_is_under(&nd->path, &nd->root))
			return -EXDEV;
	}

	if (likely(!(nd->state & ND_JUMPED)))
		return 0;

	if (likely(!(dentry->d_flags & DCACHE_OP_WEAK_REVALIDATE)))
		return 0;

	status = dentry->d_op->d_weak_revalidate(dentry, nd->flags);
	if (status > 0)
		return 0;

	if (!status)
		status = -ESTALE;

	return status;
}

static int set_root(struct nameidata *nd)
{
	struct fs_struct *fs = current->fs;

	 
	if (WARN_ON(nd->flags & LOOKUP_IS_SCOPED))
		return -ENOTRECOVERABLE;

	if (nd->flags & LOOKUP_RCU) {
		unsigned seq;

		do {
			seq = read_seqcount_begin(&fs->seq);
			nd->root = fs->root;
			nd->root_seq = __read_seqcount_begin(&nd->root.dentry->d_seq);
		} while (read_seqcount_retry(&fs->seq, seq));
	} else {
		get_fs_root(fs, &nd->root);
		nd->state |= ND_ROOT_GRABBED;
	}
	return 0;
}

static int nd_jump_root(struct nameidata *nd)
{
	if (unlikely(nd->flags & LOOKUP_BENEATH))
		return -EXDEV;
	if (unlikely(nd->flags & LOOKUP_NO_XDEV)) {
		 
		if (nd->path.mnt != NULL && nd->path.mnt != nd->root.mnt)
			return -EXDEV;
	}
	if (!nd->root.mnt) {
		int error = set_root(nd);
		if (error)
			return error;
	}
	if (nd->flags & LOOKUP_RCU) {
		struct dentry *d;
		nd->path = nd->root;
		d = nd->path.dentry;
		nd->inode = d->d_inode;
		nd->seq = nd->root_seq;
		if (read_seqcount_retry(&d->d_seq, nd->seq))
			return -ECHILD;
	} else {
		path_put(&nd->path);
		nd->path = nd->root;
		path_get(&nd->path);
		nd->inode = nd->path.dentry->d_inode;
	}
	nd->state |= ND_JUMPED;
	return 0;
}

 
int nd_jump_link(const struct path *path)
{
	int error = -ELOOP;
	struct nameidata *nd = current->nameidata;

	if (unlikely(nd->flags & LOOKUP_NO_MAGICLINKS))
		goto err;

	error = -EXDEV;
	if (unlikely(nd->flags & LOOKUP_NO_XDEV)) {
		if (nd->path.mnt != path->mnt)
			goto err;
	}
	 
	if (unlikely(nd->flags & LOOKUP_IS_SCOPED))
		goto err;

	path_put(&nd->path);
	nd->path = *path;
	nd->inode = nd->path.dentry->d_inode;
	nd->state |= ND_JUMPED;
	return 0;

err:
	path_put(path);
	return error;
}

static inline void put_link(struct nameidata *nd)
{
	struct saved *last = nd->stack + --nd->depth;
	do_delayed_call(&last->done);
	if (!(nd->flags & LOOKUP_RCU))
		path_put(&last->link);
}

static int sysctl_protected_symlinks __read_mostly;
static int sysctl_protected_hardlinks __read_mostly;
static int sysctl_protected_fifos __read_mostly;
static int sysctl_protected_regular __read_mostly;

#ifdef CONFIG_SYSCTL
static struct ctl_table namei_sysctls[] = {
	{
		.procname	= "protected_symlinks",
		.data		= &sysctl_protected_symlinks,
		.maxlen		= sizeof(int),
		.mode		= 0644,
		.proc_handler	= proc_dointvec_minmax,
		.extra1		= SYSCTL_ZERO,
		.extra2		= SYSCTL_ONE,
	},
	{
		.procname	= "protected_hardlinks",
		.data		= &sysctl_protected_hardlinks,
		.maxlen		= sizeof(int),
		.mode		= 0644,
		.proc_handler	= proc_dointvec_minmax,
		.extra1		= SYSCTL_ZERO,
		.extra2		= SYSCTL_ONE,
	},
	{
		.procname	= "protected_fifos",
		.data		= &sysctl_protected_fifos,
		.maxlen		= sizeof(int),
		.mode		= 0644,
		.proc_handler	= proc_dointvec_minmax,
		.extra1		= SYSCTL_ZERO,
		.extra2		= SYSCTL_TWO,
	},
	{
		.procname	= "protected_regular",
		.data		= &sysctl_protected_regular,
		.maxlen		= sizeof(int),
		.mode		= 0644,
		.proc_handler	= proc_dointvec_minmax,
		.extra1		= SYSCTL_ZERO,
		.extra2		= SYSCTL_TWO,
	},
	{ }
};

static int __init init_fs_namei_sysctls(void)
{
	register_sysctl_init("fs", namei_sysctls);
	return 0;
}
fs_initcall(init_fs_namei_sysctls);

#endif  

 
static inline int may_follow_link(struct nameidata *nd, const struct inode *inode)
{
	struct mnt_idmap *idmap;
	vfsuid_t vfsuid;

	if (!sysctl_protected_symlinks)
		return 0;

	idmap = mnt_idmap(nd->path.mnt);
	vfsuid = i_uid_into_vfsuid(idmap, inode);
	 
	if (vfsuid_eq_kuid(vfsuid, current_fsuid()))
		return 0;

	 
	if ((nd->dir_mode & (S_ISVTX|S_IWOTH)) != (S_ISVTX|S_IWOTH))
		return 0;

	 
	if (vfsuid_valid(nd->dir_vfsuid) && vfsuid_eq(nd->dir_vfsuid, vfsuid))
		return 0;

	if (nd->flags & LOOKUP_RCU)
		return -ECHILD;

	audit_inode(nd->name, nd->stack[0].link.dentry, 0);
	audit_log_path_denied(AUDIT_ANOM_LINK, "follow_link");
	return -EACCES;
}

 
static bool safe_hardlink_source(struct mnt_idmap *idmap,
				 struct inode *inode)
{
	umode_t mode = inode->i_mode;

	 
	if (!S_ISREG(mode))
		return false;

	 
	if (mode & S_ISUID)
		return false;

	 
	if ((mode & (S_ISGID | S_IXGRP)) == (S_ISGID | S_IXGRP))
		return false;

	 
	if (inode_permission(idmap, inode, MAY_READ | MAY_WRITE))
		return false;

	return true;
}

 
int may_linkat(struct mnt_idmap *idmap, const struct path *link)
{
	struct inode *inode = link->dentry->d_inode;

	 
	if (!vfsuid_valid(i_uid_into_vfsuid(idmap, inode)) ||
	    !vfsgid_valid(i_gid_into_vfsgid(idmap, inode)))
		return -EOVERFLOW;

	if (!sysctl_protected_hardlinks)
		return 0;

	 
	if (safe_hardlink_source(idmap, inode) ||
	    inode_owner_or_capable(idmap, inode))
		return 0;

	audit_log_path_denied(AUDIT_ANOM_LINK, "linkat");
	return -EPERM;
}

 
static int may_create_in_sticky(struct mnt_idmap *idmap,
				struct nameidata *nd, struct inode *const inode)
{
	umode_t dir_mode = nd->dir_mode;
	vfsuid_t dir_vfsuid = nd->dir_vfsuid;

	if ((!sysctl_protected_fifos && S_ISFIFO(inode->i_mode)) ||
	    (!sysctl_protected_regular && S_ISREG(inode->i_mode)) ||
	    likely(!(dir_mode & S_ISVTX)) ||
	    vfsuid_eq(i_uid_into_vfsuid(idmap, inode), dir_vfsuid) ||
	    vfsuid_eq_kuid(i_uid_into_vfsuid(idmap, inode), current_fsuid()))
		return 0;

	if (likely(dir_mode & 0002) ||
	    (dir_mode & 0020 &&
	     ((sysctl_protected_fifos >= 2 && S_ISFIFO(inode->i_mode)) ||
	      (sysctl_protected_regular >= 2 && S_ISREG(inode->i_mode))))) {
		const char *operation = S_ISFIFO(inode->i_mode) ?
					"sticky_create_fifo" :
					"sticky_create_regular";
		audit_log_path_denied(AUDIT_ANOM_CREAT, operation);
		return -EACCES;
	}
	return 0;
}

 
int follow_up(struct path *path)
{
	struct mount *mnt = real_mount(path->mnt);
	struct mount *parent;
	struct dentry *mountpoint;

	read_seqlock_excl(&mount_lock);
	parent = mnt->mnt_parent;
	if (parent == mnt) {
		read_sequnlock_excl(&mount_lock);
		return 0;
	}
	mntget(&parent->mnt);
	mountpoint = dget(mnt->mnt_mountpoint);
	read_sequnlock_excl(&mount_lock);
	dput(path->dentry);
	path->dentry = mountpoint;
	mntput(path->mnt);
	path->mnt = &parent->mnt;
	return 1;
}
EXPORT_SYMBOL(follow_up);

static bool choose_mountpoint_rcu(struct mount *m, const struct path *root,
				  struct path *path, unsigned *seqp)
{
	while (mnt_has_parent(m)) {
		struct dentry *mountpoint = m->mnt_mountpoint;

		m = m->mnt_parent;
		if (unlikely(root->dentry == mountpoint &&
			     root->mnt == &m->mnt))
			break;
		if (mountpoint != m->mnt.mnt_root) {
			path->mnt = &m->mnt;
			path->dentry = mountpoint;
			*seqp = read_seqcount_begin(&mountpoint->d_seq);
			return true;
		}
	}
	return false;
}

static bool choose_mountpoint(struct mount *m, const struct path *root,
			      struct path *path)
{
	bool found;

	rcu_read_lock();
	while (1) {
		unsigned seq, mseq = read_seqbegin(&mount_lock);

		found = choose_mountpoint_rcu(m, root, path, &seq);
		if (unlikely(!found)) {
			if (!read_seqretry(&mount_lock, mseq))
				break;
		} else {
			if (likely(__legitimize_path(path, seq, mseq)))
				break;
			rcu_read_unlock();
			path_put(path);
			rcu_read_lock();
		}
	}
	rcu_read_unlock();
	return found;
}

 
static int follow_automount(struct path *path, int *count, unsigned lookup_flags)
{
	struct dentry *dentry = path->dentry;

	 
	if (!(lookup_flags & (LOOKUP_PARENT | LOOKUP_DIRECTORY |
			   LOOKUP_OPEN | LOOKUP_CREATE | LOOKUP_AUTOMOUNT)) &&
	    dentry->d_inode)
		return -EISDIR;

	if (count && (*count)++ >= MAXSYMLINKS)
		return -ELOOP;

	return finish_automount(dentry->d_op->d_automount(path), path);
}

 
static int __traverse_mounts(struct path *path, unsigned flags, bool *jumped,
			     int *count, unsigned lookup_flags)
{
	struct vfsmount *mnt = path->mnt;
	bool need_mntput = false;
	int ret = 0;

	while (flags & DCACHE_MANAGED_DENTRY) {
		 
		if (flags & DCACHE_MANAGE_TRANSIT) {
			ret = path->dentry->d_op->d_manage(path, false);
			flags = smp_load_acquire(&path->dentry->d_flags);
			if (ret < 0)
				break;
		}

		if (flags & DCACHE_MOUNTED) {	 
			struct vfsmount *mounted = lookup_mnt(path);
			if (mounted) {		 
				dput(path->dentry);
				if (need_mntput)
					mntput(path->mnt);
				path->mnt = mounted;
				path->dentry = dget(mounted->mnt_root);
				 
				flags = path->dentry->d_flags;
				need_mntput = true;
				continue;
			}
		}

		if (!(flags & DCACHE_NEED_AUTOMOUNT))
			break;

		 
		ret = follow_automount(path, count, lookup_flags);
		flags = smp_load_acquire(&path->dentry->d_flags);
		if (ret < 0)
			break;
	}

	if (ret == -EISDIR)
		ret = 0;
	 
	if (need_mntput && path->mnt == mnt)
		mntput(path->mnt);
	if (!ret && unlikely(d_flags_negative(flags)))
		ret = -ENOENT;
	*jumped = need_mntput;
	return ret;
}

static inline int traverse_mounts(struct path *path, bool *jumped,
				  int *count, unsigned lookup_flags)
{
	unsigned flags = smp_load_acquire(&path->dentry->d_flags);

	 
	if (likely(!(flags & DCACHE_MANAGED_DENTRY))) {
		*jumped = false;
		if (unlikely(d_flags_negative(flags)))
			return -ENOENT;
		return 0;
	}
	return __traverse_mounts(path, flags, jumped, count, lookup_flags);
}

int follow_down_one(struct path *path)
{
	struct vfsmount *mounted;

	mounted = lookup_mnt(path);
	if (mounted) {
		dput(path->dentry);
		mntput(path->mnt);
		path->mnt = mounted;
		path->dentry = dget(mounted->mnt_root);
		return 1;
	}
	return 0;
}
EXPORT_SYMBOL(follow_down_one);

 
int follow_down(struct path *path, unsigned int flags)
{
	struct vfsmount *mnt = path->mnt;
	bool jumped;
	int ret = traverse_mounts(path, &jumped, NULL, flags);

	if (path->mnt != mnt)
		mntput(mnt);
	return ret;
}
EXPORT_SYMBOL(follow_down);

 
static bool __follow_mount_rcu(struct nameidata *nd, struct path *path)
{
	struct dentry *dentry = path->dentry;
	unsigned int flags = dentry->d_flags;

	if (likely(!(flags & DCACHE_MANAGED_DENTRY)))
		return true;

	if (unlikely(nd->flags & LOOKUP_NO_XDEV))
		return false;

	for (;;) {
		 
		if (unlikely(flags & DCACHE_MANAGE_TRANSIT)) {
			int res = dentry->d_op->d_manage(path, true);
			if (res)
				return res == -EISDIR;
			flags = dentry->d_flags;
		}

		if (flags & DCACHE_MOUNTED) {
			struct mount *mounted = __lookup_mnt(path->mnt, dentry);
			if (mounted) {
				path->mnt = &mounted->mnt;
				dentry = path->dentry = mounted->mnt.mnt_root;
				nd->state |= ND_JUMPED;
				nd->next_seq = read_seqcount_begin(&dentry->d_seq);
				flags = dentry->d_flags;
				 
				 
				if (read_seqretry(&mount_lock, nd->m_seq))
					return false;
				continue;
			}
			if (read_seqretry(&mount_lock, nd->m_seq))
				return false;
		}
		return !(flags & DCACHE_NEED_AUTOMOUNT);
	}
}

static inline int handle_mounts(struct nameidata *nd, struct dentry *dentry,
			  struct path *path)
{
	bool jumped;
	int ret;

	path->mnt = nd->path.mnt;
	path->dentry = dentry;
	if (nd->flags & LOOKUP_RCU) {
		unsigned int seq = nd->next_seq;
		if (likely(__follow_mount_rcu(nd, path)))
			return 0;
		 
		path->mnt = nd->path.mnt;
		path->dentry = dentry;
		nd->next_seq = seq;
		if (!try_to_unlazy_next(nd, dentry))
			return -ECHILD;
	}
	ret = traverse_mounts(path, &jumped, &nd->total_link_count, nd->flags);
	if (jumped) {
		if (unlikely(nd->flags & LOOKUP_NO_XDEV))
			ret = -EXDEV;
		else
			nd->state |= ND_JUMPED;
	}
	if (unlikely(ret)) {
		dput(path->dentry);
		if (path->mnt != nd->path.mnt)
			mntput(path->mnt);
	}
	return ret;
}

 
static struct dentry *lookup_dcache(const struct qstr *name,
				    struct dentry *dir,
				    unsigned int flags)
{
	struct dentry *dentry = d_lookup(dir, name);
	if (dentry) {
		int error = d_revalidate(dentry, flags);
		if (unlikely(error <= 0)) {
			if (!error)
				d_invalidate(dentry);
			dput(dentry);
			return ERR_PTR(error);
		}
	}
	return dentry;
}

 
struct dentry *lookup_one_qstr_excl(const struct qstr *name,
				    struct dentry *base,
				    unsigned int flags)
{
	struct dentry *dentry = lookup_dcache(name, base, flags);
	struct dentry *old;
	struct inode *dir = base->d_inode;

	if (dentry)
		return dentry;

	 
	if (unlikely(IS_DEADDIR(dir)))
		return ERR_PTR(-ENOENT);

	dentry = d_alloc(base, name);
	if (unlikely(!dentry))
		return ERR_PTR(-ENOMEM);

	old = dir->i_op->lookup(dir, dentry, flags);
	if (unlikely(old)) {
		dput(dentry);
		dentry = old;
	}
	return dentry;
}
EXPORT_SYMBOL(lookup_one_qstr_excl);

static struct dentry *lookup_fast(struct nameidata *nd)
{
	struct dentry *dentry, *parent = nd->path.dentry;
	int status = 1;

	 
	if (nd->flags & LOOKUP_RCU) {
		dentry = __d_lookup_rcu(parent, &nd->last, &nd->next_seq);
		if (unlikely(!dentry)) {
			if (!try_to_unlazy(nd))
				return ERR_PTR(-ECHILD);
			return NULL;
		}

		 
		if (read_seqcount_retry(&parent->d_seq, nd->seq))
			return ERR_PTR(-ECHILD);

		status = d_revalidate(dentry, nd->flags);
		if (likely(status > 0))
			return dentry;
		if (!try_to_unlazy_next(nd, dentry))
			return ERR_PTR(-ECHILD);
		if (status == -ECHILD)
			 
			status = d_revalidate(dentry, nd->flags);
	} else {
		dentry = __d_lookup(parent, &nd->last);
		if (unlikely(!dentry))
			return NULL;
		status = d_revalidate(dentry, nd->flags);
	}
	if (unlikely(status <= 0)) {
		if (!status)
			d_invalidate(dentry);
		dput(dentry);
		return ERR_PTR(status);
	}
	return dentry;
}

 
static struct dentry *__lookup_slow(const struct qstr *name,
				    struct dentry *dir,
				    unsigned int flags)
{
	struct dentry *dentry, *old;
	struct inode *inode = dir->d_inode;
	DECLARE_WAIT_QUEUE_HEAD_ONSTACK(wq);

	 
	if (unlikely(IS_DEADDIR(inode)))
		return ERR_PTR(-ENOENT);
again:
	dentry = d_alloc_parallel(dir, name, &wq);
	if (IS_ERR(dentry))
		return dentry;
	if (unlikely(!d_in_lookup(dentry))) {
		int error = d_revalidate(dentry, flags);
		if (unlikely(error <= 0)) {
			if (!error) {
				d_invalidate(dentry);
				dput(dentry);
				goto again;
			}
			dput(dentry);
			dentry = ERR_PTR(error);
		}
	} else {
		old = inode->i_op->lookup(inode, dentry, flags);
		d_lookup_done(dentry);
		if (unlikely(old)) {
			dput(dentry);
			dentry = old;
		}
	}
	return dentry;
}

static struct dentry *lookup_slow(const struct qstr *name,
				  struct dentry *dir,
				  unsigned int flags)
{
	struct inode *inode = dir->d_inode;
	struct dentry *res;
	inode_lock_shared(inode);
	res = __lookup_slow(name, dir, flags);
	inode_unlock_shared(inode);
	return res;
}

static inline int may_lookup(struct mnt_idmap *idmap,
			     struct nameidata *nd)
{
	if (nd->flags & LOOKUP_RCU) {
		int err = inode_permission(idmap, nd->inode, MAY_EXEC|MAY_NOT_BLOCK);
		if (err != -ECHILD || !try_to_unlazy(nd))
			return err;
	}
	return inode_permission(idmap, nd->inode, MAY_EXEC);
}

static int reserve_stack(struct nameidata *nd, struct path *link)
{
	if (unlikely(nd->total_link_count++ >= MAXSYMLINKS))
		return -ELOOP;

	if (likely(nd->depth != EMBEDDED_LEVELS))
		return 0;
	if (likely(nd->stack != nd->internal))
		return 0;
	if (likely(nd_alloc_stack(nd)))
		return 0;

	if (nd->flags & LOOKUP_RCU) {
		 
		 
		bool grabbed_link = legitimize_path(nd, link, nd->next_seq);

		if (!try_to_unlazy(nd) || !grabbed_link)
			return -ECHILD;

		if (nd_alloc_stack(nd))
			return 0;
	}
	return -ENOMEM;
}

enum {WALK_TRAILING = 1, WALK_MORE = 2, WALK_NOFOLLOW = 4};

static const char *pick_link(struct nameidata *nd, struct path *link,
		     struct inode *inode, int flags)
{
	struct saved *last;
	const char *res;
	int error = reserve_stack(nd, link);

	if (unlikely(error)) {
		if (!(nd->flags & LOOKUP_RCU))
			path_put(link);
		return ERR_PTR(error);
	}
	last = nd->stack + nd->depth++;
	last->link = *link;
	clear_delayed_call(&last->done);
	last->seq = nd->next_seq;

	if (flags & WALK_TRAILING) {
		error = may_follow_link(nd, inode);
		if (unlikely(error))
			return ERR_PTR(error);
	}

	if (unlikely(nd->flags & LOOKUP_NO_SYMLINKS) ||
			unlikely(link->mnt->mnt_flags & MNT_NOSYMFOLLOW))
		return ERR_PTR(-ELOOP);

	if (!(nd->flags & LOOKUP_RCU)) {
		touch_atime(&last->link);
		cond_resched();
	} else if (atime_needs_update(&last->link, inode)) {
		if (!try_to_unlazy(nd))
			return ERR_PTR(-ECHILD);
		touch_atime(&last->link);
	}

	error = security_inode_follow_link(link->dentry, inode,
					   nd->flags & LOOKUP_RCU);
	if (unlikely(error))
		return ERR_PTR(error);

	res = READ_ONCE(inode->i_link);
	if (!res) {
		const char * (*get)(struct dentry *, struct inode *,
				struct delayed_call *);
		get = inode->i_op->get_link;
		if (nd->flags & LOOKUP_RCU) {
			res = get(NULL, inode, &last->done);
			if (res == ERR_PTR(-ECHILD) && try_to_unlazy(nd))
				res = get(link->dentry, inode, &last->done);
		} else {
			res = get(link->dentry, inode, &last->done);
		}
		if (!res)
			goto all_done;
		if (IS_ERR(res))
			return res;
	}
	if (*res == '/') {
		error = nd_jump_root(nd);
		if (unlikely(error))
			return ERR_PTR(error);
		while (unlikely(*++res == '/'))
			;
	}
	if (*res)
		return res;
all_done:  
	put_link(nd);
	return NULL;
}

 
static const char *step_into(struct nameidata *nd, int flags,
		     struct dentry *dentry)
{
	struct path path;
	struct inode *inode;
	int err = handle_mounts(nd, dentry, &path);

	if (err < 0)
		return ERR_PTR(err);
	inode = path.dentry->d_inode;
	if (likely(!d_is_symlink(path.dentry)) ||
	   ((flags & WALK_TRAILING) && !(nd->flags & LOOKUP_FOLLOW)) ||
	   (flags & WALK_NOFOLLOW)) {
		 
		if (nd->flags & LOOKUP_RCU) {
			if (read_seqcount_retry(&path.dentry->d_seq, nd->next_seq))
				return ERR_PTR(-ECHILD);
			if (unlikely(!inode))
				return ERR_PTR(-ENOENT);
		} else {
			dput(nd->path.dentry);
			if (nd->path.mnt != path.mnt)
				mntput(nd->path.mnt);
		}
		nd->path = path;
		nd->inode = inode;
		nd->seq = nd->next_seq;
		return NULL;
	}
	if (nd->flags & LOOKUP_RCU) {
		 
		if (read_seqcount_retry(&path.dentry->d_seq, nd->next_seq))
			return ERR_PTR(-ECHILD);
	} else {
		if (path.mnt == nd->path.mnt)
			mntget(path.mnt);
	}
	return pick_link(nd, &path, inode, flags);
}

static struct dentry *follow_dotdot_rcu(struct nameidata *nd)
{
	struct dentry *parent, *old;

	if (path_equal(&nd->path, &nd->root))
		goto in_root;
	if (unlikely(nd->path.dentry == nd->path.mnt->mnt_root)) {
		struct path path;
		unsigned seq;
		if (!choose_mountpoint_rcu(real_mount(nd->path.mnt),
					   &nd->root, &path, &seq))
			goto in_root;
		if (unlikely(nd->flags & LOOKUP_NO_XDEV))
			return ERR_PTR(-ECHILD);
		nd->path = path;
		nd->inode = path.dentry->d_inode;
		nd->seq = seq;
		 
		if (read_seqretry(&mount_lock, nd->m_seq))
			return ERR_PTR(-ECHILD);
		 
	}
	old = nd->path.dentry;
	parent = old->d_parent;
	nd->next_seq = read_seqcount_begin(&parent->d_seq);
	 
	if (read_seqcount_retry(&old->d_seq, nd->seq))
		return ERR_PTR(-ECHILD);
	if (unlikely(!path_connected(nd->path.mnt, parent)))
		return ERR_PTR(-ECHILD);
	return parent;
in_root:
	if (read_seqretry(&mount_lock, nd->m_seq))
		return ERR_PTR(-ECHILD);
	if (unlikely(nd->flags & LOOKUP_BENEATH))
		return ERR_PTR(-ECHILD);
	nd->next_seq = nd->seq;
	return nd->path.dentry;
}

static struct dentry *follow_dotdot(struct nameidata *nd)
{
	struct dentry *parent;

	if (path_equal(&nd->path, &nd->root))
		goto in_root;
	if (unlikely(nd->path.dentry == nd->path.mnt->mnt_root)) {
		struct path path;

		if (!choose_mountpoint(real_mount(nd->path.mnt),
				       &nd->root, &path))
			goto in_root;
		path_put(&nd->path);
		nd->path = path;
		nd->inode = path.dentry->d_inode;
		if (unlikely(nd->flags & LOOKUP_NO_XDEV))
			return ERR_PTR(-EXDEV);
	}
	 
	parent = dget_parent(nd->path.dentry);
	if (unlikely(!path_connected(nd->path.mnt, parent))) {
		dput(parent);
		return ERR_PTR(-ENOENT);
	}
	return parent;

in_root:
	if (unlikely(nd->flags & LOOKUP_BENEATH))
		return ERR_PTR(-EXDEV);
	return dget(nd->path.dentry);
}

static const char *handle_dots(struct nameidata *nd, int type)
{
	if (type == LAST_DOTDOT) {
		const char *error = NULL;
		struct dentry *parent;

		if (!nd->root.mnt) {
			error = ERR_PTR(set_root(nd));
			if (error)
				return error;
		}
		if (nd->flags & LOOKUP_RCU)
			parent = follow_dotdot_rcu(nd);
		else
			parent = follow_dotdot(nd);
		if (IS_ERR(parent))
			return ERR_CAST(parent);
		error = step_into(nd, WALK_NOFOLLOW, parent);
		if (unlikely(error))
			return error;

		if (unlikely(nd->flags & LOOKUP_IS_SCOPED)) {
			 
			smp_rmb();
			if (__read_seqcount_retry(&mount_lock.seqcount, nd->m_seq))
				return ERR_PTR(-EAGAIN);
			if (__read_seqcount_retry(&rename_lock.seqcount, nd->r_seq))
				return ERR_PTR(-EAGAIN);
		}
	}
	return NULL;
}

static const char *walk_component(struct nameidata *nd, int flags)
{
	struct dentry *dentry;
	 
	if (unlikely(nd->last_type != LAST_NORM)) {
		if (!(flags & WALK_MORE) && nd->depth)
			put_link(nd);
		return handle_dots(nd, nd->last_type);
	}
	dentry = lookup_fast(nd);
	if (IS_ERR(dentry))
		return ERR_CAST(dentry);
	if (unlikely(!dentry)) {
		dentry = lookup_slow(&nd->last, nd->path.dentry, nd->flags);
		if (IS_ERR(dentry))
			return ERR_CAST(dentry);
	}
	if (!(flags & WALK_MORE) && nd->depth)
		put_link(nd);
	return step_into(nd, flags, dentry);
}

 
#ifdef CONFIG_DCACHE_WORD_ACCESS

#include <asm/word-at-a-time.h>

#ifdef HASH_MIX

 

#elif defined(CONFIG_64BIT)
 
#define HASH_MIX(x, y, a)	\
	(	x ^= (a),	\
	y ^= x,	x = rol64(x,12),\
	x += y,	y = rol64(y,45),\
	y *= 9			)

 
static inline unsigned int fold_hash(unsigned long x, unsigned long y)
{
	y ^= x * GOLDEN_RATIO_64;
	y *= GOLDEN_RATIO_64;
	return y >> 32;
}

#else	 

 
#define HASH_MIX(x, y, a)	\
	(	x ^= (a),	\
	y ^= x,	x = rol32(x, 7),\
	x += y,	y = rol32(y,20),\
	y *= 9			)

static inline unsigned int fold_hash(unsigned long x, unsigned long y)
{
	 
	return __hash_32(y ^ __hash_32(x));
}

#endif

 
unsigned int full_name_hash(const void *salt, const char *name, unsigned int len)
{
	unsigned long a, x = 0, y = (unsigned long)salt;

	for (;;) {
		if (!len)
			goto done;
		a = load_unaligned_zeropad(name);
		if (len < sizeof(unsigned long))
			break;
		HASH_MIX(x, y, a);
		name += sizeof(unsigned long);
		len -= sizeof(unsigned long);
	}
	x ^= a & bytemask_from_count(len);
done:
	return fold_hash(x, y);
}
EXPORT_SYMBOL(full_name_hash);

 
u64 hashlen_string(const void *salt, const char *name)
{
	unsigned long a = 0, x = 0, y = (unsigned long)salt;
	unsigned long adata, mask, len;
	const struct word_at_a_time constants = WORD_AT_A_TIME_CONSTANTS;

	len = 0;
	goto inside;

	do {
		HASH_MIX(x, y, a);
		len += sizeof(unsigned long);
inside:
		a = load_unaligned_zeropad(name+len);
	} while (!has_zero(a, &adata, &constants));

	adata = prep_zero_mask(a, adata, &constants);
	mask = create_zero_mask(adata);
	x ^= a & zero_bytemask(mask);

	return hashlen_create(fold_hash(x, y), len + find_zero(mask));
}
EXPORT_SYMBOL(hashlen_string);

 
static inline u64 hash_name(const void *salt, const char *name)
{
	unsigned long a = 0, b, x = 0, y = (unsigned long)salt;
	unsigned long adata, bdata, mask, len;
	const struct word_at_a_time constants = WORD_AT_A_TIME_CONSTANTS;

	len = 0;
	goto inside;

	do {
		HASH_MIX(x, y, a);
		len += sizeof(unsigned long);
inside:
		a = load_unaligned_zeropad(name+len);
		b = a ^ REPEAT_BYTE('/');
	} while (!(has_zero(a, &adata, &constants) | has_zero(b, &bdata, &constants)));

	adata = prep_zero_mask(a, adata, &constants);
	bdata = prep_zero_mask(b, bdata, &constants);
	mask = create_zero_mask(adata | bdata);
	x ^= a & zero_bytemask(mask);

	return hashlen_create(fold_hash(x, y), len + find_zero(mask));
}

#else	 

 
unsigned int full_name_hash(const void *salt, const char *name, unsigned int len)
{
	unsigned long hash = init_name_hash(salt);
	while (len--)
		hash = partial_name_hash((unsigned char)*name++, hash);
	return end_name_hash(hash);
}
EXPORT_SYMBOL(full_name_hash);

 
u64 hashlen_string(const void *salt, const char *name)
{
	unsigned long hash = init_name_hash(salt);
	unsigned long len = 0, c;

	c = (unsigned char)*name;
	while (c) {
		len++;
		hash = partial_name_hash(c, hash);
		c = (unsigned char)name[len];
	}
	return hashlen_create(end_name_hash(hash), len);
}
EXPORT_SYMBOL(hashlen_string);

 
static inline u64 hash_name(const void *salt, const char *name)
{
	unsigned long hash = init_name_hash(salt);
	unsigned long len = 0, c;

	c = (unsigned char)*name;
	do {
		len++;
		hash = partial_name_hash(c, hash);
		c = (unsigned char)name[len];
	} while (c && c != '/');
	return hashlen_create(end_name_hash(hash), len);
}

#endif

 
static int link_path_walk(const char *name, struct nameidata *nd)
{
	int depth = 0;  
	int err;

	nd->last_type = LAST_ROOT;
	nd->flags |= LOOKUP_PARENT;
	if (IS_ERR(name))
		return PTR_ERR(name);
	while (*name=='/')
		name++;
	if (!*name) {
		nd->dir_mode = 0;  
		return 0;
	}

	 
	for(;;) {
		struct mnt_idmap *idmap;
		const char *link;
		u64 hash_len;
		int type;

		idmap = mnt_idmap(nd->path.mnt);
		err = may_lookup(idmap, nd);
		if (err)
			return err;

		hash_len = hash_name(nd->path.dentry, name);

		type = LAST_NORM;
		if (name[0] == '.') switch (hashlen_len(hash_len)) {
			case 2:
				if (name[1] == '.') {
					type = LAST_DOTDOT;
					nd->state |= ND_JUMPED;
				}
				break;
			case 1:
				type = LAST_DOT;
		}
		if (likely(type == LAST_NORM)) {
			struct dentry *parent = nd->path.dentry;
			nd->state &= ~ND_JUMPED;
			if (unlikely(parent->d_flags & DCACHE_OP_HASH)) {
				struct qstr this = { { .hash_len = hash_len }, .name = name };
				err = parent->d_op->d_hash(parent, &this);
				if (err < 0)
					return err;
				hash_len = this.hash_len;
				name = this.name;
			}
		}

		nd->last.hash_len = hash_len;
		nd->last.name = name;
		nd->last_type = type;

		name += hashlen_len(hash_len);
		if (!*name)
			goto OK;
		 
		do {
			name++;
		} while (unlikely(*name == '/'));
		if (unlikely(!*name)) {
OK:
			 
			if (!depth) {
				nd->dir_vfsuid = i_uid_into_vfsuid(idmap, nd->inode);
				nd->dir_mode = nd->inode->i_mode;
				nd->flags &= ~LOOKUP_PARENT;
				return 0;
			}
			 
			name = nd->stack[--depth].name;
			link = walk_component(nd, 0);
		} else {
			 
			link = walk_component(nd, WALK_MORE);
		}
		if (unlikely(link)) {
			if (IS_ERR(link))
				return PTR_ERR(link);
			 
			nd->stack[depth++].name = name;
			name = link;
			continue;
		}
		if (unlikely(!d_can_lookup(nd->path.dentry))) {
			if (nd->flags & LOOKUP_RCU) {
				if (!try_to_unlazy(nd))
					return -ECHILD;
			}
			return -ENOTDIR;
		}
	}
}

 
static const char *path_init(struct nameidata *nd, unsigned flags)
{
	int error;
	const char *s = nd->name->name;

	 
	if ((flags & (LOOKUP_RCU | LOOKUP_CACHED)) == LOOKUP_CACHED)
		return ERR_PTR(-EAGAIN);

	if (!*s)
		flags &= ~LOOKUP_RCU;
	if (flags & LOOKUP_RCU)
		rcu_read_lock();
	else
		nd->seq = nd->next_seq = 0;

	nd->flags = flags;
	nd->state |= ND_JUMPED;

	nd->m_seq = __read_seqcount_begin(&mount_lock.seqcount);
	nd->r_seq = __read_seqcount_begin(&rename_lock.seqcount);
	smp_rmb();

	if (nd->state & ND_ROOT_PRESET) {
		struct dentry *root = nd->root.dentry;
		struct inode *inode = root->d_inode;
		if (*s && unlikely(!d_can_lookup(root)))
			return ERR_PTR(-ENOTDIR);
		nd->path = nd->root;
		nd->inode = inode;
		if (flags & LOOKUP_RCU) {
			nd->seq = read_seqcount_begin(&nd->path.dentry->d_seq);
			nd->root_seq = nd->seq;
		} else {
			path_get(&nd->path);
		}
		return s;
	}

	nd->root.mnt = NULL;

	 
	if (*s == '/' && !(flags & LOOKUP_IN_ROOT)) {
		error = nd_jump_root(nd);
		if (unlikely(error))
			return ERR_PTR(error);
		return s;
	}

	 
	if (nd->dfd == AT_FDCWD) {
		if (flags & LOOKUP_RCU) {
			struct fs_struct *fs = current->fs;
			unsigned seq;

			do {
				seq = read_seqcount_begin(&fs->seq);
				nd->path = fs->pwd;
				nd->inode = nd->path.dentry->d_inode;
				nd->seq = __read_seqcount_begin(&nd->path.dentry->d_seq);
			} while (read_seqcount_retry(&fs->seq, seq));
		} else {
			get_fs_pwd(current->fs, &nd->path);
			nd->inode = nd->path.dentry->d_inode;
		}
	} else {
		 
		struct fd f = fdget_raw(nd->dfd);
		struct dentry *dentry;

		if (!f.file)
			return ERR_PTR(-EBADF);

		dentry = f.file->f_path.dentry;

		if (*s && unlikely(!d_can_lookup(dentry))) {
			fdput(f);
			return ERR_PTR(-ENOTDIR);
		}

		nd->path = f.file->f_path;
		if (flags & LOOKUP_RCU) {
			nd->inode = nd->path.dentry->d_inode;
			nd->seq = read_seqcount_begin(&nd->path.dentry->d_seq);
		} else {
			path_get(&nd->path);
			nd->inode = nd->path.dentry->d_inode;
		}
		fdput(f);
	}

	 
	if (flags & LOOKUP_IS_SCOPED) {
		nd->root = nd->path;
		if (flags & LOOKUP_RCU) {
			nd->root_seq = nd->seq;
		} else {
			path_get(&nd->root);
			nd->state |= ND_ROOT_GRABBED;
		}
	}
	return s;
}

static inline const char *lookup_last(struct nameidata *nd)
{
	if (nd->last_type == LAST_NORM && nd->last.name[nd->last.len])
		nd->flags |= LOOKUP_FOLLOW | LOOKUP_DIRECTORY;

	return walk_component(nd, WALK_TRAILING);
}

static int handle_lookup_down(struct nameidata *nd)
{
	if (!(nd->flags & LOOKUP_RCU))
		dget(nd->path.dentry);
	nd->next_seq = nd->seq;
	return PTR_ERR(step_into(nd, WALK_NOFOLLOW, nd->path.dentry));
}

 
static int path_lookupat(struct nameidata *nd, unsigned flags, struct path *path)
{
	const char *s = path_init(nd, flags);
	int err;

	if (unlikely(flags & LOOKUP_DOWN) && !IS_ERR(s)) {
		err = handle_lookup_down(nd);
		if (unlikely(err < 0))
			s = ERR_PTR(err);
	}

	while (!(err = link_path_walk(s, nd)) &&
	       (s = lookup_last(nd)) != NULL)
		;
	if (!err && unlikely(nd->flags & LOOKUP_MOUNTPOINT)) {
		err = handle_lookup_down(nd);
		nd->state &= ~ND_JUMPED; 
	}
	if (!err)
		err = complete_walk(nd);

	if (!err && nd->flags & LOOKUP_DIRECTORY)
		if (!d_can_lookup(nd->path.dentry))
			err = -ENOTDIR;
	if (!err) {
		*path = nd->path;
		nd->path.mnt = NULL;
		nd->path.dentry = NULL;
	}
	terminate_walk(nd);
	return err;
}

int filename_lookup(int dfd, struct filename *name, unsigned flags,
		    struct path *path, struct path *root)
{
	int retval;
	struct nameidata nd;
	if (IS_ERR(name))
		return PTR_ERR(name);
	set_nameidata(&nd, dfd, name, root);
	retval = path_lookupat(&nd, flags | LOOKUP_RCU, path);
	if (unlikely(retval == -ECHILD))
		retval = path_lookupat(&nd, flags, path);
	if (unlikely(retval == -ESTALE))
		retval = path_lookupat(&nd, flags | LOOKUP_REVAL, path);

	if (likely(!retval))
		audit_inode(name, path->dentry,
			    flags & LOOKUP_MOUNTPOINT ? AUDIT_INODE_NOEVAL : 0);
	restore_nameidata();
	return retval;
}

 
static int path_parentat(struct nameidata *nd, unsigned flags,
				struct path *parent)
{
	const char *s = path_init(nd, flags);
	int err = link_path_walk(s, nd);
	if (!err)
		err = complete_walk(nd);
	if (!err) {
		*parent = nd->path;
		nd->path.mnt = NULL;
		nd->path.dentry = NULL;
	}
	terminate_walk(nd);
	return err;
}

 
static int __filename_parentat(int dfd, struct filename *name,
			       unsigned int flags, struct path *parent,
			       struct qstr *last, int *type,
			       const struct path *root)
{
	int retval;
	struct nameidata nd;

	if (IS_ERR(name))
		return PTR_ERR(name);
	set_nameidata(&nd, dfd, name, root);
	retval = path_parentat(&nd, flags | LOOKUP_RCU, parent);
	if (unlikely(retval == -ECHILD))
		retval = path_parentat(&nd, flags, parent);
	if (unlikely(retval == -ESTALE))
		retval = path_parentat(&nd, flags | LOOKUP_REVAL, parent);
	if (likely(!retval)) {
		*last = nd.last;
		*type = nd.last_type;
		audit_inode(name, parent->dentry, AUDIT_INODE_PARENT);
	}
	restore_nameidata();
	return retval;
}

static int filename_parentat(int dfd, struct filename *name,
			     unsigned int flags, struct path *parent,
			     struct qstr *last, int *type)
{
	return __filename_parentat(dfd, name, flags, parent, last, type, NULL);
}

 
static struct dentry *__kern_path_locked(struct filename *name, struct path *path)
{
	struct dentry *d;
	struct qstr last;
	int type, error;

	error = filename_parentat(AT_FDCWD, name, 0, path, &last, &type);
	if (error)
		return ERR_PTR(error);
	if (unlikely(type != LAST_NORM)) {
		path_put(path);
		return ERR_PTR(-EINVAL);
	}
	inode_lock_nested(path->dentry->d_inode, I_MUTEX_PARENT);
	d = lookup_one_qstr_excl(&last, path->dentry, 0);
	if (IS_ERR(d)) {
		inode_unlock(path->dentry->d_inode);
		path_put(path);
	}
	return d;
}

struct dentry *kern_path_locked(const char *name, struct path *path)
{
	struct filename *filename = getname_kernel(name);
	struct dentry *res = __kern_path_locked(filename, path);

	putname(filename);
	return res;
}

int kern_path(const char *name, unsigned int flags, struct path *path)
{
	struct filename *filename = getname_kernel(name);
	int ret = filename_lookup(AT_FDCWD, filename, flags, path, NULL);

	putname(filename);
	return ret;

}
EXPORT_SYMBOL(kern_path);

 
int vfs_path_parent_lookup(struct filename *filename, unsigned int flags,
			   struct path *parent, struct qstr *last, int *type,
			   const struct path *root)
{
	return  __filename_parentat(AT_FDCWD, filename, flags, parent, last,
				    type, root);
}
EXPORT_SYMBOL(vfs_path_parent_lookup);

 
int vfs_path_lookup(struct dentry *dentry, struct vfsmount *mnt,
		    const char *name, unsigned int flags,
		    struct path *path)
{
	struct filename *filename;
	struct path root = {.mnt = mnt, .dentry = dentry};
	int ret;

	filename = getname_kernel(name);
	 
	ret = filename_lookup(AT_FDCWD, filename, flags, path, &root);
	putname(filename);
	return ret;
}
EXPORT_SYMBOL(vfs_path_lookup);

static int lookup_one_common(struct mnt_idmap *idmap,
			     const char *name, struct dentry *base, int len,
			     struct qstr *this)
{
	this->name = name;
	this->len = len;
	this->hash = full_name_hash(base, name, len);
	if (!len)
		return -EACCES;

	if (unlikely(name[0] == '.')) {
		if (len < 2 || (len == 2 && name[1] == '.'))
			return -EACCES;
	}

	while (len--) {
		unsigned int c = *(const unsigned char *)name++;
		if (c == '/' || c == '\0')
			return -EACCES;
	}
	 
	if (base->d_flags & DCACHE_OP_HASH) {
		int err = base->d_op->d_hash(base, this);
		if (err < 0)
			return err;
	}

	return inode_permission(idmap, base->d_inode, MAY_EXEC);
}

 
struct dentry *try_lookup_one_len(const char *name, struct dentry *base, int len)
{
	struct qstr this;
	int err;

	WARN_ON_ONCE(!inode_is_locked(base->d_inode));

	err = lookup_one_common(&nop_mnt_idmap, name, base, len, &this);
	if (err)
		return ERR_PTR(err);

	return lookup_dcache(&this, base, 0);
}
EXPORT_SYMBOL(try_lookup_one_len);

 
struct dentry *lookup_one_len(const char *name, struct dentry *base, int len)
{
	struct dentry *dentry;
	struct qstr this;
	int err;

	WARN_ON_ONCE(!inode_is_locked(base->d_inode));

	err = lookup_one_common(&nop_mnt_idmap, name, base, len, &this);
	if (err)
		return ERR_PTR(err);

	dentry = lookup_dcache(&this, base, 0);
	return dentry ? dentry : __lookup_slow(&this, base, 0);
}
EXPORT_SYMBOL(lookup_one_len);

 
struct dentry *lookup_one(struct mnt_idmap *idmap, const char *name,
			  struct dentry *base, int len)
{
	struct dentry *dentry;
	struct qstr this;
	int err;

	WARN_ON_ONCE(!inode_is_locked(base->d_inode));

	err = lookup_one_common(idmap, name, base, len, &this);
	if (err)
		return ERR_PTR(err);

	dentry = lookup_dcache(&this, base, 0);
	return dentry ? dentry : __lookup_slow(&this, base, 0);
}
EXPORT_SYMBOL(lookup_one);

 
struct dentry *lookup_one_unlocked(struct mnt_idmap *idmap,
				   const char *name, struct dentry *base,
				   int len)
{
	struct qstr this;
	int err;
	struct dentry *ret;

	err = lookup_one_common(idmap, name, base, len, &this);
	if (err)
		return ERR_PTR(err);

	ret = lookup_dcache(&this, base, 0);
	if (!ret)
		ret = lookup_slow(&this, base, 0);
	return ret;
}
EXPORT_SYMBOL(lookup_one_unlocked);

 
struct dentry *lookup_one_positive_unlocked(struct mnt_idmap *idmap,
					    const char *name,
					    struct dentry *base, int len)
{
	struct dentry *ret = lookup_one_unlocked(idmap, name, base, len);

	if (!IS_ERR(ret) && d_flags_negative(smp_load_acquire(&ret->d_flags))) {
		dput(ret);
		ret = ERR_PTR(-ENOENT);
	}
	return ret;
}
EXPORT_SYMBOL(lookup_one_positive_unlocked);

 
struct dentry *lookup_one_len_unlocked(const char *name,
				       struct dentry *base, int len)
{
	return lookup_one_unlocked(&nop_mnt_idmap, name, base, len);
}
EXPORT_SYMBOL(lookup_one_len_unlocked);

 
struct dentry *lookup_positive_unlocked(const char *name,
				       struct dentry *base, int len)
{
	return lookup_one_positive_unlocked(&nop_mnt_idmap, name, base, len);
}
EXPORT_SYMBOL(lookup_positive_unlocked);

#ifdef CONFIG_UNIX98_PTYS
int path_pts(struct path *path)
{
	 
	struct dentry *parent = dget_parent(path->dentry);
	struct dentry *child;
	struct qstr this = QSTR_INIT("pts", 3);

	if (unlikely(!path_connected(path->mnt, parent))) {
		dput(parent);
		return -ENOENT;
	}
	dput(path->dentry);
	path->dentry = parent;
	child = d_hash_and_lookup(parent, &this);
	if (IS_ERR_OR_NULL(child))
		return -ENOENT;

	path->dentry = child;
	dput(parent);
	follow_down(path, 0);
	return 0;
}
#endif

int user_path_at_empty(int dfd, const char __user *name, unsigned flags,
		 struct path *path, int *empty)
{
	struct filename *filename = getname_flags(name, flags, empty);
	int ret = filename_lookup(dfd, filename, flags, path, NULL);

	putname(filename);
	return ret;
}
EXPORT_SYMBOL(user_path_at_empty);

int __check_sticky(struct mnt_idmap *idmap, struct inode *dir,
		   struct inode *inode)
{
	kuid_t fsuid = current_fsuid();

	if (vfsuid_eq_kuid(i_uid_into_vfsuid(idmap, inode), fsuid))
		return 0;
	if (vfsuid_eq_kuid(i_uid_into_vfsuid(idmap, dir), fsuid))
		return 0;
	return !capable_wrt_inode_uidgid(idmap, inode, CAP_FOWNER);
}
EXPORT_SYMBOL(__check_sticky);

 
static int may_delete(struct mnt_idmap *idmap, struct inode *dir,
		      struct dentry *victim, bool isdir)
{
	struct inode *inode = d_backing_inode(victim);
	int error;

	if (d_is_negative(victim))
		return -ENOENT;
	BUG_ON(!inode);

	BUG_ON(victim->d_parent->d_inode != dir);

	 
	if (!vfsuid_valid(i_uid_into_vfsuid(idmap, inode)) ||
	    !vfsgid_valid(i_gid_into_vfsgid(idmap, inode)))
		return -EOVERFLOW;

	audit_inode_child(dir, victim, AUDIT_TYPE_CHILD_DELETE);

	error = inode_permission(idmap, dir, MAY_WRITE | MAY_EXEC);
	if (error)
		return error;
	if (IS_APPEND(dir))
		return -EPERM;

	if (check_sticky(idmap, dir, inode) || IS_APPEND(inode) ||
	    IS_IMMUTABLE(inode) || IS_SWAPFILE(inode) ||
	    HAS_UNMAPPED_ID(idmap, inode))
		return -EPERM;
	if (isdir) {
		if (!d_is_dir(victim))
			return -ENOTDIR;
		if (IS_ROOT(victim))
			return -EBUSY;
	} else if (d_is_dir(victim))
		return -EISDIR;
	if (IS_DEADDIR(dir))
		return -ENOENT;
	if (victim->d_flags & DCACHE_NFSFS_RENAMED)
		return -EBUSY;
	return 0;
}

 
static inline int may_create(struct mnt_idmap *idmap,
			     struct inode *dir, struct dentry *child)
{
	audit_inode_child(dir, child, AUDIT_TYPE_CHILD_CREATE);
	if (child->d_inode)
		return -EEXIST;
	if (IS_DEADDIR(dir))
		return -ENOENT;
	if (!fsuidgid_has_mapping(dir->i_sb, idmap))
		return -EOVERFLOW;

	return inode_permission(idmap, dir, MAY_WRITE | MAY_EXEC);
}

static struct dentry *lock_two_directories(struct dentry *p1, struct dentry *p2)
{
	struct dentry *p;

	p = d_ancestor(p2, p1);
	if (p) {
		inode_lock_nested(p2->d_inode, I_MUTEX_PARENT);
		inode_lock_nested(p1->d_inode, I_MUTEX_CHILD);
		return p;
	}

	p = d_ancestor(p1, p2);
	if (p) {
		inode_lock_nested(p1->d_inode, I_MUTEX_PARENT);
		inode_lock_nested(p2->d_inode, I_MUTEX_CHILD);
		return p;
	}

	lock_two_inodes(p1->d_inode, p2->d_inode,
			I_MUTEX_PARENT, I_MUTEX_PARENT2);
	return NULL;
}

 
struct dentry *lock_rename(struct dentry *p1, struct dentry *p2)
{
	if (p1 == p2) {
		inode_lock_nested(p1->d_inode, I_MUTEX_PARENT);
		return NULL;
	}

	mutex_lock(&p1->d_sb->s_vfs_rename_mutex);
	return lock_two_directories(p1, p2);
}
EXPORT_SYMBOL(lock_rename);

 
struct dentry *lock_rename_child(struct dentry *c1, struct dentry *p2)
{
	if (READ_ONCE(c1->d_parent) == p2) {
		 
		inode_lock_nested(p2->d_inode, I_MUTEX_PARENT);
		 
		if (likely(c1->d_parent == p2))
			return NULL;

		 
		inode_unlock(p2->d_inode);
	}

	mutex_lock(&c1->d_sb->s_vfs_rename_mutex);
	 
	if (likely(c1->d_parent != p2))
		return lock_two_directories(c1->d_parent, p2);

	 
	inode_lock_nested(p2->d_inode, I_MUTEX_PARENT);
	mutex_unlock(&c1->d_sb->s_vfs_rename_mutex);
	return NULL;
}
EXPORT_SYMBOL(lock_rename_child);

void unlock_rename(struct dentry *p1, struct dentry *p2)
{
	inode_unlock(p1->d_inode);
	if (p1 != p2) {
		inode_unlock(p2->d_inode);
		mutex_unlock(&p1->d_sb->s_vfs_rename_mutex);
	}
}
EXPORT_SYMBOL(unlock_rename);

 
static inline umode_t mode_strip_umask(const struct inode *dir, umode_t mode)
{
	if (!IS_POSIXACL(dir))
		mode &= ~current_umask();
	return mode;
}

 
static inline umode_t vfs_prepare_mode(struct mnt_idmap *idmap,
				       const struct inode *dir, umode_t mode,
				       umode_t mask_perms, umode_t type)
{
	mode = mode_strip_sgid(idmap, dir, mode);
	mode = mode_strip_umask(dir, mode);

	 
	mode &= (mask_perms & ~S_IFMT);
	mode |= (type & S_IFMT);

	return mode;
}

 
int vfs_create(struct mnt_idmap *idmap, struct inode *dir,
	       struct dentry *dentry, umode_t mode, bool want_excl)
{
	int error;

	error = may_create(idmap, dir, dentry);
	if (error)
		return error;

	if (!dir->i_op->create)
		return -EACCES;	 

	mode = vfs_prepare_mode(idmap, dir, mode, S_IALLUGO, S_IFREG);
	error = security_inode_create(dir, dentry, mode);
	if (error)
		return error;
	error = dir->i_op->create(idmap, dir, dentry, mode, want_excl);
	if (!error)
		fsnotify_create(dir, dentry);
	return error;
}
EXPORT_SYMBOL(vfs_create);

int vfs_mkobj(struct dentry *dentry, umode_t mode,
		int (*f)(struct dentry *, umode_t, void *),
		void *arg)
{
	struct inode *dir = dentry->d_parent->d_inode;
	int error = may_create(&nop_mnt_idmap, dir, dentry);
	if (error)
		return error;

	mode &= S_IALLUGO;
	mode |= S_IFREG;
	error = security_inode_create(dir, dentry, mode);
	if (error)
		return error;
	error = f(dentry, mode, arg);
	if (!error)
		fsnotify_create(dir, dentry);
	return error;
}
EXPORT_SYMBOL(vfs_mkobj);

bool may_open_dev(const struct path *path)
{
	return !(path->mnt->mnt_flags & MNT_NODEV) &&
		!(path->mnt->mnt_sb->s_iflags & SB_I_NODEV);
}

static int may_open(struct mnt_idmap *idmap, const struct path *path,
		    int acc_mode, int flag)
{
	struct dentry *dentry = path->dentry;
	struct inode *inode = dentry->d_inode;
	int error;

	if (!inode)
		return -ENOENT;

	switch (inode->i_mode & S_IFMT) {
	case S_IFLNK:
		return -ELOOP;
	case S_IFDIR:
		if (acc_mode & MAY_WRITE)
			return -EISDIR;
		if (acc_mode & MAY_EXEC)
			return -EACCES;
		break;
	case S_IFBLK:
	case S_IFCHR:
		if (!may_open_dev(path))
			return -EACCES;
		fallthrough;
	case S_IFIFO:
	case S_IFSOCK:
		if (acc_mode & MAY_EXEC)
			return -EACCES;
		flag &= ~O_TRUNC;
		break;
	case S_IFREG:
		if ((acc_mode & MAY_EXEC) && path_noexec(path))
			return -EACCES;
		break;
	}

	error = inode_permission(idmap, inode, MAY_OPEN | acc_mode);
	if (error)
		return error;

	 
	if (IS_APPEND(inode)) {
		if  ((flag & O_ACCMODE) != O_RDONLY && !(flag & O_APPEND))
			return -EPERM;
		if (flag & O_TRUNC)
			return -EPERM;
	}

	 
	if (flag & O_NOATIME && !inode_owner_or_capable(idmap, inode))
		return -EPERM;

	return 0;
}

static int handle_truncate(struct mnt_idmap *idmap, struct file *filp)
{
	const struct path *path = &filp->f_path;
	struct inode *inode = path->dentry->d_inode;
	int error = get_write_access(inode);
	if (error)
		return error;

	error = security_file_truncate(filp);
	if (!error) {
		error = do_truncate(idmap, path->dentry, 0,
				    ATTR_MTIME|ATTR_CTIME|ATTR_OPEN,
				    filp);
	}
	put_write_access(inode);
	return error;
}

static inline int open_to_namei_flags(int flag)
{
	if ((flag & O_ACCMODE) == 3)
		flag--;
	return flag;
}

static int may_o_create(struct mnt_idmap *idmap,
			const struct path *dir, struct dentry *dentry,
			umode_t mode)
{
	int error = security_path_mknod(dir, dentry, mode, 0);
	if (error)
		return error;

	if (!fsuidgid_has_mapping(dir->dentry->d_sb, idmap))
		return -EOVERFLOW;

	error = inode_permission(idmap, dir->dentry->d_inode,
				 MAY_WRITE | MAY_EXEC);
	if (error)
		return error;

	return security_inode_create(dir->dentry->d_inode, dentry, mode);
}

 
static struct dentry *atomic_open(struct nameidata *nd, struct dentry *dentry,
				  struct file *file,
				  int open_flag, umode_t mode)
{
	struct dentry *const DENTRY_NOT_SET = (void *) -1UL;
	struct inode *dir =  nd->path.dentry->d_inode;
	int error;

	if (nd->flags & LOOKUP_DIRECTORY)
		open_flag |= O_DIRECTORY;

	file->f_path.dentry = DENTRY_NOT_SET;
	file->f_path.mnt = nd->path.mnt;
	error = dir->i_op->atomic_open(dir, dentry, file,
				       open_to_namei_flags(open_flag), mode);
	d_lookup_done(dentry);
	if (!error) {
		if (file->f_mode & FMODE_OPENED) {
			if (unlikely(dentry != file->f_path.dentry)) {
				dput(dentry);
				dentry = dget(file->f_path.dentry);
			}
		} else if (WARN_ON(file->f_path.dentry == DENTRY_NOT_SET)) {
			error = -EIO;
		} else {
			if (file->f_path.dentry) {
				dput(dentry);
				dentry = file->f_path.dentry;
			}
			if (unlikely(d_is_negative(dentry)))
				error = -ENOENT;
		}
	}
	if (error) {
		dput(dentry);
		dentry = ERR_PTR(error);
	}
	return dentry;
}

 
static struct dentry *lookup_open(struct nameidata *nd, struct file *file,
				  const struct open_flags *op,
				  bool got_write)
{
	struct mnt_idmap *idmap;
	struct dentry *dir = nd->path.dentry;
	struct inode *dir_inode = dir->d_inode;
	int open_flag = op->open_flag;
	struct dentry *dentry;
	int error, create_error = 0;
	umode_t mode = op->mode;
	DECLARE_WAIT_QUEUE_HEAD_ONSTACK(wq);

	if (unlikely(IS_DEADDIR(dir_inode)))
		return ERR_PTR(-ENOENT);

	file->f_mode &= ~FMODE_CREATED;
	dentry = d_lookup(dir, &nd->last);
	for (;;) {
		if (!dentry) {
			dentry = d_alloc_parallel(dir, &nd->last, &wq);
			if (IS_ERR(dentry))
				return dentry;
		}
		if (d_in_lookup(dentry))
			break;

		error = d_revalidate(dentry, nd->flags);
		if (likely(error > 0))
			break;
		if (error)
			goto out_dput;
		d_invalidate(dentry);
		dput(dentry);
		dentry = NULL;
	}
	if (dentry->d_inode) {
		 
		return dentry;
	}

	 
	if (unlikely(!got_write))
		open_flag &= ~O_TRUNC;
	idmap = mnt_idmap(nd->path.mnt);
	if (open_flag & O_CREAT) {
		if (open_flag & O_EXCL)
			open_flag &= ~O_TRUNC;
		mode = vfs_prepare_mode(idmap, dir->d_inode, mode, mode, mode);
		if (likely(got_write))
			create_error = may_o_create(idmap, &nd->path,
						    dentry, mode);
		else
			create_error = -EROFS;
	}
	if (create_error)
		open_flag &= ~O_CREAT;
	if (dir_inode->i_op->atomic_open) {
		dentry = atomic_open(nd, dentry, file, open_flag, mode);
		if (unlikely(create_error) && dentry == ERR_PTR(-ENOENT))
			dentry = ERR_PTR(create_error);
		return dentry;
	}

	if (d_in_lookup(dentry)) {
		struct dentry *res = dir_inode->i_op->lookup(dir_inode, dentry,
							     nd->flags);
		d_lookup_done(dentry);
		if (unlikely(res)) {
			if (IS_ERR(res)) {
				error = PTR_ERR(res);
				goto out_dput;
			}
			dput(dentry);
			dentry = res;
		}
	}

	 
	if (!dentry->d_inode && (open_flag & O_CREAT)) {
		file->f_mode |= FMODE_CREATED;
		audit_inode_child(dir_inode, dentry, AUDIT_TYPE_CHILD_CREATE);
		if (!dir_inode->i_op->create) {
			error = -EACCES;
			goto out_dput;
		}

		error = dir_inode->i_op->create(idmap, dir_inode, dentry,
						mode, open_flag & O_EXCL);
		if (error)
			goto out_dput;
	}
	if (unlikely(create_error) && !dentry->d_inode) {
		error = create_error;
		goto out_dput;
	}
	return dentry;

out_dput:
	dput(dentry);
	return ERR_PTR(error);
}

static const char *open_last_lookups(struct nameidata *nd,
		   struct file *file, const struct open_flags *op)
{
	struct dentry *dir = nd->path.dentry;
	int open_flag = op->open_flag;
	bool got_write = false;
	struct dentry *dentry;
	const char *res;

	nd->flags |= op->intent;

	if (nd->last_type != LAST_NORM) {
		if (nd->depth)
			put_link(nd);
		return handle_dots(nd, nd->last_type);
	}

	if (!(open_flag & O_CREAT)) {
		if (nd->last.name[nd->last.len])
			nd->flags |= LOOKUP_FOLLOW | LOOKUP_DIRECTORY;
		 
		dentry = lookup_fast(nd);
		if (IS_ERR(dentry))
			return ERR_CAST(dentry);
		if (likely(dentry))
			goto finish_lookup;

		BUG_ON(nd->flags & LOOKUP_RCU);
	} else {
		 
		if (nd->flags & LOOKUP_RCU) {
			if (!try_to_unlazy(nd))
				return ERR_PTR(-ECHILD);
		}
		audit_inode(nd->name, dir, AUDIT_INODE_PARENT);
		 
		if (unlikely(nd->last.name[nd->last.len]))
			return ERR_PTR(-EISDIR);
	}

	if (open_flag & (O_CREAT | O_TRUNC | O_WRONLY | O_RDWR)) {
		got_write = !mnt_want_write(nd->path.mnt);
		 
	}
	if (open_flag & O_CREAT)
		inode_lock(dir->d_inode);
	else
		inode_lock_shared(dir->d_inode);
	dentry = lookup_open(nd, file, op, got_write);
	if (!IS_ERR(dentry) && (file->f_mode & FMODE_CREATED))
		fsnotify_create(dir->d_inode, dentry);
	if (open_flag & O_CREAT)
		inode_unlock(dir->d_inode);
	else
		inode_unlock_shared(dir->d_inode);

	if (got_write)
		mnt_drop_write(nd->path.mnt);

	if (IS_ERR(dentry))
		return ERR_CAST(dentry);

	if (file->f_mode & (FMODE_OPENED | FMODE_CREATED)) {
		dput(nd->path.dentry);
		nd->path.dentry = dentry;
		return NULL;
	}

finish_lookup:
	if (nd->depth)
		put_link(nd);
	res = step_into(nd, WALK_TRAILING, dentry);
	if (unlikely(res))
		nd->flags &= ~(LOOKUP_OPEN|LOOKUP_CREATE|LOOKUP_EXCL);
	return res;
}

 
static int do_open(struct nameidata *nd,
		   struct file *file, const struct open_flags *op)
{
	struct mnt_idmap *idmap;
	int open_flag = op->open_flag;
	bool do_truncate;
	int acc_mode;
	int error;

	if (!(file->f_mode & (FMODE_OPENED | FMODE_CREATED))) {
		error = complete_walk(nd);
		if (error)
			return error;
	}
	if (!(file->f_mode & FMODE_CREATED))
		audit_inode(nd->name, nd->path.dentry, 0);
	idmap = mnt_idmap(nd->path.mnt);
	if (open_flag & O_CREAT) {
		if ((open_flag & O_EXCL) && !(file->f_mode & FMODE_CREATED))
			return -EEXIST;
		if (d_is_dir(nd->path.dentry))
			return -EISDIR;
		error = may_create_in_sticky(idmap, nd,
					     d_backing_inode(nd->path.dentry));
		if (unlikely(error))
			return error;
	}
	if ((nd->flags & LOOKUP_DIRECTORY) && !d_can_lookup(nd->path.dentry))
		return -ENOTDIR;

	do_truncate = false;
	acc_mode = op->acc_mode;
	if (file->f_mode & FMODE_CREATED) {
		 
		open_flag &= ~O_TRUNC;
		acc_mode = 0;
	} else if (d_is_reg(nd->path.dentry) && open_flag & O_TRUNC) {
		error = mnt_want_write(nd->path.mnt);
		if (error)
			return error;
		do_truncate = true;
	}
	error = may_open(idmap, &nd->path, acc_mode, open_flag);
	if (!error && !(file->f_mode & FMODE_OPENED))
		error = vfs_open(&nd->path, file);
	if (!error)
		error = ima_file_check(file, op->acc_mode);
	if (!error && do_truncate)
		error = handle_truncate(idmap, file);
	if (unlikely(error > 0)) {
		WARN_ON(1);
		error = -EINVAL;
	}
	if (do_truncate)
		mnt_drop_write(nd->path.mnt);
	return error;
}

 
static int vfs_tmpfile(struct mnt_idmap *idmap,
		       const struct path *parentpath,
		       struct file *file, umode_t mode)
{
	struct dentry *child;
	struct inode *dir = d_inode(parentpath->dentry);
	struct inode *inode;
	int error;
	int open_flag = file->f_flags;

	 
	error = inode_permission(idmap, dir, MAY_WRITE | MAY_EXEC);
	if (error)
		return error;
	if (!dir->i_op->tmpfile)
		return -EOPNOTSUPP;
	child = d_alloc(parentpath->dentry, &slash_name);
	if (unlikely(!child))
		return -ENOMEM;
	file->f_path.mnt = parentpath->mnt;
	file->f_path.dentry = child;
	mode = vfs_prepare_mode(idmap, dir, mode, mode, mode);
	error = dir->i_op->tmpfile(idmap, dir, file, mode);
	dput(child);
	if (error)
		return error;
	 
	error = may_open(idmap, &file->f_path, 0, file->f_flags);
	if (error)
		return error;
	inode = file_inode(file);
	if (!(open_flag & O_EXCL)) {
		spin_lock(&inode->i_lock);
		inode->i_state |= I_LINKABLE;
		spin_unlock(&inode->i_lock);
	}
	ima_post_create_tmpfile(idmap, inode);
	return 0;
}

 
struct file *kernel_tmpfile_open(struct mnt_idmap *idmap,
				 const struct path *parentpath,
				 umode_t mode, int open_flag,
				 const struct cred *cred)
{
	struct file *file;
	int error;

	file = alloc_empty_file_noaccount(open_flag, cred);
	if (IS_ERR(file))
		return file;

	error = vfs_tmpfile(idmap, parentpath, file, mode);
	if (error) {
		fput(file);
		file = ERR_PTR(error);
	}
	return file;
}
EXPORT_SYMBOL(kernel_tmpfile_open);

static int do_tmpfile(struct nameidata *nd, unsigned flags,
		const struct open_flags *op,
		struct file *file)
{
	struct path path;
	int error = path_lookupat(nd, flags | LOOKUP_DIRECTORY, &path);

	if (unlikely(error))
		return error;
	error = mnt_want_write(path.mnt);
	if (unlikely(error))
		goto out;
	error = vfs_tmpfile(mnt_idmap(path.mnt), &path, file, op->mode);
	if (error)
		goto out2;
	audit_inode(nd->name, file->f_path.dentry, 0);
out2:
	mnt_drop_write(path.mnt);
out:
	path_put(&path);
	return error;
}

static int do_o_path(struct nameidata *nd, unsigned flags, struct file *file)
{
	struct path path;
	int error = path_lookupat(nd, flags, &path);
	if (!error) {
		audit_inode(nd->name, path.dentry, 0);
		error = vfs_open(&path, file);
		path_put(&path);
	}
	return error;
}

static struct file *path_openat(struct nameidata *nd,
			const struct open_flags *op, unsigned flags)
{
	struct file *file;
	int error;

	file = alloc_empty_file(op->open_flag, current_cred());
	if (IS_ERR(file))
		return file;

	if (unlikely(file->f_flags & __O_TMPFILE)) {
		error = do_tmpfile(nd, flags, op, file);
	} else if (unlikely(file->f_flags & O_PATH)) {
		error = do_o_path(nd, flags, file);
	} else {
		const char *s = path_init(nd, flags);
		while (!(error = link_path_walk(s, nd)) &&
		       (s = open_last_lookups(nd, file, op)) != NULL)
			;
		if (!error)
			error = do_open(nd, file, op);
		terminate_walk(nd);
	}
	if (likely(!error)) {
		if (likely(file->f_mode & FMODE_OPENED))
			return file;
		WARN_ON(1);
		error = -EINVAL;
	}
	fput(file);
	if (error == -EOPENSTALE) {
		if (flags & LOOKUP_RCU)
			error = -ECHILD;
		else
			error = -ESTALE;
	}
	return ERR_PTR(error);
}

struct file *do_filp_open(int dfd, struct filename *pathname,
		const struct open_flags *op)
{
	struct nameidata nd;
	int flags = op->lookup_flags;
	struct file *filp;

	set_nameidata(&nd, dfd, pathname, NULL);
	filp = path_openat(&nd, op, flags | LOOKUP_RCU);
	if (unlikely(filp == ERR_PTR(-ECHILD)))
		filp = path_openat(&nd, op, flags);
	if (unlikely(filp == ERR_PTR(-ESTALE)))
		filp = path_openat(&nd, op, flags | LOOKUP_REVAL);
	restore_nameidata();
	return filp;
}

struct file *do_file_open_root(const struct path *root,
		const char *name, const struct open_flags *op)
{
	struct nameidata nd;
	struct file *file;
	struct filename *filename;
	int flags = op->lookup_flags;

	if (d_is_symlink(root->dentry) && op->intent & LOOKUP_OPEN)
		return ERR_PTR(-ELOOP);

	filename = getname_kernel(name);
	if (IS_ERR(filename))
		return ERR_CAST(filename);

	set_nameidata(&nd, -1, filename, root);
	file = path_openat(&nd, op, flags | LOOKUP_RCU);
	if (unlikely(file == ERR_PTR(-ECHILD)))
		file = path_openat(&nd, op, flags);
	if (unlikely(file == ERR_PTR(-ESTALE)))
		file = path_openat(&nd, op, flags | LOOKUP_REVAL);
	restore_nameidata();
	putname(filename);
	return file;
}

static struct dentry *filename_create(int dfd, struct filename *name,
				      struct path *path, unsigned int lookup_flags)
{
	struct dentry *dentry = ERR_PTR(-EEXIST);
	struct qstr last;
	bool want_dir = lookup_flags & LOOKUP_DIRECTORY;
	unsigned int reval_flag = lookup_flags & LOOKUP_REVAL;
	unsigned int create_flags = LOOKUP_CREATE | LOOKUP_EXCL;
	int type;
	int err2;
	int error;

	error = filename_parentat(dfd, name, reval_flag, path, &last, &type);
	if (error)
		return ERR_PTR(error);

	 
	if (unlikely(type != LAST_NORM))
		goto out;

	 
	err2 = mnt_want_write(path->mnt);
	 
	if (last.name[last.len] && !want_dir)
		create_flags = 0;
	inode_lock_nested(path->dentry->d_inode, I_MUTEX_PARENT);
	dentry = lookup_one_qstr_excl(&last, path->dentry,
				      reval_flag | create_flags);
	if (IS_ERR(dentry))
		goto unlock;

	error = -EEXIST;
	if (d_is_positive(dentry))
		goto fail;

	 
	if (unlikely(!create_flags)) {
		error = -ENOENT;
		goto fail;
	}
	if (unlikely(err2)) {
		error = err2;
		goto fail;
	}
	return dentry;
fail:
	dput(dentry);
	dentry = ERR_PTR(error);
unlock:
	inode_unlock(path->dentry->d_inode);
	if (!err2)
		mnt_drop_write(path->mnt);
out:
	path_put(path);
	return dentry;
}

struct dentry *kern_path_create(int dfd, const char *pathname,
				struct path *path, unsigned int lookup_flags)
{
	struct filename *filename = getname_kernel(pathname);
	struct dentry *res = filename_create(dfd, filename, path, lookup_flags);

	putname(filename);
	return res;
}
EXPORT_SYMBOL(kern_path_create);

void done_path_create(struct path *path, struct dentry *dentry)
{
	dput(dentry);
	inode_unlock(path->dentry->d_inode);
	mnt_drop_write(path->mnt);
	path_put(path);
}
EXPORT_SYMBOL(done_path_create);

inline struct dentry *user_path_create(int dfd, const char __user *pathname,
				struct path *path, unsigned int lookup_flags)
{
	struct filename *filename = getname(pathname);
	struct dentry *res = filename_create(dfd, filename, path, lookup_flags);

	putname(filename);
	return res;
}
EXPORT_SYMBOL(user_path_create);

 
int vfs_mknod(struct mnt_idmap *idmap, struct inode *dir,
	      struct dentry *dentry, umode_t mode, dev_t dev)
{
	bool is_whiteout = S_ISCHR(mode) && dev == WHITEOUT_DEV;
	int error = may_create(idmap, dir, dentry);

	if (error)
		return error;

	if ((S_ISCHR(mode) || S_ISBLK(mode)) && !is_whiteout &&
	    !capable(CAP_MKNOD))
		return -EPERM;

	if (!dir->i_op->mknod)
		return -EPERM;

	mode = vfs_prepare_mode(idmap, dir, mode, mode, mode);
	error = devcgroup_inode_mknod(mode, dev);
	if (error)
		return error;

	error = security_inode_mknod(dir, dentry, mode, dev);
	if (error)
		return error;

	error = dir->i_op->mknod(idmap, dir, dentry, mode, dev);
	if (!error)
		fsnotify_create(dir, dentry);
	return error;
}
EXPORT_SYMBOL(vfs_mknod);

static int may_mknod(umode_t mode)
{
	switch (mode & S_IFMT) {
	case S_IFREG:
	case S_IFCHR:
	case S_IFBLK:
	case S_IFIFO:
	case S_IFSOCK:
	case 0:  
		return 0;
	case S_IFDIR:
		return -EPERM;
	default:
		return -EINVAL;
	}
}

static int do_mknodat(int dfd, struct filename *name, umode_t mode,
		unsigned int dev)
{
	struct mnt_idmap *idmap;
	struct dentry *dentry;
	struct path path;
	int error;
	unsigned int lookup_flags = 0;

	error = may_mknod(mode);
	if (error)
		goto out1;
retry:
	dentry = filename_create(dfd, name, &path, lookup_flags);
	error = PTR_ERR(dentry);
	if (IS_ERR(dentry))
		goto out1;

	error = security_path_mknod(&path, dentry,
			mode_strip_umask(path.dentry->d_inode, mode), dev);
	if (error)
		goto out2;

	idmap = mnt_idmap(path.mnt);
	switch (mode & S_IFMT) {
		case 0: case S_IFREG:
			error = vfs_create(idmap, path.dentry->d_inode,
					   dentry, mode, true);
			if (!error)
				ima_post_path_mknod(idmap, dentry);
			break;
		case S_IFCHR: case S_IFBLK:
			error = vfs_mknod(idmap, path.dentry->d_inode,
					  dentry, mode, new_decode_dev(dev));
			break;
		case S_IFIFO: case S_IFSOCK:
			error = vfs_mknod(idmap, path.dentry->d_inode,
					  dentry, mode, 0);
			break;
	}
out2:
	done_path_create(&path, dentry);
	if (retry_estale(error, lookup_flags)) {
		lookup_flags |= LOOKUP_REVAL;
		goto retry;
	}
out1:
	putname(name);
	return error;
}

SYSCALL_DEFINE4(mknodat, int, dfd, const char __user *, filename, umode_t, mode,
		unsigned int, dev)
{
	return do_mknodat(dfd, getname(filename), mode, dev);
}

SYSCALL_DEFINE3(mknod, const char __user *, filename, umode_t, mode, unsigned, dev)
{
	return do_mknodat(AT_FDCWD, getname(filename), mode, dev);
}

 
int vfs_mkdir(struct mnt_idmap *idmap, struct inode *dir,
	      struct dentry *dentry, umode_t mode)
{
	int error;
	unsigned max_links = dir->i_sb->s_max_links;

	error = may_create(idmap, dir, dentry);
	if (error)
		return error;

	if (!dir->i_op->mkdir)
		return -EPERM;

	mode = vfs_prepare_mode(idmap, dir, mode, S_IRWXUGO | S_ISVTX, 0);
	error = security_inode_mkdir(dir, dentry, mode);
	if (error)
		return error;

	if (max_links && dir->i_nlink >= max_links)
		return -EMLINK;

	error = dir->i_op->mkdir(idmap, dir, dentry, mode);
	if (!error)
		fsnotify_mkdir(dir, dentry);
	return error;
}
EXPORT_SYMBOL(vfs_mkdir);

int do_mkdirat(int dfd, struct filename *name, umode_t mode)
{
	struct dentry *dentry;
	struct path path;
	int error;
	unsigned int lookup_flags = LOOKUP_DIRECTORY;

retry:
	dentry = filename_create(dfd, name, &path, lookup_flags);
	error = PTR_ERR(dentry);
	if (IS_ERR(dentry))
		goto out_putname;

	error = security_path_mkdir(&path, dentry,
			mode_strip_umask(path.dentry->d_inode, mode));
	if (!error) {
		error = vfs_mkdir(mnt_idmap(path.mnt), path.dentry->d_inode,
				  dentry, mode);
	}
	done_path_create(&path, dentry);
	if (retry_estale(error, lookup_flags)) {
		lookup_flags |= LOOKUP_REVAL;
		goto retry;
	}
out_putname:
	putname(name);
	return error;
}

SYSCALL_DEFINE3(mkdirat, int, dfd, const char __user *, pathname, umode_t, mode)
{
	return do_mkdirat(dfd, getname(pathname), mode);
}

SYSCALL_DEFINE2(mkdir, const char __user *, pathname, umode_t, mode)
{
	return do_mkdirat(AT_FDCWD, getname(pathname), mode);
}

 
int vfs_rmdir(struct mnt_idmap *idmap, struct inode *dir,
		     struct dentry *dentry)
{
	int error = may_delete(idmap, dir, dentry, 1);

	if (error)
		return error;

	if (!dir->i_op->rmdir)
		return -EPERM;

	dget(dentry);
	inode_lock(dentry->d_inode);

	error = -EBUSY;
	if (is_local_mountpoint(dentry) ||
	    (dentry->d_inode->i_flags & S_KERNEL_FILE))
		goto out;

	error = security_inode_rmdir(dir, dentry);
	if (error)
		goto out;

	error = dir->i_op->rmdir(dir, dentry);
	if (error)
		goto out;

	shrink_dcache_parent(dentry);
	dentry->d_inode->i_flags |= S_DEAD;
	dont_mount(dentry);
	detach_mounts(dentry);

out:
	inode_unlock(dentry->d_inode);
	dput(dentry);
	if (!error)
		d_delete_notify(dir, dentry);
	return error;
}
EXPORT_SYMBOL(vfs_rmdir);

int do_rmdir(int dfd, struct filename *name)
{
	int error;
	struct dentry *dentry;
	struct path path;
	struct qstr last;
	int type;
	unsigned int lookup_flags = 0;
retry:
	error = filename_parentat(dfd, name, lookup_flags, &path, &last, &type);
	if (error)
		goto exit1;

	switch (type) {
	case LAST_DOTDOT:
		error = -ENOTEMPTY;
		goto exit2;
	case LAST_DOT:
		error = -EINVAL;
		goto exit2;
	case LAST_ROOT:
		error = -EBUSY;
		goto exit2;
	}

	error = mnt_want_write(path.mnt);
	if (error)
		goto exit2;

	inode_lock_nested(path.dentry->d_inode, I_MUTEX_PARENT);
	dentry = lookup_one_qstr_excl(&last, path.dentry, lookup_flags);
	error = PTR_ERR(dentry);
	if (IS_ERR(dentry))
		goto exit3;
	if (!dentry->d_inode) {
		error = -ENOENT;
		goto exit4;
	}
	error = security_path_rmdir(&path, dentry);
	if (error)
		goto exit4;
	error = vfs_rmdir(mnt_idmap(path.mnt), path.dentry->d_inode, dentry);
exit4:
	dput(dentry);
exit3:
	inode_unlock(path.dentry->d_inode);
	mnt_drop_write(path.mnt);
exit2:
	path_put(&path);
	if (retry_estale(error, lookup_flags)) {
		lookup_flags |= LOOKUP_REVAL;
		goto retry;
	}
exit1:
	putname(name);
	return error;
}

SYSCALL_DEFINE1(rmdir, const char __user *, pathname)
{
	return do_rmdir(AT_FDCWD, getname(pathname));
}

 
int vfs_unlink(struct mnt_idmap *idmap, struct inode *dir,
	       struct dentry *dentry, struct inode **delegated_inode)
{
	struct inode *target = dentry->d_inode;
	int error = may_delete(idmap, dir, dentry, 0);

	if (error)
		return error;

	if (!dir->i_op->unlink)
		return -EPERM;

	inode_lock(target);
	if (IS_SWAPFILE(target))
		error = -EPERM;
	else if (is_local_mountpoint(dentry))
		error = -EBUSY;
	else {
		error = security_inode_unlink(dir, dentry);
		if (!error) {
			error = try_break_deleg(target, delegated_inode);
			if (error)
				goto out;
			error = dir->i_op->unlink(dir, dentry);
			if (!error) {
				dont_mount(dentry);
				detach_mounts(dentry);
			}
		}
	}
out:
	inode_unlock(target);

	 
	if (!error && dentry->d_flags & DCACHE_NFSFS_RENAMED) {
		fsnotify_unlink(dir, dentry);
	} else if (!error) {
		fsnotify_link_count(target);
		d_delete_notify(dir, dentry);
	}

	return error;
}
EXPORT_SYMBOL(vfs_unlink);

 
int do_unlinkat(int dfd, struct filename *name)
{
	int error;
	struct dentry *dentry;
	struct path path;
	struct qstr last;
	int type;
	struct inode *inode = NULL;
	struct inode *delegated_inode = NULL;
	unsigned int lookup_flags = 0;
retry:
	error = filename_parentat(dfd, name, lookup_flags, &path, &last, &type);
	if (error)
		goto exit1;

	error = -EISDIR;
	if (type != LAST_NORM)
		goto exit2;

	error = mnt_want_write(path.mnt);
	if (error)
		goto exit2;
retry_deleg:
	inode_lock_nested(path.dentry->d_inode, I_MUTEX_PARENT);
	dentry = lookup_one_qstr_excl(&last, path.dentry, lookup_flags);
	error = PTR_ERR(dentry);
	if (!IS_ERR(dentry)) {

		 
		if (last.name[last.len])
			goto slashes;
		inode = dentry->d_inode;
		if (d_is_negative(dentry))
			goto slashes;
		ihold(inode);
		error = security_path_unlink(&path, dentry);
		if (error)
			goto exit3;
		error = vfs_unlink(mnt_idmap(path.mnt), path.dentry->d_inode,
				   dentry, &delegated_inode);
exit3:
		dput(dentry);
	}
	inode_unlock(path.dentry->d_inode);
	if (inode)
		iput(inode);	 
	inode = NULL;
	if (delegated_inode) {
		error = break_deleg_wait(&delegated_inode);
		if (!error)
			goto retry_deleg;
	}
	mnt_drop_write(path.mnt);
exit2:
	path_put(&path);
	if (retry_estale(error, lookup_flags)) {
		lookup_flags |= LOOKUP_REVAL;
		inode = NULL;
		goto retry;
	}
exit1:
	putname(name);
	return error;

slashes:
	if (d_is_negative(dentry))
		error = -ENOENT;
	else if (d_is_dir(dentry))
		error = -EISDIR;
	else
		error = -ENOTDIR;
	goto exit3;
}

SYSCALL_DEFINE3(unlinkat, int, dfd, const char __user *, pathname, int, flag)
{
	if ((flag & ~AT_REMOVEDIR) != 0)
		return -EINVAL;

	if (flag & AT_REMOVEDIR)
		return do_rmdir(dfd, getname(pathname));
	return do_unlinkat(dfd, getname(pathname));
}

SYSCALL_DEFINE1(unlink, const char __user *, pathname)
{
	return do_unlinkat(AT_FDCWD, getname(pathname));
}

 
int vfs_symlink(struct mnt_idmap *idmap, struct inode *dir,
		struct dentry *dentry, const char *oldname)
{
	int error;

	error = may_create(idmap, dir, dentry);
	if (error)
		return error;

	if (!dir->i_op->symlink)
		return -EPERM;

	error = security_inode_symlink(dir, dentry, oldname);
	if (error)
		return error;

	error = dir->i_op->symlink(idmap, dir, dentry, oldname);
	if (!error)
		fsnotify_create(dir, dentry);
	return error;
}
EXPORT_SYMBOL(vfs_symlink);

int do_symlinkat(struct filename *from, int newdfd, struct filename *to)
{
	int error;
	struct dentry *dentry;
	struct path path;
	unsigned int lookup_flags = 0;

	if (IS_ERR(from)) {
		error = PTR_ERR(from);
		goto out_putnames;
	}
retry:
	dentry = filename_create(newdfd, to, &path, lookup_flags);
	error = PTR_ERR(dentry);
	if (IS_ERR(dentry))
		goto out_putnames;

	error = security_path_symlink(&path, dentry, from->name);
	if (!error)
		error = vfs_symlink(mnt_idmap(path.mnt), path.dentry->d_inode,
				    dentry, from->name);
	done_path_create(&path, dentry);
	if (retry_estale(error, lookup_flags)) {
		lookup_flags |= LOOKUP_REVAL;
		goto retry;
	}
out_putnames:
	putname(to);
	putname(from);
	return error;
}

SYSCALL_DEFINE3(symlinkat, const char __user *, oldname,
		int, newdfd, const char __user *, newname)
{
	return do_symlinkat(getname(oldname), newdfd, getname(newname));
}

SYSCALL_DEFINE2(symlink, const char __user *, oldname, const char __user *, newname)
{
	return do_symlinkat(getname(oldname), AT_FDCWD, getname(newname));
}

 
int vfs_link(struct dentry *old_dentry, struct mnt_idmap *idmap,
	     struct inode *dir, struct dentry *new_dentry,
	     struct inode **delegated_inode)
{
	struct inode *inode = old_dentry->d_inode;
	unsigned max_links = dir->i_sb->s_max_links;
	int error;

	if (!inode)
		return -ENOENT;

	error = may_create(idmap, dir, new_dentry);
	if (error)
		return error;

	if (dir->i_sb != inode->i_sb)
		return -EXDEV;

	 
	if (IS_APPEND(inode) || IS_IMMUTABLE(inode))
		return -EPERM;
	 
	if (HAS_UNMAPPED_ID(idmap, inode))
		return -EPERM;
	if (!dir->i_op->link)
		return -EPERM;
	if (S_ISDIR(inode->i_mode))
		return -EPERM;

	error = security_inode_link(old_dentry, dir, new_dentry);
	if (error)
		return error;

	inode_lock(inode);
	 
	if (inode->i_nlink == 0 && !(inode->i_state & I_LINKABLE))
		error =  -ENOENT;
	else if (max_links && inode->i_nlink >= max_links)
		error = -EMLINK;
	else {
		error = try_break_deleg(inode, delegated_inode);
		if (!error)
			error = dir->i_op->link(old_dentry, dir, new_dentry);
	}

	if (!error && (inode->i_state & I_LINKABLE)) {
		spin_lock(&inode->i_lock);
		inode->i_state &= ~I_LINKABLE;
		spin_unlock(&inode->i_lock);
	}
	inode_unlock(inode);
	if (!error)
		fsnotify_link(dir, inode, new_dentry);
	return error;
}
EXPORT_SYMBOL(vfs_link);

 
int do_linkat(int olddfd, struct filename *old, int newdfd,
	      struct filename *new, int flags)
{
	struct mnt_idmap *idmap;
	struct dentry *new_dentry;
	struct path old_path, new_path;
	struct inode *delegated_inode = NULL;
	int how = 0;
	int error;

	if ((flags & ~(AT_SYMLINK_FOLLOW | AT_EMPTY_PATH)) != 0) {
		error = -EINVAL;
		goto out_putnames;
	}
	 
	if (flags & AT_EMPTY_PATH && !capable(CAP_DAC_READ_SEARCH)) {
		error = -ENOENT;
		goto out_putnames;
	}

	if (flags & AT_SYMLINK_FOLLOW)
		how |= LOOKUP_FOLLOW;
retry:
	error = filename_lookup(olddfd, old, how, &old_path, NULL);
	if (error)
		goto out_putnames;

	new_dentry = filename_create(newdfd, new, &new_path,
					(how & LOOKUP_REVAL));
	error = PTR_ERR(new_dentry);
	if (IS_ERR(new_dentry))
		goto out_putpath;

	error = -EXDEV;
	if (old_path.mnt != new_path.mnt)
		goto out_dput;
	idmap = mnt_idmap(new_path.mnt);
	error = may_linkat(idmap, &old_path);
	if (unlikely(error))
		goto out_dput;
	error = security_path_link(old_path.dentry, &new_path, new_dentry);
	if (error)
		goto out_dput;
	error = vfs_link(old_path.dentry, idmap, new_path.dentry->d_inode,
			 new_dentry, &delegated_inode);
out_dput:
	done_path_create(&new_path, new_dentry);
	if (delegated_inode) {
		error = break_deleg_wait(&delegated_inode);
		if (!error) {
			path_put(&old_path);
			goto retry;
		}
	}
	if (retry_estale(error, how)) {
		path_put(&old_path);
		how |= LOOKUP_REVAL;
		goto retry;
	}
out_putpath:
	path_put(&old_path);
out_putnames:
	putname(old);
	putname(new);

	return error;
}

SYSCALL_DEFINE5(linkat, int, olddfd, const char __user *, oldname,
		int, newdfd, const char __user *, newname, int, flags)
{
	return do_linkat(olddfd, getname_uflags(oldname, flags),
		newdfd, getname(newname), flags);
}

SYSCALL_DEFINE2(link, const char __user *, oldname, const char __user *, newname)
{
	return do_linkat(AT_FDCWD, getname(oldname), AT_FDCWD, getname(newname), 0);
}

 
int vfs_rename(struct renamedata *rd)
{
	int error;
	struct inode *old_dir = rd->old_dir, *new_dir = rd->new_dir;
	struct dentry *old_dentry = rd->old_dentry;
	struct dentry *new_dentry = rd->new_dentry;
	struct inode **delegated_inode = rd->delegated_inode;
	unsigned int flags = rd->flags;
	bool is_dir = d_is_dir(old_dentry);
	struct inode *source = old_dentry->d_inode;
	struct inode *target = new_dentry->d_inode;
	bool new_is_dir = false;
	unsigned max_links = new_dir->i_sb->s_max_links;
	struct name_snapshot old_name;

	if (source == target)
		return 0;

	error = may_delete(rd->old_mnt_idmap, old_dir, old_dentry, is_dir);
	if (error)
		return error;

	if (!target) {
		error = may_create(rd->new_mnt_idmap, new_dir, new_dentry);
	} else {
		new_is_dir = d_is_dir(new_dentry);

		if (!(flags & RENAME_EXCHANGE))
			error = may_delete(rd->new_mnt_idmap, new_dir,
					   new_dentry, is_dir);
		else
			error = may_delete(rd->new_mnt_idmap, new_dir,
					   new_dentry, new_is_dir);
	}
	if (error)
		return error;

	if (!old_dir->i_op->rename)
		return -EPERM;

	 
	if (new_dir != old_dir) {
		if (is_dir) {
			error = inode_permission(rd->old_mnt_idmap, source,
						 MAY_WRITE);
			if (error)
				return error;
		}
		if ((flags & RENAME_EXCHANGE) && new_is_dir) {
			error = inode_permission(rd->new_mnt_idmap, target,
						 MAY_WRITE);
			if (error)
				return error;
		}
	}

	error = security_inode_rename(old_dir, old_dentry, new_dir, new_dentry,
				      flags);
	if (error)
		return error;

	take_dentry_name_snapshot(&old_name, old_dentry);
	dget(new_dentry);
	 
	lock_two_inodes(source, target, I_MUTEX_NORMAL, I_MUTEX_NONDIR2);

	error = -EPERM;
	if (IS_SWAPFILE(source) || (target && IS_SWAPFILE(target)))
		goto out;

	error = -EBUSY;
	if (is_local_mountpoint(old_dentry) || is_local_mountpoint(new_dentry))
		goto out;

	if (max_links && new_dir != old_dir) {
		error = -EMLINK;
		if (is_dir && !new_is_dir && new_dir->i_nlink >= max_links)
			goto out;
		if ((flags & RENAME_EXCHANGE) && !is_dir && new_is_dir &&
		    old_dir->i_nlink >= max_links)
			goto out;
	}
	if (!is_dir) {
		error = try_break_deleg(source, delegated_inode);
		if (error)
			goto out;
	}
	if (target && !new_is_dir) {
		error = try_break_deleg(target, delegated_inode);
		if (error)
			goto out;
	}
	error = old_dir->i_op->rename(rd->new_mnt_idmap, old_dir, old_dentry,
				      new_dir, new_dentry, flags);
	if (error)
		goto out;

	if (!(flags & RENAME_EXCHANGE) && target) {
		if (is_dir) {
			shrink_dcache_parent(new_dentry);
			target->i_flags |= S_DEAD;
		}
		dont_mount(new_dentry);
		detach_mounts(new_dentry);
	}
	if (!(old_dir->i_sb->s_type->fs_flags & FS_RENAME_DOES_D_MOVE)) {
		if (!(flags & RENAME_EXCHANGE))
			d_move(old_dentry, new_dentry);
		else
			d_exchange(old_dentry, new_dentry);
	}
out:
	inode_unlock(source);
	if (target)
		inode_unlock(target);
	dput(new_dentry);
	if (!error) {
		fsnotify_move(old_dir, new_dir, &old_name.name, is_dir,
			      !(flags & RENAME_EXCHANGE) ? target : NULL, old_dentry);
		if (flags & RENAME_EXCHANGE) {
			fsnotify_move(new_dir, old_dir, &old_dentry->d_name,
				      new_is_dir, NULL, new_dentry);
		}
	}
	release_dentry_name_snapshot(&old_name);

	return error;
}
EXPORT_SYMBOL(vfs_rename);

int do_renameat2(int olddfd, struct filename *from, int newdfd,
		 struct filename *to, unsigned int flags)
{
	struct renamedata rd;
	struct dentry *old_dentry, *new_dentry;
	struct dentry *trap;
	struct path old_path, new_path;
	struct qstr old_last, new_last;
	int old_type, new_type;
	struct inode *delegated_inode = NULL;
	unsigned int lookup_flags = 0, target_flags = LOOKUP_RENAME_TARGET;
	bool should_retry = false;
	int error = -EINVAL;

	if (flags & ~(RENAME_NOREPLACE | RENAME_EXCHANGE | RENAME_WHITEOUT))
		goto put_names;

	if ((flags & (RENAME_NOREPLACE | RENAME_WHITEOUT)) &&
	    (flags & RENAME_EXCHANGE))
		goto put_names;

	if (flags & RENAME_EXCHANGE)
		target_flags = 0;

retry:
	error = filename_parentat(olddfd, from, lookup_flags, &old_path,
				  &old_last, &old_type);
	if (error)
		goto put_names;

	error = filename_parentat(newdfd, to, lookup_flags, &new_path, &new_last,
				  &new_type);
	if (error)
		goto exit1;

	error = -EXDEV;
	if (old_path.mnt != new_path.mnt)
		goto exit2;

	error = -EBUSY;
	if (old_type != LAST_NORM)
		goto exit2;

	if (flags & RENAME_NOREPLACE)
		error = -EEXIST;
	if (new_type != LAST_NORM)
		goto exit2;

	error = mnt_want_write(old_path.mnt);
	if (error)
		goto exit2;

retry_deleg:
	trap = lock_rename(new_path.dentry, old_path.dentry);

	old_dentry = lookup_one_qstr_excl(&old_last, old_path.dentry,
					  lookup_flags);
	error = PTR_ERR(old_dentry);
	if (IS_ERR(old_dentry))
		goto exit3;
	 
	error = -ENOENT;
	if (d_is_negative(old_dentry))
		goto exit4;
	new_dentry = lookup_one_qstr_excl(&new_last, new_path.dentry,
					  lookup_flags | target_flags);
	error = PTR_ERR(new_dentry);
	if (IS_ERR(new_dentry))
		goto exit4;
	error = -EEXIST;
	if ((flags & RENAME_NOREPLACE) && d_is_positive(new_dentry))
		goto exit5;
	if (flags & RENAME_EXCHANGE) {
		error = -ENOENT;
		if (d_is_negative(new_dentry))
			goto exit5;

		if (!d_is_dir(new_dentry)) {
			error = -ENOTDIR;
			if (new_last.name[new_last.len])
				goto exit5;
		}
	}
	 
	if (!d_is_dir(old_dentry)) {
		error = -ENOTDIR;
		if (old_last.name[old_last.len])
			goto exit5;
		if (!(flags & RENAME_EXCHANGE) && new_last.name[new_last.len])
			goto exit5;
	}
	 
	error = -EINVAL;
	if (old_dentry == trap)
		goto exit5;
	 
	if (!(flags & RENAME_EXCHANGE))
		error = -ENOTEMPTY;
	if (new_dentry == trap)
		goto exit5;

	error = security_path_rename(&old_path, old_dentry,
				     &new_path, new_dentry, flags);
	if (error)
		goto exit5;

	rd.old_dir	   = old_path.dentry->d_inode;
	rd.old_dentry	   = old_dentry;
	rd.old_mnt_idmap   = mnt_idmap(old_path.mnt);
	rd.new_dir	   = new_path.dentry->d_inode;
	rd.new_dentry	   = new_dentry;
	rd.new_mnt_idmap   = mnt_idmap(new_path.mnt);
	rd.delegated_inode = &delegated_inode;
	rd.flags	   = flags;
	error = vfs_rename(&rd);
exit5:
	dput(new_dentry);
exit4:
	dput(old_dentry);
exit3:
	unlock_rename(new_path.dentry, old_path.dentry);
	if (delegated_inode) {
		error = break_deleg_wait(&delegated_inode);
		if (!error)
			goto retry_deleg;
	}
	mnt_drop_write(old_path.mnt);
exit2:
	if (retry_estale(error, lookup_flags))
		should_retry = true;
	path_put(&new_path);
exit1:
	path_put(&old_path);
	if (should_retry) {
		should_retry = false;
		lookup_flags |= LOOKUP_REVAL;
		goto retry;
	}
put_names:
	putname(from);
	putname(to);
	return error;
}

SYSCALL_DEFINE5(renameat2, int, olddfd, const char __user *, oldname,
		int, newdfd, const char __user *, newname, unsigned int, flags)
{
	return do_renameat2(olddfd, getname(oldname), newdfd, getname(newname),
				flags);
}

SYSCALL_DEFINE4(renameat, int, olddfd, const char __user *, oldname,
		int, newdfd, const char __user *, newname)
{
	return do_renameat2(olddfd, getname(oldname), newdfd, getname(newname),
				0);
}

SYSCALL_DEFINE2(rename, const char __user *, oldname, const char __user *, newname)
{
	return do_renameat2(AT_FDCWD, getname(oldname), AT_FDCWD,
				getname(newname), 0);
}

int readlink_copy(char __user *buffer, int buflen, const char *link)
{
	int len = PTR_ERR(link);
	if (IS_ERR(link))
		goto out;

	len = strlen(link);
	if (len > (unsigned) buflen)
		len = buflen;
	if (copy_to_user(buffer, link, len))
		len = -EFAULT;
out:
	return len;
}

 
int vfs_readlink(struct dentry *dentry, char __user *buffer, int buflen)
{
	struct inode *inode = d_inode(dentry);
	DEFINE_DELAYED_CALL(done);
	const char *link;
	int res;

	if (unlikely(!(inode->i_opflags & IOP_DEFAULT_READLINK))) {
		if (unlikely(inode->i_op->readlink))
			return inode->i_op->readlink(dentry, buffer, buflen);

		if (!d_is_symlink(dentry))
			return -EINVAL;

		spin_lock(&inode->i_lock);
		inode->i_opflags |= IOP_DEFAULT_READLINK;
		spin_unlock(&inode->i_lock);
	}

	link = READ_ONCE(inode->i_link);
	if (!link) {
		link = inode->i_op->get_link(dentry, inode, &done);
		if (IS_ERR(link))
			return PTR_ERR(link);
	}
	res = readlink_copy(buffer, buflen, link);
	do_delayed_call(&done);
	return res;
}
EXPORT_SYMBOL(vfs_readlink);

 
const char *vfs_get_link(struct dentry *dentry, struct delayed_call *done)
{
	const char *res = ERR_PTR(-EINVAL);
	struct inode *inode = d_inode(dentry);

	if (d_is_symlink(dentry)) {
		res = ERR_PTR(security_inode_readlink(dentry));
		if (!res)
			res = inode->i_op->get_link(dentry, inode, done);
	}
	return res;
}
EXPORT_SYMBOL(vfs_get_link);

 
const char *page_get_link(struct dentry *dentry, struct inode *inode,
			  struct delayed_call *callback)
{
	char *kaddr;
	struct page *page;
	struct address_space *mapping = inode->i_mapping;

	if (!dentry) {
		page = find_get_page(mapping, 0);
		if (!page)
			return ERR_PTR(-ECHILD);
		if (!PageUptodate(page)) {
			put_page(page);
			return ERR_PTR(-ECHILD);
		}
	} else {
		page = read_mapping_page(mapping, 0, NULL);
		if (IS_ERR(page))
			return (char*)page;
	}
	set_delayed_call(callback, page_put_link, page);
	BUG_ON(mapping_gfp_mask(mapping) & __GFP_HIGHMEM);
	kaddr = page_address(page);
	nd_terminate_link(kaddr, inode->i_size, PAGE_SIZE - 1);
	return kaddr;
}

EXPORT_SYMBOL(page_get_link);

void page_put_link(void *arg)
{
	put_page(arg);
}
EXPORT_SYMBOL(page_put_link);

int page_readlink(struct dentry *dentry, char __user *buffer, int buflen)
{
	DEFINE_DELAYED_CALL(done);
	int res = readlink_copy(buffer, buflen,
				page_get_link(dentry, d_inode(dentry),
					      &done));
	do_delayed_call(&done);
	return res;
}
EXPORT_SYMBOL(page_readlink);

int page_symlink(struct inode *inode, const char *symname, int len)
{
	struct address_space *mapping = inode->i_mapping;
	const struct address_space_operations *aops = mapping->a_ops;
	bool nofs = !mapping_gfp_constraint(mapping, __GFP_FS);
	struct page *page;
	void *fsdata = NULL;
	int err;
	unsigned int flags;

retry:
	if (nofs)
		flags = memalloc_nofs_save();
	err = aops->write_begin(NULL, mapping, 0, len-1, &page, &fsdata);
	if (nofs)
		memalloc_nofs_restore(flags);
	if (err)
		goto fail;

	memcpy(page_address(page), symname, len-1);

	err = aops->write_end(NULL, mapping, 0, len-1, len-1,
							page, fsdata);
	if (err < 0)
		goto fail;
	if (err < len-1)
		goto retry;

	mark_inode_dirty(inode);
	return 0;
fail:
	return err;
}
EXPORT_SYMBOL(page_symlink);

const struct inode_operations page_symlink_inode_operations = {
	.get_link	= page_get_link,
};
EXPORT_SYMBOL(page_symlink_inode_operations);
