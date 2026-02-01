 

#include <sys/zfs_context.h>
#include <sys/zfs_file.h>
#include <sys/stat.h>
#include <sys/file.h>
#include <linux/falloc.h>
#include <linux/fs.h>
#include <linux/uaccess.h>
#ifdef HAVE_FDTABLE_HEADER
#include <linux/fdtable.h>
#endif

 
int
zfs_file_open(const char *path, int flags, int mode, zfs_file_t **fpp)
{
	struct file *filp;
	int saved_umask;

	if (!(flags & O_CREAT) && (flags & O_WRONLY))
		flags |= O_EXCL;

	if (flags & O_CREAT)
		saved_umask = xchg(&current->fs->umask, 0);

	filp = filp_open(path, flags, mode);

	if (flags & O_CREAT)
		(void) xchg(&current->fs->umask, saved_umask);

	if (IS_ERR(filp))
		return (-PTR_ERR(filp));

	*fpp = filp;
	return (0);
}

void
zfs_file_close(zfs_file_t *fp)
{
	filp_close(fp, 0);
}

static ssize_t
zfs_file_write_impl(zfs_file_t *fp, const void *buf, size_t count, loff_t *off)
{
#if defined(HAVE_KERNEL_WRITE_PPOS)
	return (kernel_write(fp, buf, count, off));
#else
	mm_segment_t saved_fs;
	ssize_t rc;

	saved_fs = get_fs();
	set_fs(KERNEL_DS);

	rc = vfs_write(fp, (__force const char __user __user *)buf, count, off);

	set_fs(saved_fs);

	return (rc);
#endif
}

 
int
zfs_file_write(zfs_file_t *fp, const void *buf, size_t count, ssize_t *resid)
{
	loff_t off = fp->f_pos;
	ssize_t rc;

	rc = zfs_file_write_impl(fp, buf, count, &off);
	if (rc < 0)
		return (-rc);

	fp->f_pos = off;

	if (resid) {
		*resid = count - rc;
	} else if (rc != count) {
		return (EIO);
	}

	return (0);
}

 
int
zfs_file_pwrite(zfs_file_t *fp, const void *buf, size_t count, loff_t off,
    ssize_t *resid)
{
	ssize_t rc;

	rc  = zfs_file_write_impl(fp, buf, count, &off);
	if (rc < 0)
		return (-rc);

	if (resid) {
		*resid = count - rc;
	} else if (rc != count) {
		return (EIO);
	}

	return (0);
}

static ssize_t
zfs_file_read_impl(zfs_file_t *fp, void *buf, size_t count, loff_t *off)
{
#if defined(HAVE_KERNEL_READ_PPOS)
	return (kernel_read(fp, buf, count, off));
#else
	mm_segment_t saved_fs;
	ssize_t rc;

	saved_fs = get_fs();
	set_fs(KERNEL_DS);

	rc = vfs_read(fp, (void __user *)buf, count, off);
	set_fs(saved_fs);

	return (rc);
#endif
}

 
int
zfs_file_read(zfs_file_t *fp, void *buf, size_t count, ssize_t *resid)
{
	loff_t off = fp->f_pos;
	ssize_t rc;

	rc = zfs_file_read_impl(fp, buf, count, &off);
	if (rc < 0)
		return (-rc);

	fp->f_pos = off;

	if (resid) {
		*resid = count - rc;
	} else if (rc != count) {
		return (EIO);
	}

	return (0);
}

 
int
zfs_file_pread(zfs_file_t *fp, void *buf, size_t count, loff_t off,
    ssize_t *resid)
{
	ssize_t rc;

	rc = zfs_file_read_impl(fp, buf, count, &off);
	if (rc < 0)
		return (-rc);

	if (resid) {
		*resid = count - rc;
	} else if (rc != count) {
		return (EIO);
	}

	return (0);
}

 
int
zfs_file_seek(zfs_file_t *fp, loff_t *offp, int whence)
{
	loff_t rc;

	if (*offp < 0)
		return (EINVAL);

	rc = vfs_llseek(fp, *offp, whence);
	if (rc < 0)
		return (-rc);

	*offp = rc;

	return (0);
}

 
int
zfs_file_getattr(zfs_file_t *filp, zfs_file_attr_t *zfattr)
{
	struct kstat stat;
	int rc;

#if defined(HAVE_4ARGS_VFS_GETATTR)
	rc = vfs_getattr(&filp->f_path, &stat, STATX_BASIC_STATS,
	    AT_STATX_SYNC_AS_STAT);
#elif defined(HAVE_2ARGS_VFS_GETATTR)
	rc = vfs_getattr(&filp->f_path, &stat);
#elif defined(HAVE_3ARGS_VFS_GETATTR)
	rc = vfs_getattr(filp->f_path.mnt, filp->f_dentry, &stat);
#else
#error "No available vfs_getattr()"
#endif
	if (rc)
		return (-rc);

	zfattr->zfa_size = stat.size;
	zfattr->zfa_mode = stat.mode;

	return (0);
}

 
int
zfs_file_fsync(zfs_file_t *filp, int flags)
{
	int datasync = 0;
	int error;
	int fstrans;

	if (flags & O_DSYNC)
		datasync = 1;

	 
	fstrans = __spl_pf_fstrans_check();
	if (fstrans)
		current->flags &= ~(__SPL_PF_FSTRANS);

	error = -vfs_fsync(filp, datasync);

	if (fstrans)
		current->flags |= __SPL_PF_FSTRANS;

	return (error);
}

 
int
zfs_file_fallocate(zfs_file_t *fp, int mode, loff_t offset, loff_t len)
{
	 
	int fstrans = __spl_pf_fstrans_check();
	if (fstrans)
		current->flags &= ~(__SPL_PF_FSTRANS);

	 
	int error = EOPNOTSUPP;
	if (fp->f_op->fallocate)
		error = fp->f_op->fallocate(fp, mode, offset, len);

	if (fstrans)
		current->flags |= __SPL_PF_FSTRANS;

	return (error);
}

 
loff_t
zfs_file_off(zfs_file_t *fp)
{
	return (fp->f_pos);
}

 
void *
zfs_file_private(zfs_file_t *fp)
{
	return (fp->private_data);
}

 
int
zfs_file_unlink(const char *path)
{
	return (EOPNOTSUPP);
}

 
zfs_file_t *
zfs_file_get(int fd)
{
	return (fget(fd));
}

 
void
zfs_file_put(zfs_file_t *fp)
{
	fput(fp);
}
