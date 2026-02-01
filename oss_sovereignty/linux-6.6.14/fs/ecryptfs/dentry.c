
 

#include <linux/dcache.h>
#include <linux/namei.h>
#include <linux/mount.h>
#include <linux/fs_stack.h>
#include <linux/slab.h>
#include "ecryptfs_kernel.h"

 
static int ecryptfs_d_revalidate(struct dentry *dentry, unsigned int flags)
{
	struct dentry *lower_dentry = ecryptfs_dentry_to_lower(dentry);
	int rc = 1;

	if (flags & LOOKUP_RCU)
		return -ECHILD;

	if (lower_dentry->d_flags & DCACHE_OP_REVALIDATE)
		rc = lower_dentry->d_op->d_revalidate(lower_dentry, flags);

	if (d_really_is_positive(dentry)) {
		struct inode *inode = d_inode(dentry);

		fsstack_copy_attr_all(inode, ecryptfs_inode_to_lower(inode));
		if (!inode->i_nlink)
			return 0;
	}
	return rc;
}

struct kmem_cache *ecryptfs_dentry_info_cache;

static void ecryptfs_dentry_free_rcu(struct rcu_head *head)
{
	kmem_cache_free(ecryptfs_dentry_info_cache,
		container_of(head, struct ecryptfs_dentry_info, rcu));
}

 
static void ecryptfs_d_release(struct dentry *dentry)
{
	struct ecryptfs_dentry_info *p = dentry->d_fsdata;
	if (p) {
		path_put(&p->lower_path);
		call_rcu(&p->rcu, ecryptfs_dentry_free_rcu);
	}
}

const struct dentry_operations ecryptfs_dops = {
	.d_revalidate = ecryptfs_d_revalidate,
	.d_release = ecryptfs_d_release,
};
