 

#include <linux/namei.h>
#include "hfs_fs.h"

 

static int hfs_revalidate_dentry(struct dentry *dentry, unsigned int flags)
{
	struct inode *inode;
	int diff;

	if (flags & LOOKUP_RCU)
		return -ECHILD;

	inode = d_inode(dentry);
	if(!inode)
		return 1;

	 
	diff = sys_tz.tz_minuteswest * 60 - HFS_I(inode)->tz_secondswest;
	if (diff) {
		struct timespec64 ctime = inode_get_ctime(inode);

		inode_set_ctime(inode, ctime.tv_sec + diff, ctime.tv_nsec);
		inode->i_atime.tv_sec += diff;
		inode->i_mtime.tv_sec += diff;
		HFS_I(inode)->tz_secondswest += diff;
	}
	return 1;
}

const struct dentry_operations hfs_dentry_operations =
{
	.d_revalidate	= hfs_revalidate_dentry,
	.d_hash		= hfs_hash_dentry,
	.d_compare	= hfs_compare_dentry,
};

