
 

#include <linux/syscalls.h>
#include <linux/mm.h>
#include <linux/capability.h>
#include <linux/compat.h>
#include <linux/file.h>
#include <linux/fs.h>
#include <linux/security.h>
#include <linux/export.h>
#include <linux/uaccess.h>
#include <linux/writeback.h>
#include <linux/buffer_head.h>
#include <linux/falloc.h>
#include <linux/sched/signal.h>
#include <linux/fiemap.h>
#include <linux/mount.h>
#include <linux/fscrypt.h>
#include <linux/fileattr.h>

#include "internal.h"

#include <asm/ioctls.h>

 
#define FIEMAP_MAX_EXTENTS	(UINT_MAX / sizeof(struct fiemap_extent))

 
long vfs_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
	int error = -ENOTTY;

	if (!filp->f_op->unlocked_ioctl)
		goto out;

	error = filp->f_op->unlocked_ioctl(filp, cmd, arg);
	if (error == -ENOIOCTLCMD)
		error = -ENOTTY;
 out:
	return error;
}
EXPORT_SYMBOL(vfs_ioctl);

static int ioctl_fibmap(struct file *filp, int __user *p)
{
	struct inode *inode = file_inode(filp);
	struct super_block *sb = inode->i_sb;
	int error, ur_block;
	sector_t block;

	if (!capable(CAP_SYS_RAWIO))
		return -EPERM;

	error = get_user(ur_block, p);
	if (error)
		return error;

	if (ur_block < 0)
		return -EINVAL;

	block = ur_block;
	error = bmap(inode, &block);

	if (block > INT_MAX) {
		error = -ERANGE;
		pr_warn_ratelimited("[%s/%d] FS: %s File: %pD4 would truncate fibmap result\n",
				    current->comm, task_pid_nr(current),
				    sb->s_id, filp);
	}

	if (error)
		ur_block = 0;
	else
		ur_block = block;

	if (put_user(ur_block, p))
		error = -EFAULT;

	return error;
}

 
int fiemap_fill_next_extent(struct fiemap_extent_info *fieinfo, u64 logical,
			    u64 phys, u64 len, u32 flags)
{
	struct fiemap_extent extent;
	struct fiemap_extent __user *dest = fieinfo->fi_extents_start;

	 
	if (fieinfo->fi_extents_max == 0) {
		fieinfo->fi_extents_mapped++;
		return (flags & FIEMAP_EXTENT_LAST) ? 1 : 0;
	}

	if (fieinfo->fi_extents_mapped >= fieinfo->fi_extents_max)
		return 1;

#define SET_UNKNOWN_FLAGS	(FIEMAP_EXTENT_DELALLOC)
#define SET_NO_UNMOUNTED_IO_FLAGS	(FIEMAP_EXTENT_DATA_ENCRYPTED)
#define SET_NOT_ALIGNED_FLAGS	(FIEMAP_EXTENT_DATA_TAIL|FIEMAP_EXTENT_DATA_INLINE)

	if (flags & SET_UNKNOWN_FLAGS)
		flags |= FIEMAP_EXTENT_UNKNOWN;
	if (flags & SET_NO_UNMOUNTED_IO_FLAGS)
		flags |= FIEMAP_EXTENT_ENCODED;
	if (flags & SET_NOT_ALIGNED_FLAGS)
		flags |= FIEMAP_EXTENT_NOT_ALIGNED;

	memset(&extent, 0, sizeof(extent));
	extent.fe_logical = logical;
	extent.fe_physical = phys;
	extent.fe_length = len;
	extent.fe_flags = flags;

	dest += fieinfo->fi_extents_mapped;
	if (copy_to_user(dest, &extent, sizeof(extent)))
		return -EFAULT;

	fieinfo->fi_extents_mapped++;
	if (fieinfo->fi_extents_mapped == fieinfo->fi_extents_max)
		return 1;
	return (flags & FIEMAP_EXTENT_LAST) ? 1 : 0;
}
EXPORT_SYMBOL(fiemap_fill_next_extent);

 
int fiemap_prep(struct inode *inode, struct fiemap_extent_info *fieinfo,
		u64 start, u64 *len, u32 supported_flags)
{
	u64 maxbytes = inode->i_sb->s_maxbytes;
	u32 incompat_flags;
	int ret = 0;

	if (*len == 0)
		return -EINVAL;
	if (start >= maxbytes)
		return -EFBIG;

	 
	if (*len > maxbytes || (maxbytes - *len) < start)
		*len = maxbytes - start;

	supported_flags |= FIEMAP_FLAG_SYNC;
	supported_flags &= FIEMAP_FLAGS_COMPAT;
	incompat_flags = fieinfo->fi_flags & ~supported_flags;
	if (incompat_flags) {
		fieinfo->fi_flags = incompat_flags;
		return -EBADR;
	}

	if (fieinfo->fi_flags & FIEMAP_FLAG_SYNC)
		ret = filemap_write_and_wait(inode->i_mapping);
	return ret;
}
EXPORT_SYMBOL(fiemap_prep);

static int ioctl_fiemap(struct file *filp, struct fiemap __user *ufiemap)
{
	struct fiemap fiemap;
	struct fiemap_extent_info fieinfo = { 0, };
	struct inode *inode = file_inode(filp);
	int error;

	if (!inode->i_op->fiemap)
		return -EOPNOTSUPP;

	if (copy_from_user(&fiemap, ufiemap, sizeof(fiemap)))
		return -EFAULT;

	if (fiemap.fm_extent_count > FIEMAP_MAX_EXTENTS)
		return -EINVAL;

	fieinfo.fi_flags = fiemap.fm_flags;
	fieinfo.fi_extents_max = fiemap.fm_extent_count;
	fieinfo.fi_extents_start = ufiemap->fm_extents;

	error = inode->i_op->fiemap(inode, &fieinfo, fiemap.fm_start,
			fiemap.fm_length);

	fiemap.fm_flags = fieinfo.fi_flags;
	fiemap.fm_mapped_extents = fieinfo.fi_extents_mapped;
	if (copy_to_user(ufiemap, &fiemap, sizeof(fiemap)))
		error = -EFAULT;

	return error;
}

static long ioctl_file_clone(struct file *dst_file, unsigned long srcfd,
			     u64 off, u64 olen, u64 destoff)
{
	struct fd src_file = fdget(srcfd);
	loff_t cloned;
	int ret;

	if (!src_file.file)
		return -EBADF;
	cloned = vfs_clone_file_range(src_file.file, off, dst_file, destoff,
				      olen, 0);
	if (cloned < 0)
		ret = cloned;
	else if (olen && cloned != olen)
		ret = -EINVAL;
	else
		ret = 0;
	fdput(src_file);
	return ret;
}

static long ioctl_file_clone_range(struct file *file,
				   struct file_clone_range __user *argp)
{
	struct file_clone_range args;

	if (copy_from_user(&args, argp, sizeof(args)))
		return -EFAULT;
	return ioctl_file_clone(file, args.src_fd, args.src_offset,
				args.src_length, args.dest_offset);
}

 
static int ioctl_preallocate(struct file *filp, int mode, void __user *argp)
{
	struct inode *inode = file_inode(filp);
	struct space_resv sr;

	if (copy_from_user(&sr, argp, sizeof(sr)))
		return -EFAULT;

	switch (sr.l_whence) {
	case SEEK_SET:
		break;
	case SEEK_CUR:
		sr.l_start += filp->f_pos;
		break;
	case SEEK_END:
		sr.l_start += i_size_read(inode);
		break;
	default:
		return -EINVAL;
	}

	return vfs_fallocate(filp, mode | FALLOC_FL_KEEP_SIZE, sr.l_start,
			sr.l_len);
}

 
#if defined CONFIG_COMPAT && defined(CONFIG_X86_64)
 
static int compat_ioctl_preallocate(struct file *file, int mode,
				    struct space_resv_32 __user *argp)
{
	struct inode *inode = file_inode(file);
	struct space_resv_32 sr;

	if (copy_from_user(&sr, argp, sizeof(sr)))
		return -EFAULT;

	switch (sr.l_whence) {
	case SEEK_SET:
		break;
	case SEEK_CUR:
		sr.l_start += file->f_pos;
		break;
	case SEEK_END:
		sr.l_start += i_size_read(inode);
		break;
	default:
		return -EINVAL;
	}

	return vfs_fallocate(file, mode | FALLOC_FL_KEEP_SIZE, sr.l_start, sr.l_len);
}
#endif

static int file_ioctl(struct file *filp, unsigned int cmd, int __user *p)
{
	switch (cmd) {
	case FIBMAP:
		return ioctl_fibmap(filp, p);
	case FS_IOC_RESVSP:
	case FS_IOC_RESVSP64:
		return ioctl_preallocate(filp, 0, p);
	case FS_IOC_UNRESVSP:
	case FS_IOC_UNRESVSP64:
		return ioctl_preallocate(filp, FALLOC_FL_PUNCH_HOLE, p);
	case FS_IOC_ZERO_RANGE:
		return ioctl_preallocate(filp, FALLOC_FL_ZERO_RANGE, p);
	}

	return -ENOIOCTLCMD;
}

static int ioctl_fionbio(struct file *filp, int __user *argp)
{
	unsigned int flag;
	int on, error;

	error = get_user(on, argp);
	if (error)
		return error;
	flag = O_NONBLOCK;
#ifdef __sparc__
	 
	if (O_NONBLOCK != O_NDELAY)
		flag |= O_NDELAY;
#endif
	spin_lock(&filp->f_lock);
	if (on)
		filp->f_flags |= flag;
	else
		filp->f_flags &= ~flag;
	spin_unlock(&filp->f_lock);
	return error;
}

static int ioctl_fioasync(unsigned int fd, struct file *filp,
			  int __user *argp)
{
	unsigned int flag;
	int on, error;

	error = get_user(on, argp);
	if (error)
		return error;
	flag = on ? FASYNC : 0;

	 
	if ((flag ^ filp->f_flags) & FASYNC) {
		if (filp->f_op->fasync)
			 
			error = filp->f_op->fasync(fd, filp, on);
		else
			error = -ENOTTY;
	}
	return error < 0 ? error : 0;
}

static int ioctl_fsfreeze(struct file *filp)
{
	struct super_block *sb = file_inode(filp)->i_sb;

	if (!ns_capable(sb->s_user_ns, CAP_SYS_ADMIN))
		return -EPERM;

	 
	if (sb->s_op->freeze_fs == NULL && sb->s_op->freeze_super == NULL)
		return -EOPNOTSUPP;

	 
	if (sb->s_op->freeze_super)
		return sb->s_op->freeze_super(sb, FREEZE_HOLDER_USERSPACE);
	return freeze_super(sb, FREEZE_HOLDER_USERSPACE);
}

static int ioctl_fsthaw(struct file *filp)
{
	struct super_block *sb = file_inode(filp)->i_sb;

	if (!ns_capable(sb->s_user_ns, CAP_SYS_ADMIN))
		return -EPERM;

	 
	if (sb->s_op->thaw_super)
		return sb->s_op->thaw_super(sb, FREEZE_HOLDER_USERSPACE);
	return thaw_super(sb, FREEZE_HOLDER_USERSPACE);
}

static int ioctl_file_dedupe_range(struct file *file,
				   struct file_dedupe_range __user *argp)
{
	struct file_dedupe_range *same = NULL;
	int ret;
	unsigned long size;
	u16 count;

	if (get_user(count, &argp->dest_count)) {
		ret = -EFAULT;
		goto out;
	}

	size = offsetof(struct file_dedupe_range, info[count]);
	if (size > PAGE_SIZE) {
		ret = -ENOMEM;
		goto out;
	}

	same = memdup_user(argp, size);
	if (IS_ERR(same)) {
		ret = PTR_ERR(same);
		same = NULL;
		goto out;
	}

	same->dest_count = count;
	ret = vfs_dedupe_file_range(file, same);
	if (ret)
		goto out;

	ret = copy_to_user(argp, same, size);
	if (ret)
		ret = -EFAULT;

out:
	kfree(same);
	return ret;
}

 
void fileattr_fill_xflags(struct fileattr *fa, u32 xflags)
{
	memset(fa, 0, sizeof(*fa));
	fa->fsx_valid = true;
	fa->fsx_xflags = xflags;
	if (fa->fsx_xflags & FS_XFLAG_IMMUTABLE)
		fa->flags |= FS_IMMUTABLE_FL;
	if (fa->fsx_xflags & FS_XFLAG_APPEND)
		fa->flags |= FS_APPEND_FL;
	if (fa->fsx_xflags & FS_XFLAG_SYNC)
		fa->flags |= FS_SYNC_FL;
	if (fa->fsx_xflags & FS_XFLAG_NOATIME)
		fa->flags |= FS_NOATIME_FL;
	if (fa->fsx_xflags & FS_XFLAG_NODUMP)
		fa->flags |= FS_NODUMP_FL;
	if (fa->fsx_xflags & FS_XFLAG_DAX)
		fa->flags |= FS_DAX_FL;
	if (fa->fsx_xflags & FS_XFLAG_PROJINHERIT)
		fa->flags |= FS_PROJINHERIT_FL;
}
EXPORT_SYMBOL(fileattr_fill_xflags);

 
void fileattr_fill_flags(struct fileattr *fa, u32 flags)
{
	memset(fa, 0, sizeof(*fa));
	fa->flags_valid = true;
	fa->flags = flags;
	if (fa->flags & FS_SYNC_FL)
		fa->fsx_xflags |= FS_XFLAG_SYNC;
	if (fa->flags & FS_IMMUTABLE_FL)
		fa->fsx_xflags |= FS_XFLAG_IMMUTABLE;
	if (fa->flags & FS_APPEND_FL)
		fa->fsx_xflags |= FS_XFLAG_APPEND;
	if (fa->flags & FS_NODUMP_FL)
		fa->fsx_xflags |= FS_XFLAG_NODUMP;
	if (fa->flags & FS_NOATIME_FL)
		fa->fsx_xflags |= FS_XFLAG_NOATIME;
	if (fa->flags & FS_DAX_FL)
		fa->fsx_xflags |= FS_XFLAG_DAX;
	if (fa->flags & FS_PROJINHERIT_FL)
		fa->fsx_xflags |= FS_XFLAG_PROJINHERIT;
}
EXPORT_SYMBOL(fileattr_fill_flags);

 
int vfs_fileattr_get(struct dentry *dentry, struct fileattr *fa)
{
	struct inode *inode = d_inode(dentry);

	if (!inode->i_op->fileattr_get)
		return -ENOIOCTLCMD;

	return inode->i_op->fileattr_get(dentry, fa);
}
EXPORT_SYMBOL(vfs_fileattr_get);

 
int copy_fsxattr_to_user(const struct fileattr *fa, struct fsxattr __user *ufa)
{
	struct fsxattr xfa;

	memset(&xfa, 0, sizeof(xfa));
	xfa.fsx_xflags = fa->fsx_xflags;
	xfa.fsx_extsize = fa->fsx_extsize;
	xfa.fsx_nextents = fa->fsx_nextents;
	xfa.fsx_projid = fa->fsx_projid;
	xfa.fsx_cowextsize = fa->fsx_cowextsize;

	if (copy_to_user(ufa, &xfa, sizeof(xfa)))
		return -EFAULT;

	return 0;
}
EXPORT_SYMBOL(copy_fsxattr_to_user);

static int copy_fsxattr_from_user(struct fileattr *fa,
				  struct fsxattr __user *ufa)
{
	struct fsxattr xfa;

	if (copy_from_user(&xfa, ufa, sizeof(xfa)))
		return -EFAULT;

	fileattr_fill_xflags(fa, xfa.fsx_xflags);
	fa->fsx_extsize = xfa.fsx_extsize;
	fa->fsx_nextents = xfa.fsx_nextents;
	fa->fsx_projid = xfa.fsx_projid;
	fa->fsx_cowextsize = xfa.fsx_cowextsize;

	return 0;
}

 
static int fileattr_set_prepare(struct inode *inode,
			      const struct fileattr *old_ma,
			      struct fileattr *fa)
{
	int err;

	 
	if ((fa->flags ^ old_ma->flags) & (FS_APPEND_FL | FS_IMMUTABLE_FL) &&
	    !capable(CAP_LINUX_IMMUTABLE))
		return -EPERM;

	err = fscrypt_prepare_setflags(inode, old_ma->flags, fa->flags);
	if (err)
		return err;

	 
	if (current_user_ns() != &init_user_ns) {
		if (old_ma->fsx_projid != fa->fsx_projid)
			return -EINVAL;
		if ((old_ma->fsx_xflags ^ fa->fsx_xflags) &
				FS_XFLAG_PROJINHERIT)
			return -EINVAL;
	} else {
		 
		if (old_ma->fsx_projid != fa->fsx_projid &&
		    !projid_valid(make_kprojid(&init_user_ns, fa->fsx_projid)))
			return -EINVAL;
	}

	 
	if ((fa->fsx_xflags & FS_XFLAG_EXTSIZE) && !S_ISREG(inode->i_mode))
		return -EINVAL;

	if ((fa->fsx_xflags & FS_XFLAG_EXTSZINHERIT) &&
			!S_ISDIR(inode->i_mode))
		return -EINVAL;

	if ((fa->fsx_xflags & FS_XFLAG_COWEXTSIZE) &&
	    !S_ISREG(inode->i_mode) && !S_ISDIR(inode->i_mode))
		return -EINVAL;

	 
	if ((fa->fsx_xflags & FS_XFLAG_DAX) &&
	    !(S_ISREG(inode->i_mode) || S_ISDIR(inode->i_mode)))
		return -EINVAL;

	 
	if (fa->fsx_extsize == 0)
		fa->fsx_xflags &= ~(FS_XFLAG_EXTSIZE | FS_XFLAG_EXTSZINHERIT);
	if (fa->fsx_cowextsize == 0)
		fa->fsx_xflags &= ~FS_XFLAG_COWEXTSIZE;

	return 0;
}

 
int vfs_fileattr_set(struct mnt_idmap *idmap, struct dentry *dentry,
		     struct fileattr *fa)
{
	struct inode *inode = d_inode(dentry);
	struct fileattr old_ma = {};
	int err;

	if (!inode->i_op->fileattr_set)
		return -ENOIOCTLCMD;

	if (!inode_owner_or_capable(idmap, inode))
		return -EPERM;

	inode_lock(inode);
	err = vfs_fileattr_get(dentry, &old_ma);
	if (!err) {
		 
		if (fa->flags_valid) {
			fa->fsx_xflags |= old_ma.fsx_xflags & ~FS_XFLAG_COMMON;
			fa->fsx_extsize = old_ma.fsx_extsize;
			fa->fsx_nextents = old_ma.fsx_nextents;
			fa->fsx_projid = old_ma.fsx_projid;
			fa->fsx_cowextsize = old_ma.fsx_cowextsize;
		} else {
			fa->flags |= old_ma.flags & ~FS_COMMON_FL;
		}
		err = fileattr_set_prepare(inode, &old_ma, fa);
		if (!err)
			err = inode->i_op->fileattr_set(idmap, dentry, fa);
	}
	inode_unlock(inode);

	return err;
}
EXPORT_SYMBOL(vfs_fileattr_set);

static int ioctl_getflags(struct file *file, unsigned int __user *argp)
{
	struct fileattr fa = { .flags_valid = true };  
	int err;

	err = vfs_fileattr_get(file->f_path.dentry, &fa);
	if (!err)
		err = put_user(fa.flags, argp);
	return err;
}

static int ioctl_setflags(struct file *file, unsigned int __user *argp)
{
	struct mnt_idmap *idmap = file_mnt_idmap(file);
	struct dentry *dentry = file->f_path.dentry;
	struct fileattr fa;
	unsigned int flags;
	int err;

	err = get_user(flags, argp);
	if (!err) {
		err = mnt_want_write_file(file);
		if (!err) {
			fileattr_fill_flags(&fa, flags);
			err = vfs_fileattr_set(idmap, dentry, &fa);
			mnt_drop_write_file(file);
		}
	}
	return err;
}

static int ioctl_fsgetxattr(struct file *file, void __user *argp)
{
	struct fileattr fa = { .fsx_valid = true };  
	int err;

	err = vfs_fileattr_get(file->f_path.dentry, &fa);
	if (!err)
		err = copy_fsxattr_to_user(&fa, argp);

	return err;
}

static int ioctl_fssetxattr(struct file *file, void __user *argp)
{
	struct mnt_idmap *idmap = file_mnt_idmap(file);
	struct dentry *dentry = file->f_path.dentry;
	struct fileattr fa;
	int err;

	err = copy_fsxattr_from_user(&fa, argp);
	if (!err) {
		err = mnt_want_write_file(file);
		if (!err) {
			err = vfs_fileattr_set(idmap, dentry, &fa);
			mnt_drop_write_file(file);
		}
	}
	return err;
}

 
static int do_vfs_ioctl(struct file *filp, unsigned int fd,
			unsigned int cmd, unsigned long arg)
{
	void __user *argp = (void __user *)arg;
	struct inode *inode = file_inode(filp);

	switch (cmd) {
	case FIOCLEX:
		set_close_on_exec(fd, 1);
		return 0;

	case FIONCLEX:
		set_close_on_exec(fd, 0);
		return 0;

	case FIONBIO:
		return ioctl_fionbio(filp, argp);

	case FIOASYNC:
		return ioctl_fioasync(fd, filp, argp);

	case FIOQSIZE:
		if (S_ISDIR(inode->i_mode) || S_ISREG(inode->i_mode) ||
		    S_ISLNK(inode->i_mode)) {
			loff_t res = inode_get_bytes(inode);
			return copy_to_user(argp, &res, sizeof(res)) ?
					    -EFAULT : 0;
		}

		return -ENOTTY;

	case FIFREEZE:
		return ioctl_fsfreeze(filp);

	case FITHAW:
		return ioctl_fsthaw(filp);

	case FS_IOC_FIEMAP:
		return ioctl_fiemap(filp, argp);

	case FIGETBSZ:
		 
		if (!inode->i_sb->s_blocksize)
			return -EINVAL;

		return put_user(inode->i_sb->s_blocksize, (int __user *)argp);

	case FICLONE:
		return ioctl_file_clone(filp, arg, 0, 0, 0);

	case FICLONERANGE:
		return ioctl_file_clone_range(filp, argp);

	case FIDEDUPERANGE:
		return ioctl_file_dedupe_range(filp, argp);

	case FIONREAD:
		if (!S_ISREG(inode->i_mode))
			return vfs_ioctl(filp, cmd, arg);

		return put_user(i_size_read(inode) - filp->f_pos,
				(int __user *)argp);

	case FS_IOC_GETFLAGS:
		return ioctl_getflags(filp, argp);

	case FS_IOC_SETFLAGS:
		return ioctl_setflags(filp, argp);

	case FS_IOC_FSGETXATTR:
		return ioctl_fsgetxattr(filp, argp);

	case FS_IOC_FSSETXATTR:
		return ioctl_fssetxattr(filp, argp);

	default:
		if (S_ISREG(inode->i_mode))
			return file_ioctl(filp, cmd, argp);
		break;
	}

	return -ENOIOCTLCMD;
}

SYSCALL_DEFINE3(ioctl, unsigned int, fd, unsigned int, cmd, unsigned long, arg)
{
	struct fd f = fdget(fd);
	int error;

	if (!f.file)
		return -EBADF;

	error = security_file_ioctl(f.file, cmd, arg);
	if (error)
		goto out;

	error = do_vfs_ioctl(f.file, fd, cmd, arg);
	if (error == -ENOIOCTLCMD)
		error = vfs_ioctl(f.file, cmd, arg);

out:
	fdput(f);
	return error;
}

#ifdef CONFIG_COMPAT
 
long compat_ptr_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
	if (!file->f_op->unlocked_ioctl)
		return -ENOIOCTLCMD;

	return file->f_op->unlocked_ioctl(file, cmd, (unsigned long)compat_ptr(arg));
}
EXPORT_SYMBOL(compat_ptr_ioctl);

COMPAT_SYSCALL_DEFINE3(ioctl, unsigned int, fd, unsigned int, cmd,
		       compat_ulong_t, arg)
{
	struct fd f = fdget(fd);
	int error;

	if (!f.file)
		return -EBADF;

	 
	error = security_file_ioctl(f.file, cmd, arg);
	if (error)
		goto out;

	switch (cmd) {
	 
	case FICLONE:
		error = ioctl_file_clone(f.file, arg, 0, 0, 0);
		break;

#if defined(CONFIG_X86_64)
	 
	case FS_IOC_RESVSP_32:
	case FS_IOC_RESVSP64_32:
		error = compat_ioctl_preallocate(f.file, 0, compat_ptr(arg));
		break;
	case FS_IOC_UNRESVSP_32:
	case FS_IOC_UNRESVSP64_32:
		error = compat_ioctl_preallocate(f.file, FALLOC_FL_PUNCH_HOLE,
				compat_ptr(arg));
		break;
	case FS_IOC_ZERO_RANGE_32:
		error = compat_ioctl_preallocate(f.file, FALLOC_FL_ZERO_RANGE,
				compat_ptr(arg));
		break;
#endif

	 
	case FS_IOC32_GETFLAGS:
	case FS_IOC32_SETFLAGS:
		cmd = (cmd == FS_IOC32_GETFLAGS) ?
			FS_IOC_GETFLAGS : FS_IOC_SETFLAGS;
		fallthrough;
	 
	default:
		error = do_vfs_ioctl(f.file, fd, cmd,
				     (unsigned long)compat_ptr(arg));
		if (error != -ENOIOCTLCMD)
			break;

		if (f.file->f_op->compat_ioctl)
			error = f.file->f_op->compat_ioctl(f.file, cmd, arg);
		if (error == -ENOIOCTLCMD)
			error = -ENOTTY;
		break;
	}

 out:
	fdput(f);

	return error;
}
#endif
