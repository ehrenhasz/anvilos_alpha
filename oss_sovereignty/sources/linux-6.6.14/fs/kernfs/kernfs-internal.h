


#ifndef __KERNFS_INTERNAL_H
#define __KERNFS_INTERNAL_H

#include <linux/lockdep.h>
#include <linux/fs.h>
#include <linux/mutex.h>
#include <linux/rwsem.h>
#include <linux/xattr.h>

#include <linux/kernfs.h>
#include <linux/fs_context.h>

struct kernfs_iattrs {
	kuid_t			ia_uid;
	kgid_t			ia_gid;
	struct timespec64	ia_atime;
	struct timespec64	ia_mtime;
	struct timespec64	ia_ctime;

	struct simple_xattrs	xattrs;
	atomic_t		nr_user_xattrs;
	atomic_t		user_xattr_size;
};

struct kernfs_root {
	
	struct kernfs_node	*kn;
	unsigned int		flags;	

	
	struct idr		ino_idr;
	u32			last_id_lowbits;
	u32			id_highbits;
	struct kernfs_syscall_ops *syscall_ops;

	
	struct list_head	supers;

	wait_queue_head_t	deactivate_waitq;
	struct rw_semaphore	kernfs_rwsem;
	struct rw_semaphore	kernfs_iattr_rwsem;
	struct rw_semaphore	kernfs_supers_rwsem;
};


#define KN_DEACTIVATED_BIAS		(INT_MIN + 1)




static inline struct kernfs_root *kernfs_root(struct kernfs_node *kn)
{
	
	if (kn->parent)
		kn = kn->parent;
	return kn->dir.root;
}


struct kernfs_super_info {
	struct super_block	*sb;

	
	struct kernfs_root	*root;

	
	const void		*ns;

	
	struct list_head	node;
};
#define kernfs_info(SB) ((struct kernfs_super_info *)(SB->s_fs_info))

static inline struct kernfs_node *kernfs_dentry_node(struct dentry *dentry)
{
	if (d_really_is_negative(dentry))
		return NULL;
	return d_inode(dentry)->i_private;
}

static inline void kernfs_set_rev(struct kernfs_node *parent,
				  struct dentry *dentry)
{
	dentry->d_time = parent->dir.rev;
}

static inline void kernfs_inc_rev(struct kernfs_node *parent)
{
	parent->dir.rev++;
}

static inline bool kernfs_dir_changed(struct kernfs_node *parent,
				      struct dentry *dentry)
{
	if (parent->dir.rev != dentry->d_time)
		return true;
	return false;
}

extern const struct super_operations kernfs_sops;
extern struct kmem_cache *kernfs_node_cache, *kernfs_iattrs_cache;


extern const struct xattr_handler *kernfs_xattr_handlers[];
void kernfs_evict_inode(struct inode *inode);
int kernfs_iop_permission(struct mnt_idmap *idmap,
			  struct inode *inode, int mask);
int kernfs_iop_setattr(struct mnt_idmap *idmap, struct dentry *dentry,
		       struct iattr *iattr);
int kernfs_iop_getattr(struct mnt_idmap *idmap,
		       const struct path *path, struct kstat *stat,
		       u32 request_mask, unsigned int query_flags);
ssize_t kernfs_iop_listxattr(struct dentry *dentry, char *buf, size_t size);
int __kernfs_setattr(struct kernfs_node *kn, const struct iattr *iattr);


extern const struct dentry_operations kernfs_dops;
extern const struct file_operations kernfs_dir_fops;
extern const struct inode_operations kernfs_dir_iops;

struct kernfs_node *kernfs_get_active(struct kernfs_node *kn);
void kernfs_put_active(struct kernfs_node *kn);
int kernfs_add_one(struct kernfs_node *kn);
struct kernfs_node *kernfs_new_node(struct kernfs_node *parent,
				    const char *name, umode_t mode,
				    kuid_t uid, kgid_t gid,
				    unsigned flags);


extern const struct file_operations kernfs_file_fops;

bool kernfs_should_drain_open_files(struct kernfs_node *kn);
void kernfs_drain_open_files(struct kernfs_node *kn);


extern const struct inode_operations kernfs_symlink_iops;


extern struct kernfs_global_locks *kernfs_locks;
#endif	
