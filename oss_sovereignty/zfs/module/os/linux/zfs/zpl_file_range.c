#ifdef CONFIG_COMPAT
#include <linux/compat.h>
#endif
#include <linux/fs.h>
#include <sys/file.h>
#include <sys/zfs_znode.h>
#include <sys/zfs_vnops.h>
#include <sys/zfeature.h>
int zfs_bclone_enabled = 0;
static ssize_t
__zpl_clone_file_range(struct file *src_file, loff_t src_off,
    struct file *dst_file, loff_t dst_off, size_t len)
{
	struct inode *src_i = file_inode(src_file);
	struct inode *dst_i = file_inode(dst_file);
	uint64_t src_off_o = (uint64_t)src_off;
	uint64_t dst_off_o = (uint64_t)dst_off;
	uint64_t len_o = (uint64_t)len;
	cred_t *cr = CRED();
	fstrans_cookie_t cookie;
	int err;
	if (!zfs_bclone_enabled)
		return (-EOPNOTSUPP);
	if (!spa_feature_is_enabled(
	    dmu_objset_spa(ITOZSB(dst_i)->z_os), SPA_FEATURE_BLOCK_CLONING))
		return (-EOPNOTSUPP);
	if (src_i != dst_i)
		spl_inode_lock_shared(src_i);
	spl_inode_lock(dst_i);
	crhold(cr);
	cookie = spl_fstrans_mark();
	err = -zfs_clone_range(ITOZ(src_i), &src_off_o, ITOZ(dst_i),
	    &dst_off_o, &len_o, cr);
	spl_fstrans_unmark(cookie);
	crfree(cr);
	spl_inode_unlock(dst_i);
	if (src_i != dst_i)
		spl_inode_unlock_shared(src_i);
	if (err < 0)
		return (err);
	return ((ssize_t)len_o);
}
#if defined(HAVE_VFS_COPY_FILE_RANGE) || \
    defined(HAVE_VFS_FILE_OPERATIONS_EXTEND)
ssize_t
zpl_copy_file_range(struct file *src_file, loff_t src_off,
    struct file *dst_file, loff_t dst_off, size_t len, unsigned int flags)
{
	ssize_t ret;
	if (flags != 0)
		return (-EINVAL);
	ret = __zpl_clone_file_range(src_file, src_off,
	    dst_file, dst_off, len);
#ifdef HAVE_VFS_GENERIC_COPY_FILE_RANGE
	if (ret == -EOPNOTSUPP || ret == -EINVAL || ret == -EXDEV ||
	    ret == -EAGAIN)
		ret = generic_copy_file_range(src_file, src_off, dst_file,
		    dst_off, len, flags);
#else
	if (ret == -EINVAL || ret == -EXDEV || ret == -EAGAIN)
		ret = -EOPNOTSUPP;
#endif  
	return (ret);
}
#endif  
#ifdef HAVE_VFS_REMAP_FILE_RANGE
loff_t
zpl_remap_file_range(struct file *src_file, loff_t src_off,
    struct file *dst_file, loff_t dst_off, loff_t len, unsigned int flags)
{
	if (flags & ~(REMAP_FILE_DEDUP | REMAP_FILE_CAN_SHORTEN))
		return (-EINVAL);
	flags &= ~REMAP_FILE_CAN_SHORTEN;
	if (flags & REMAP_FILE_DEDUP)
		return (-EOPNOTSUPP);
	if (len == 0)
		len = i_size_read(file_inode(src_file)) - src_off;
	return (__zpl_clone_file_range(src_file, src_off,
	    dst_file, dst_off, len));
}
#endif  
#if defined(HAVE_VFS_CLONE_FILE_RANGE) || \
    defined(HAVE_VFS_FILE_OPERATIONS_EXTEND)
int
zpl_clone_file_range(struct file *src_file, loff_t src_off,
    struct file *dst_file, loff_t dst_off, uint64_t len)
{
	if (len == 0)
		len = i_size_read(file_inode(src_file)) - src_off;
	return (__zpl_clone_file_range(src_file, src_off,
	    dst_file, dst_off, len));
}
#endif  
#ifdef HAVE_VFS_DEDUPE_FILE_RANGE
int
zpl_dedupe_file_range(struct file *src_file, loff_t src_off,
    struct file *dst_file, loff_t dst_off, uint64_t len)
{
	return (-EOPNOTSUPP);
}
#endif  
long
zpl_ioctl_ficlone(struct file *dst_file, void *arg)
{
	unsigned long sfd = (unsigned long)arg;
	struct file *src_file = fget(sfd);
	if (src_file == NULL)
		return (-EBADF);
	if (dst_file->f_op != src_file->f_op) {
		fput(src_file);
		return (-EXDEV);
	}
	size_t len = i_size_read(file_inode(src_file));
	ssize_t ret =
	    __zpl_clone_file_range(src_file, 0, dst_file, 0, len);
	fput(src_file);
	if (ret < 0) {
		if (ret == -EOPNOTSUPP)
			return (-ENOTTY);
		return (ret);
	}
	if (ret != len)
		return (-EINVAL);
	return (0);
}
long
zpl_ioctl_ficlonerange(struct file *dst_file, void __user *arg)
{
	zfs_ioc_compat_file_clone_range_t fcr;
	if (copy_from_user(&fcr, arg, sizeof (fcr)))
		return (-EFAULT);
	struct file *src_file = fget(fcr.fcr_src_fd);
	if (src_file == NULL)
		return (-EBADF);
	if (dst_file->f_op != src_file->f_op) {
		fput(src_file);
		return (-EXDEV);
	}
	size_t len = fcr.fcr_src_length;
	if (len == 0)
		len = i_size_read(file_inode(src_file)) - fcr.fcr_src_offset;
	ssize_t ret = __zpl_clone_file_range(src_file, fcr.fcr_src_offset,
	    dst_file, fcr.fcr_dest_offset, len);
	fput(src_file);
	if (ret < 0) {
		if (ret == -EOPNOTSUPP)
			return (-ENOTTY);
		return (ret);
	}
	if (ret != len)
		return (-EINVAL);
	return (0);
}
long
zpl_ioctl_fideduperange(struct file *filp, void *arg)
{
	(void) arg;
	return (-ENOTTY);
}
