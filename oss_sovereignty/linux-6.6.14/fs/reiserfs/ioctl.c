 

#include <linux/capability.h>
#include <linux/fs.h>
#include <linux/mount.h>
#include "reiserfs.h"
#include <linux/time.h>
#include <linux/uaccess.h>
#include <linux/pagemap.h>
#include <linux/compat.h>
#include <linux/fileattr.h>

int reiserfs_fileattr_get(struct dentry *dentry, struct fileattr *fa)
{
	struct inode *inode = d_inode(dentry);

	if (!reiserfs_attrs(inode->i_sb))
		return -ENOTTY;

	fileattr_fill_flags(fa, REISERFS_I(inode)->i_attrs);

	return 0;
}

int reiserfs_fileattr_set(struct mnt_idmap *idmap,
			  struct dentry *dentry, struct fileattr *fa)
{
	struct inode *inode = d_inode(dentry);
	unsigned int flags = fa->flags;
	int err;

	reiserfs_write_lock(inode->i_sb);

	err = -ENOTTY;
	if (!reiserfs_attrs(inode->i_sb))
		goto unlock;

	err = -EOPNOTSUPP;
	if (fileattr_has_fsx(fa))
		goto unlock;

	 
	err = -EPERM;
	if (IS_NOQUOTA(inode))
		goto unlock;

	if ((flags & REISERFS_NOTAIL_FL) && S_ISREG(inode->i_mode)) {
		err = reiserfs_unpack(inode);
		if (err)
			goto unlock;
	}
	sd_attrs_to_i_attrs(flags, inode);
	REISERFS_I(inode)->i_attrs = flags;
	inode_set_ctime_current(inode);
	mark_inode_dirty(inode);
	err = 0;
unlock:
	reiserfs_write_unlock(inode->i_sb);

	return err;
}

 
long reiserfs_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
	struct inode *inode = file_inode(filp);
	int err = 0;

	reiserfs_write_lock(inode->i_sb);

	switch (cmd) {
	case REISERFS_IOC_UNPACK:
		if (S_ISREG(inode->i_mode)) {
			if (arg)
				err = reiserfs_unpack(inode);
		} else
			err = -ENOTTY;
		break;
		 
	case REISERFS_IOC_GETVERSION:
		err = put_user(inode->i_generation, (int __user *)arg);
		break;
	case REISERFS_IOC_SETVERSION:
		if (!inode_owner_or_capable(&nop_mnt_idmap, inode)) {
			err = -EPERM;
			break;
		}
		err = mnt_want_write_file(filp);
		if (err)
			break;
		if (get_user(inode->i_generation, (int __user *)arg)) {
			err = -EFAULT;
			goto setversion_out;
		}
		inode_set_ctime_current(inode);
		mark_inode_dirty(inode);
setversion_out:
		mnt_drop_write_file(filp);
		break;
	default:
		err = -ENOTTY;
	}

	reiserfs_write_unlock(inode->i_sb);

	return err;
}

#ifdef CONFIG_COMPAT
long reiserfs_compat_ioctl(struct file *file, unsigned int cmd,
				unsigned long arg)
{
	 
	switch (cmd) {
	case REISERFS_IOC32_UNPACK:
		cmd = REISERFS_IOC_UNPACK;
		break;
	case REISERFS_IOC32_GETVERSION:
		cmd = REISERFS_IOC_GETVERSION;
		break;
	case REISERFS_IOC32_SETVERSION:
		cmd = REISERFS_IOC_SETVERSION;
		break;
	default:
		return -ENOIOCTLCMD;
	}

	return reiserfs_ioctl(file, cmd, (unsigned long) compat_ptr(arg));
}
#endif

int reiserfs_commit_write(struct file *f, struct page *page,
			  unsigned from, unsigned to);
 
int reiserfs_unpack(struct inode *inode)
{
	int retval = 0;
	int index;
	struct page *page;
	struct address_space *mapping;
	unsigned long write_from;
	unsigned long blocksize = inode->i_sb->s_blocksize;

	if (inode->i_size == 0) {
		REISERFS_I(inode)->i_flags |= i_nopack_mask;
		return 0;
	}
	 
	if (REISERFS_I(inode)->i_flags & i_nopack_mask) {
		return 0;
	}

	 
	{
		int depth = reiserfs_write_unlock_nested(inode->i_sb);

		inode_lock(inode);
		reiserfs_write_lock_nested(inode->i_sb, depth);
	}

	reiserfs_write_lock(inode->i_sb);

	write_from = inode->i_size & (blocksize - 1);
	 
	if (write_from == 0) {
		REISERFS_I(inode)->i_flags |= i_nopack_mask;
		goto out;
	}

	 
	index = inode->i_size >> PAGE_SHIFT;
	mapping = inode->i_mapping;
	page = grab_cache_page(mapping, index);
	retval = -ENOMEM;
	if (!page) {
		goto out;
	}
	retval = __reiserfs_write_begin(page, write_from, 0);
	if (retval)
		goto out_unlock;

	 
	flush_dcache_page(page);
	retval = reiserfs_commit_write(NULL, page, write_from, write_from);
	REISERFS_I(inode)->i_flags |= i_nopack_mask;

out_unlock:
	unlock_page(page);
	put_page(page);

out:
	inode_unlock(inode);
	reiserfs_write_unlock(inode->i_sb);
	return retval;
}
