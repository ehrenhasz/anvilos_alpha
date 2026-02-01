
 

#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/time.h>
#include <linux/fs.h>
#include <linux/stat.h>
#include <linux/errno.h>
#include <linux/string.h>
#include <linux/namei.h>
#include <linux/module.h>
#include <linux/uaccess.h>

#include <linux/coda.h>
#include "coda_psdev.h"
#include "coda_linux.h"

 
static int coda_ioctl_permission(struct mnt_idmap *idmap,
				 struct inode *inode, int mask);
static long coda_pioctl(struct file *filp, unsigned int cmd,
			unsigned long user_data);

 
const struct inode_operations coda_ioctl_inode_operations = {
	.permission	= coda_ioctl_permission,
	.setattr	= coda_setattr,
};

const struct file_operations coda_ioctl_operations = {
	.unlocked_ioctl	= coda_pioctl,
	.llseek		= noop_llseek,
};

 
static int coda_ioctl_permission(struct mnt_idmap *idmap,
				 struct inode *inode, int mask)
{
	return (mask & MAY_EXEC) ? -EACCES : 0;
}

static long coda_pioctl(struct file *filp, unsigned int cmd,
			unsigned long user_data)
{
	struct path path;
	int error;
	struct PioctlData data;
	struct inode *inode = file_inode(filp);
	struct inode *target_inode = NULL;
	struct coda_inode_info *cnp;

	 
	if (copy_from_user(&data, (void __user *)user_data, sizeof(data)))
		return -EINVAL;

	 
	error = user_path_at(AT_FDCWD, data.path,
			     data.follow ? LOOKUP_FOLLOW : 0, &path);
	if (error)
		return error;

	target_inode = d_inode(path.dentry);

	 
	if (target_inode->i_sb != inode->i_sb) {
		error = -EINVAL;
		goto out;
	}

	 
	cnp = ITOC(target_inode);

	error = venus_pioctl(inode->i_sb, &(cnp->c_fid), cmd, &data);
out:
	path_put(&path);
	return error;
}
