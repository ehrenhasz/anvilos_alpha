
 
#include "xfs.h"
#include "xfs_fs.h"
#include "xfs_shared.h"
#include "xfs_format.h"
#include "xfs_log_format.h"
#include "xfs_trans_resv.h"
#include "xfs_mount.h"
#include "xfs_inode.h"
#include "xfs_trans.h"
#include "xfs_inode_item.h"
#include "xfs_bmap.h"
#include "xfs_bmap_util.h"
#include "xfs_dir2.h"
#include "xfs_dir2_priv.h"
#include "xfs_ioctl.h"
#include "xfs_trace.h"
#include "xfs_log.h"
#include "xfs_icache.h"
#include "xfs_pnfs.h"
#include "xfs_iomap.h"
#include "xfs_reflink.h"

#include <linux/dax.h>
#include <linux/falloc.h>
#include <linux/backing-dev.h>
#include <linux/mman.h>
#include <linux/fadvise.h>
#include <linux/mount.h>

static const struct vm_operations_struct xfs_file_vm_ops;

 
static bool
xfs_is_falloc_aligned(
	struct xfs_inode	*ip,
	loff_t			pos,
	long long int		len)
{
	struct xfs_mount	*mp = ip->i_mount;
	uint64_t		mask;

	if (XFS_IS_REALTIME_INODE(ip)) {
		if (!is_power_of_2(mp->m_sb.sb_rextsize)) {
			u64	rextbytes;
			u32	mod;

			rextbytes = XFS_FSB_TO_B(mp, mp->m_sb.sb_rextsize);
			div_u64_rem(pos, rextbytes, &mod);
			if (mod)
				return false;
			div_u64_rem(len, rextbytes, &mod);
			return mod == 0;
		}
		mask = XFS_FSB_TO_B(mp, mp->m_sb.sb_rextsize) - 1;
	} else {
		mask = mp->m_sb.sb_blocksize - 1;
	}

	return !((pos | len) & mask);
}

 
STATIC int
xfs_dir_fsync(
	struct file		*file,
	loff_t			start,
	loff_t			end,
	int			datasync)
{
	struct xfs_inode	*ip = XFS_I(file->f_mapping->host);

	trace_xfs_dir_fsync(ip);
	return xfs_log_force_inode(ip);
}

static xfs_csn_t
xfs_fsync_seq(
	struct xfs_inode	*ip,
	bool			datasync)
{
	if (!xfs_ipincount(ip))
		return 0;
	if (datasync && !(ip->i_itemp->ili_fsync_fields & ~XFS_ILOG_TIMESTAMP))
		return 0;
	return ip->i_itemp->ili_commit_seq;
}

 
static  int
xfs_fsync_flush_log(
	struct xfs_inode	*ip,
	bool			datasync,
	int			*log_flushed)
{
	int			error = 0;
	xfs_csn_t		seq;

	xfs_ilock(ip, XFS_ILOCK_SHARED);
	seq = xfs_fsync_seq(ip, datasync);
	if (seq) {
		error = xfs_log_force_seq(ip->i_mount, seq, XFS_LOG_SYNC,
					  log_flushed);

		spin_lock(&ip->i_itemp->ili_lock);
		ip->i_itemp->ili_fsync_fields = 0;
		spin_unlock(&ip->i_itemp->ili_lock);
	}
	xfs_iunlock(ip, XFS_ILOCK_SHARED);
	return error;
}

STATIC int
xfs_file_fsync(
	struct file		*file,
	loff_t			start,
	loff_t			end,
	int			datasync)
{
	struct xfs_inode	*ip = XFS_I(file->f_mapping->host);
	struct xfs_mount	*mp = ip->i_mount;
	int			error, err2;
	int			log_flushed = 0;

	trace_xfs_file_fsync(ip);

	error = file_write_and_wait_range(file, start, end);
	if (error)
		return error;

	if (xfs_is_shutdown(mp))
		return -EIO;

	xfs_iflags_clear(ip, XFS_ITRUNCATED);

	 
	if (XFS_IS_REALTIME_INODE(ip))
		error = blkdev_issue_flush(mp->m_rtdev_targp->bt_bdev);
	else if (mp->m_logdev_targp != mp->m_ddev_targp)
		error = blkdev_issue_flush(mp->m_ddev_targp->bt_bdev);

	 
	if (xfs_ipincount(ip)) {
		err2 = xfs_fsync_flush_log(ip, datasync, &log_flushed);
		if (err2 && !error)
			error = err2;
	}

	 
	if (!log_flushed && !XFS_IS_REALTIME_INODE(ip) &&
	    mp->m_logdev_targp == mp->m_ddev_targp) {
		err2 = blkdev_issue_flush(mp->m_ddev_targp->bt_bdev);
		if (err2 && !error)
			error = err2;
	}

	return error;
}

static int
xfs_ilock_iocb(
	struct kiocb		*iocb,
	unsigned int		lock_mode)
{
	struct xfs_inode	*ip = XFS_I(file_inode(iocb->ki_filp));

	if (iocb->ki_flags & IOCB_NOWAIT) {
		if (!xfs_ilock_nowait(ip, lock_mode))
			return -EAGAIN;
	} else {
		xfs_ilock(ip, lock_mode);
	}

	return 0;
}

STATIC ssize_t
xfs_file_dio_read(
	struct kiocb		*iocb,
	struct iov_iter		*to)
{
	struct xfs_inode	*ip = XFS_I(file_inode(iocb->ki_filp));
	ssize_t			ret;

	trace_xfs_file_direct_read(iocb, to);

	if (!iov_iter_count(to))
		return 0;  

	file_accessed(iocb->ki_filp);

	ret = xfs_ilock_iocb(iocb, XFS_IOLOCK_SHARED);
	if (ret)
		return ret;
	ret = iomap_dio_rw(iocb, to, &xfs_read_iomap_ops, NULL, 0, NULL, 0);
	xfs_iunlock(ip, XFS_IOLOCK_SHARED);

	return ret;
}

static noinline ssize_t
xfs_file_dax_read(
	struct kiocb		*iocb,
	struct iov_iter		*to)
{
	struct xfs_inode	*ip = XFS_I(iocb->ki_filp->f_mapping->host);
	ssize_t			ret = 0;

	trace_xfs_file_dax_read(iocb, to);

	if (!iov_iter_count(to))
		return 0;  

	ret = xfs_ilock_iocb(iocb, XFS_IOLOCK_SHARED);
	if (ret)
		return ret;
	ret = dax_iomap_rw(iocb, to, &xfs_read_iomap_ops);
	xfs_iunlock(ip, XFS_IOLOCK_SHARED);

	file_accessed(iocb->ki_filp);
	return ret;
}

STATIC ssize_t
xfs_file_buffered_read(
	struct kiocb		*iocb,
	struct iov_iter		*to)
{
	struct xfs_inode	*ip = XFS_I(file_inode(iocb->ki_filp));
	ssize_t			ret;

	trace_xfs_file_buffered_read(iocb, to);

	ret = xfs_ilock_iocb(iocb, XFS_IOLOCK_SHARED);
	if (ret)
		return ret;
	ret = generic_file_read_iter(iocb, to);
	xfs_iunlock(ip, XFS_IOLOCK_SHARED);

	return ret;
}

STATIC ssize_t
xfs_file_read_iter(
	struct kiocb		*iocb,
	struct iov_iter		*to)
{
	struct inode		*inode = file_inode(iocb->ki_filp);
	struct xfs_mount	*mp = XFS_I(inode)->i_mount;
	ssize_t			ret = 0;

	XFS_STATS_INC(mp, xs_read_calls);

	if (xfs_is_shutdown(mp))
		return -EIO;

	if (IS_DAX(inode))
		ret = xfs_file_dax_read(iocb, to);
	else if (iocb->ki_flags & IOCB_DIRECT)
		ret = xfs_file_dio_read(iocb, to);
	else
		ret = xfs_file_buffered_read(iocb, to);

	if (ret > 0)
		XFS_STATS_ADD(mp, xs_read_bytes, ret);
	return ret;
}

STATIC ssize_t
xfs_file_splice_read(
	struct file		*in,
	loff_t			*ppos,
	struct pipe_inode_info	*pipe,
	size_t			len,
	unsigned int		flags)
{
	struct inode		*inode = file_inode(in);
	struct xfs_inode	*ip = XFS_I(inode);
	struct xfs_mount	*mp = ip->i_mount;
	ssize_t			ret = 0;

	XFS_STATS_INC(mp, xs_read_calls);

	if (xfs_is_shutdown(mp))
		return -EIO;

	trace_xfs_file_splice_read(ip, *ppos, len);

	xfs_ilock(ip, XFS_IOLOCK_SHARED);
	ret = filemap_splice_read(in, ppos, pipe, len, flags);
	xfs_iunlock(ip, XFS_IOLOCK_SHARED);
	if (ret > 0)
		XFS_STATS_ADD(mp, xs_read_bytes, ret);
	return ret;
}

 
STATIC ssize_t
xfs_file_write_checks(
	struct kiocb		*iocb,
	struct iov_iter		*from,
	unsigned int		*iolock)
{
	struct file		*file = iocb->ki_filp;
	struct inode		*inode = file->f_mapping->host;
	struct xfs_inode	*ip = XFS_I(inode);
	ssize_t			error = 0;
	size_t			count = iov_iter_count(from);
	bool			drained_dio = false;
	loff_t			isize;

restart:
	error = generic_write_checks(iocb, from);
	if (error <= 0)
		return error;

	if (iocb->ki_flags & IOCB_NOWAIT) {
		error = break_layout(inode, false);
		if (error == -EWOULDBLOCK)
			error = -EAGAIN;
	} else {
		error = xfs_break_layouts(inode, iolock, BREAK_WRITE);
	}

	if (error)
		return error;

	 
	if (*iolock == XFS_IOLOCK_SHARED && !IS_NOSEC(inode)) {
		xfs_iunlock(ip, *iolock);
		*iolock = XFS_IOLOCK_EXCL;
		error = xfs_ilock_iocb(iocb, *iolock);
		if (error) {
			*iolock = 0;
			return error;
		}
		goto restart;
	}

	 
	if (iocb->ki_pos <= i_size_read(inode))
		goto out;

	spin_lock(&ip->i_flags_lock);
	isize = i_size_read(inode);
	if (iocb->ki_pos > isize) {
		spin_unlock(&ip->i_flags_lock);

		if (iocb->ki_flags & IOCB_NOWAIT)
			return -EAGAIN;

		if (!drained_dio) {
			if (*iolock == XFS_IOLOCK_SHARED) {
				xfs_iunlock(ip, *iolock);
				*iolock = XFS_IOLOCK_EXCL;
				xfs_ilock(ip, *iolock);
				iov_iter_reexpand(from, count);
			}
			 
			inode_dio_wait(inode);
			drained_dio = true;
			goto restart;
		}

		trace_xfs_zero_eof(ip, isize, iocb->ki_pos - isize);
		error = xfs_zero_range(ip, isize, iocb->ki_pos - isize, NULL);
		if (error)
			return error;
	} else
		spin_unlock(&ip->i_flags_lock);

out:
	return kiocb_modified(iocb);
}

static int
xfs_dio_write_end_io(
	struct kiocb		*iocb,
	ssize_t			size,
	int			error,
	unsigned		flags)
{
	struct inode		*inode = file_inode(iocb->ki_filp);
	struct xfs_inode	*ip = XFS_I(inode);
	loff_t			offset = iocb->ki_pos;
	unsigned int		nofs_flag;

	trace_xfs_end_io_direct_write(ip, offset, size);

	if (xfs_is_shutdown(ip->i_mount))
		return -EIO;

	if (error)
		return error;
	if (!size)
		return 0;

	 
	XFS_STATS_ADD(ip->i_mount, xs_write_bytes, size);

	 
	nofs_flag = memalloc_nofs_save();

	if (flags & IOMAP_DIO_COW) {
		error = xfs_reflink_end_cow(ip, offset, size);
		if (error)
			goto out;
	}

	 
	if (flags & IOMAP_DIO_UNWRITTEN) {
		error = xfs_iomap_write_unwritten(ip, offset, size, true);
		goto out;
	}

	 
	if (offset + size <= i_size_read(inode))
		goto out;

	spin_lock(&ip->i_flags_lock);
	if (offset + size > i_size_read(inode)) {
		i_size_write(inode, offset + size);
		spin_unlock(&ip->i_flags_lock);
		error = xfs_setfilesize(ip, offset, size);
	} else {
		spin_unlock(&ip->i_flags_lock);
	}

out:
	memalloc_nofs_restore(nofs_flag);
	return error;
}

static const struct iomap_dio_ops xfs_dio_write_ops = {
	.end_io		= xfs_dio_write_end_io,
};

 
static noinline ssize_t
xfs_file_dio_write_aligned(
	struct xfs_inode	*ip,
	struct kiocb		*iocb,
	struct iov_iter		*from)
{
	unsigned int		iolock = XFS_IOLOCK_SHARED;
	ssize_t			ret;

	ret = xfs_ilock_iocb(iocb, iolock);
	if (ret)
		return ret;
	ret = xfs_file_write_checks(iocb, from, &iolock);
	if (ret)
		goto out_unlock;

	 
	if (iolock == XFS_IOLOCK_EXCL) {
		xfs_ilock_demote(ip, XFS_IOLOCK_EXCL);
		iolock = XFS_IOLOCK_SHARED;
	}
	trace_xfs_file_direct_write(iocb, from);
	ret = iomap_dio_rw(iocb, from, &xfs_direct_write_iomap_ops,
			   &xfs_dio_write_ops, 0, NULL, 0);
out_unlock:
	if (iolock)
		xfs_iunlock(ip, iolock);
	return ret;
}

 
static noinline ssize_t
xfs_file_dio_write_unaligned(
	struct xfs_inode	*ip,
	struct kiocb		*iocb,
	struct iov_iter		*from)
{
	size_t			isize = i_size_read(VFS_I(ip));
	size_t			count = iov_iter_count(from);
	unsigned int		iolock = XFS_IOLOCK_SHARED;
	unsigned int		flags = IOMAP_DIO_OVERWRITE_ONLY;
	ssize_t			ret;

	 
	if (iocb->ki_pos > isize || iocb->ki_pos + count >= isize) {
		if (iocb->ki_flags & IOCB_NOWAIT)
			return -EAGAIN;
retry_exclusive:
		iolock = XFS_IOLOCK_EXCL;
		flags = IOMAP_DIO_FORCE_WAIT;
	}

	ret = xfs_ilock_iocb(iocb, iolock);
	if (ret)
		return ret;

	 
	if (xfs_is_cow_inode(ip)) {
		trace_xfs_reflink_bounce_dio_write(iocb, from);
		ret = -ENOTBLK;
		goto out_unlock;
	}

	ret = xfs_file_write_checks(iocb, from, &iolock);
	if (ret)
		goto out_unlock;

	 
	if (flags & IOMAP_DIO_FORCE_WAIT)
		inode_dio_wait(VFS_I(ip));

	trace_xfs_file_direct_write(iocb, from);
	ret = iomap_dio_rw(iocb, from, &xfs_direct_write_iomap_ops,
			   &xfs_dio_write_ops, flags, NULL, 0);

	 
	if (ret == -EAGAIN && !(iocb->ki_flags & IOCB_NOWAIT)) {
		ASSERT(flags & IOMAP_DIO_OVERWRITE_ONLY);
		xfs_iunlock(ip, iolock);
		goto retry_exclusive;
	}

out_unlock:
	if (iolock)
		xfs_iunlock(ip, iolock);
	return ret;
}

static ssize_t
xfs_file_dio_write(
	struct kiocb		*iocb,
	struct iov_iter		*from)
{
	struct xfs_inode	*ip = XFS_I(file_inode(iocb->ki_filp));
	struct xfs_buftarg      *target = xfs_inode_buftarg(ip);
	size_t			count = iov_iter_count(from);

	 
	if ((iocb->ki_pos | count) & target->bt_logical_sectormask)
		return -EINVAL;
	if ((iocb->ki_pos | count) & ip->i_mount->m_blockmask)
		return xfs_file_dio_write_unaligned(ip, iocb, from);
	return xfs_file_dio_write_aligned(ip, iocb, from);
}

static noinline ssize_t
xfs_file_dax_write(
	struct kiocb		*iocb,
	struct iov_iter		*from)
{
	struct inode		*inode = iocb->ki_filp->f_mapping->host;
	struct xfs_inode	*ip = XFS_I(inode);
	unsigned int		iolock = XFS_IOLOCK_EXCL;
	ssize_t			ret, error = 0;
	loff_t			pos;

	ret = xfs_ilock_iocb(iocb, iolock);
	if (ret)
		return ret;
	ret = xfs_file_write_checks(iocb, from, &iolock);
	if (ret)
		goto out;

	pos = iocb->ki_pos;

	trace_xfs_file_dax_write(iocb, from);
	ret = dax_iomap_rw(iocb, from, &xfs_dax_write_iomap_ops);
	if (ret > 0 && iocb->ki_pos > i_size_read(inode)) {
		i_size_write(inode, iocb->ki_pos);
		error = xfs_setfilesize(ip, pos, ret);
	}
out:
	if (iolock)
		xfs_iunlock(ip, iolock);
	if (error)
		return error;

	if (ret > 0) {
		XFS_STATS_ADD(ip->i_mount, xs_write_bytes, ret);

		 
		ret = generic_write_sync(iocb, ret);
	}
	return ret;
}

STATIC ssize_t
xfs_file_buffered_write(
	struct kiocb		*iocb,
	struct iov_iter		*from)
{
	struct inode		*inode = iocb->ki_filp->f_mapping->host;
	struct xfs_inode	*ip = XFS_I(inode);
	ssize_t			ret;
	bool			cleared_space = false;
	unsigned int		iolock;

write_retry:
	iolock = XFS_IOLOCK_EXCL;
	ret = xfs_ilock_iocb(iocb, iolock);
	if (ret)
		return ret;

	ret = xfs_file_write_checks(iocb, from, &iolock);
	if (ret)
		goto out;

	trace_xfs_file_buffered_write(iocb, from);
	ret = iomap_file_buffered_write(iocb, from,
			&xfs_buffered_write_iomap_ops);

	 
	if (ret == -EDQUOT && !cleared_space) {
		xfs_iunlock(ip, iolock);
		xfs_blockgc_free_quota(ip, XFS_ICWALK_FLAG_SYNC);
		cleared_space = true;
		goto write_retry;
	} else if (ret == -ENOSPC && !cleared_space) {
		struct xfs_icwalk	icw = {0};

		cleared_space = true;
		xfs_flush_inodes(ip->i_mount);

		xfs_iunlock(ip, iolock);
		icw.icw_flags = XFS_ICWALK_FLAG_SYNC;
		xfs_blockgc_free_space(ip->i_mount, &icw);
		goto write_retry;
	}

out:
	if (iolock)
		xfs_iunlock(ip, iolock);

	if (ret > 0) {
		XFS_STATS_ADD(ip->i_mount, xs_write_bytes, ret);
		 
		ret = generic_write_sync(iocb, ret);
	}
	return ret;
}

STATIC ssize_t
xfs_file_write_iter(
	struct kiocb		*iocb,
	struct iov_iter		*from)
{
	struct inode		*inode = iocb->ki_filp->f_mapping->host;
	struct xfs_inode	*ip = XFS_I(inode);
	ssize_t			ret;
	size_t			ocount = iov_iter_count(from);

	XFS_STATS_INC(ip->i_mount, xs_write_calls);

	if (ocount == 0)
		return 0;

	if (xfs_is_shutdown(ip->i_mount))
		return -EIO;

	if (IS_DAX(inode))
		return xfs_file_dax_write(iocb, from);

	if (iocb->ki_flags & IOCB_DIRECT) {
		 
		ret = xfs_file_dio_write(iocb, from);
		if (ret != -ENOTBLK)
			return ret;
	}

	return xfs_file_buffered_write(iocb, from);
}

static void
xfs_wait_dax_page(
	struct inode		*inode)
{
	struct xfs_inode        *ip = XFS_I(inode);

	xfs_iunlock(ip, XFS_MMAPLOCK_EXCL);
	schedule();
	xfs_ilock(ip, XFS_MMAPLOCK_EXCL);
}

int
xfs_break_dax_layouts(
	struct inode		*inode,
	bool			*retry)
{
	struct page		*page;

	ASSERT(xfs_isilocked(XFS_I(inode), XFS_MMAPLOCK_EXCL));

	page = dax_layout_busy_page(inode->i_mapping);
	if (!page)
		return 0;

	*retry = true;
	return ___wait_var_event(&page->_refcount,
			atomic_read(&page->_refcount) == 1, TASK_INTERRUPTIBLE,
			0, 0, xfs_wait_dax_page(inode));
}

int
xfs_break_layouts(
	struct inode		*inode,
	uint			*iolock,
	enum layout_break_reason reason)
{
	bool			retry;
	int			error;

	ASSERT(xfs_isilocked(XFS_I(inode), XFS_IOLOCK_SHARED|XFS_IOLOCK_EXCL));

	do {
		retry = false;
		switch (reason) {
		case BREAK_UNMAP:
			error = xfs_break_dax_layouts(inode, &retry);
			if (error || retry)
				break;
			fallthrough;
		case BREAK_WRITE:
			error = xfs_break_leased_layouts(inode, iolock, &retry);
			break;
		default:
			WARN_ON_ONCE(1);
			error = -EINVAL;
		}
	} while (error == 0 && retry);

	return error;
}

 
static inline bool xfs_file_sync_writes(struct file *filp)
{
	struct xfs_inode	*ip = XFS_I(file_inode(filp));

	if (xfs_has_wsync(ip->i_mount))
		return true;
	if (filp->f_flags & (__O_SYNC | O_DSYNC))
		return true;
	if (IS_SYNC(file_inode(filp)))
		return true;

	return false;
}

#define	XFS_FALLOC_FL_SUPPORTED						\
		(FALLOC_FL_KEEP_SIZE | FALLOC_FL_PUNCH_HOLE |		\
		 FALLOC_FL_COLLAPSE_RANGE | FALLOC_FL_ZERO_RANGE |	\
		 FALLOC_FL_INSERT_RANGE | FALLOC_FL_UNSHARE_RANGE)

STATIC long
xfs_file_fallocate(
	struct file		*file,
	int			mode,
	loff_t			offset,
	loff_t			len)
{
	struct inode		*inode = file_inode(file);
	struct xfs_inode	*ip = XFS_I(inode);
	long			error;
	uint			iolock = XFS_IOLOCK_EXCL | XFS_MMAPLOCK_EXCL;
	loff_t			new_size = 0;
	bool			do_file_insert = false;

	if (!S_ISREG(inode->i_mode))
		return -EINVAL;
	if (mode & ~XFS_FALLOC_FL_SUPPORTED)
		return -EOPNOTSUPP;

	xfs_ilock(ip, iolock);
	error = xfs_break_layouts(inode, &iolock, BREAK_UNMAP);
	if (error)
		goto out_unlock;

	 
	inode_dio_wait(inode);

	 
	if (mode & (FALLOC_FL_PUNCH_HOLE | FALLOC_FL_ZERO_RANGE |
		    FALLOC_FL_COLLAPSE_RANGE)) {
		error = xfs_flush_unmap_range(ip, offset, len);
		if (error)
			goto out_unlock;
	}

	error = file_modified(file);
	if (error)
		goto out_unlock;

	if (mode & FALLOC_FL_PUNCH_HOLE) {
		error = xfs_free_file_space(ip, offset, len);
		if (error)
			goto out_unlock;
	} else if (mode & FALLOC_FL_COLLAPSE_RANGE) {
		if (!xfs_is_falloc_aligned(ip, offset, len)) {
			error = -EINVAL;
			goto out_unlock;
		}

		 
		if (offset + len >= i_size_read(inode)) {
			error = -EINVAL;
			goto out_unlock;
		}

		new_size = i_size_read(inode) - len;

		error = xfs_collapse_file_space(ip, offset, len);
		if (error)
			goto out_unlock;
	} else if (mode & FALLOC_FL_INSERT_RANGE) {
		loff_t		isize = i_size_read(inode);

		if (!xfs_is_falloc_aligned(ip, offset, len)) {
			error = -EINVAL;
			goto out_unlock;
		}

		 
		if (inode->i_sb->s_maxbytes - isize < len) {
			error = -EFBIG;
			goto out_unlock;
		}
		new_size = isize + len;

		 
		if (offset >= isize) {
			error = -EINVAL;
			goto out_unlock;
		}
		do_file_insert = true;
	} else {
		if (!(mode & FALLOC_FL_KEEP_SIZE) &&
		    offset + len > i_size_read(inode)) {
			new_size = offset + len;
			error = inode_newsize_ok(inode, new_size);
			if (error)
				goto out_unlock;
		}

		if (mode & FALLOC_FL_ZERO_RANGE) {
			 
			unsigned int blksize = i_blocksize(inode);

			trace_xfs_zero_file_space(ip);

			error = xfs_free_file_space(ip, offset, len);
			if (error)
				goto out_unlock;

			len = round_up(offset + len, blksize) -
			      round_down(offset, blksize);
			offset = round_down(offset, blksize);
		} else if (mode & FALLOC_FL_UNSHARE_RANGE) {
			error = xfs_reflink_unshare(ip, offset, len);
			if (error)
				goto out_unlock;
		} else {
			 
			if (xfs_is_always_cow_inode(ip)) {
				error = -EOPNOTSUPP;
				goto out_unlock;
			}
		}

		if (!xfs_is_always_cow_inode(ip)) {
			error = xfs_alloc_file_space(ip, offset, len);
			if (error)
				goto out_unlock;
		}
	}

	 
	if (new_size) {
		struct iattr iattr;

		iattr.ia_valid = ATTR_SIZE;
		iattr.ia_size = new_size;
		error = xfs_vn_setattr_size(file_mnt_idmap(file),
					    file_dentry(file), &iattr);
		if (error)
			goto out_unlock;
	}

	 
	if (do_file_insert) {
		error = xfs_insert_file_space(ip, offset, len);
		if (error)
			goto out_unlock;
	}

	if (xfs_file_sync_writes(file))
		error = xfs_log_force_inode(ip);

out_unlock:
	xfs_iunlock(ip, iolock);
	return error;
}

STATIC int
xfs_file_fadvise(
	struct file	*file,
	loff_t		start,
	loff_t		end,
	int		advice)
{
	struct xfs_inode *ip = XFS_I(file_inode(file));
	int ret;
	int lockflags = 0;

	 
	if (advice == POSIX_FADV_WILLNEED) {
		lockflags = XFS_IOLOCK_SHARED;
		xfs_ilock(ip, lockflags);
	}
	ret = generic_fadvise(file, start, end, advice);
	if (lockflags)
		xfs_iunlock(ip, lockflags);
	return ret;
}

STATIC loff_t
xfs_file_remap_range(
	struct file		*file_in,
	loff_t			pos_in,
	struct file		*file_out,
	loff_t			pos_out,
	loff_t			len,
	unsigned int		remap_flags)
{
	struct inode		*inode_in = file_inode(file_in);
	struct xfs_inode	*src = XFS_I(inode_in);
	struct inode		*inode_out = file_inode(file_out);
	struct xfs_inode	*dest = XFS_I(inode_out);
	struct xfs_mount	*mp = src->i_mount;
	loff_t			remapped = 0;
	xfs_extlen_t		cowextsize;
	int			ret;

	if (remap_flags & ~(REMAP_FILE_DEDUP | REMAP_FILE_ADVISORY))
		return -EINVAL;

	if (!xfs_has_reflink(mp))
		return -EOPNOTSUPP;

	if (xfs_is_shutdown(mp))
		return -EIO;

	 
	ret = xfs_reflink_remap_prep(file_in, pos_in, file_out, pos_out,
			&len, remap_flags);
	if (ret || len == 0)
		return ret;

	trace_xfs_reflink_remap_range(src, pos_in, len, dest, pos_out);

	ret = xfs_reflink_remap_blocks(src, pos_in, dest, pos_out, len,
			&remapped);
	if (ret)
		goto out_unlock;

	 
	cowextsize = 0;
	if (pos_in == 0 && len == i_size_read(inode_in) &&
	    (src->i_diflags2 & XFS_DIFLAG2_COWEXTSIZE) &&
	    pos_out == 0 && len >= i_size_read(inode_out) &&
	    !(dest->i_diflags2 & XFS_DIFLAG2_COWEXTSIZE))
		cowextsize = src->i_cowextsize;

	ret = xfs_reflink_update_dest(dest, pos_out + len, cowextsize,
			remap_flags);
	if (ret)
		goto out_unlock;

	if (xfs_file_sync_writes(file_in) || xfs_file_sync_writes(file_out))
		xfs_log_force_inode(dest);
out_unlock:
	xfs_iunlock2_io_mmap(src, dest);
	if (ret)
		trace_xfs_reflink_remap_range_error(dest, ret, _RET_IP_);
	return remapped > 0 ? remapped : ret;
}

STATIC int
xfs_file_open(
	struct inode	*inode,
	struct file	*file)
{
	if (xfs_is_shutdown(XFS_M(inode->i_sb)))
		return -EIO;
	file->f_mode |= FMODE_NOWAIT | FMODE_BUF_RASYNC | FMODE_BUF_WASYNC |
			FMODE_DIO_PARALLEL_WRITE | FMODE_CAN_ODIRECT;
	return generic_file_open(inode, file);
}

STATIC int
xfs_dir_open(
	struct inode	*inode,
	struct file	*file)
{
	struct xfs_inode *ip = XFS_I(inode);
	unsigned int	mode;
	int		error;

	error = xfs_file_open(inode, file);
	if (error)
		return error;

	 
	mode = xfs_ilock_data_map_shared(ip);
	if (ip->i_df.if_nextents > 0)
		error = xfs_dir3_data_readahead(ip, 0, 0);
	xfs_iunlock(ip, mode);
	return error;
}

STATIC int
xfs_file_release(
	struct inode	*inode,
	struct file	*filp)
{
	return xfs_release(XFS_I(inode));
}

STATIC int
xfs_file_readdir(
	struct file	*file,
	struct dir_context *ctx)
{
	struct inode	*inode = file_inode(file);
	xfs_inode_t	*ip = XFS_I(inode);
	size_t		bufsize;

	 
	bufsize = (size_t)min_t(loff_t, XFS_READDIR_BUFSIZE, ip->i_disk_size);

	return xfs_readdir(NULL, ip, ctx, bufsize);
}

STATIC loff_t
xfs_file_llseek(
	struct file	*file,
	loff_t		offset,
	int		whence)
{
	struct inode		*inode = file->f_mapping->host;

	if (xfs_is_shutdown(XFS_I(inode)->i_mount))
		return -EIO;

	switch (whence) {
	default:
		return generic_file_llseek(file, offset, whence);
	case SEEK_HOLE:
		offset = iomap_seek_hole(inode, offset, &xfs_seek_iomap_ops);
		break;
	case SEEK_DATA:
		offset = iomap_seek_data(inode, offset, &xfs_seek_iomap_ops);
		break;
	}

	if (offset < 0)
		return offset;
	return vfs_setpos(file, offset, inode->i_sb->s_maxbytes);
}

#ifdef CONFIG_FS_DAX
static inline vm_fault_t
xfs_dax_fault(
	struct vm_fault		*vmf,
	unsigned int		order,
	bool			write_fault,
	pfn_t			*pfn)
{
	return dax_iomap_fault(vmf, order, pfn, NULL,
			(write_fault && !vmf->cow_page) ?
				&xfs_dax_write_iomap_ops :
				&xfs_read_iomap_ops);
}
#else
static inline vm_fault_t
xfs_dax_fault(
	struct vm_fault		*vmf,
	unsigned int		order,
	bool			write_fault,
	pfn_t			*pfn)
{
	ASSERT(0);
	return VM_FAULT_SIGBUS;
}
#endif

 
static vm_fault_t
__xfs_filemap_fault(
	struct vm_fault		*vmf,
	unsigned int		order,
	bool			write_fault)
{
	struct inode		*inode = file_inode(vmf->vma->vm_file);
	struct xfs_inode	*ip = XFS_I(inode);
	vm_fault_t		ret;

	trace_xfs_filemap_fault(ip, order, write_fault);

	if (write_fault) {
		sb_start_pagefault(inode->i_sb);
		file_update_time(vmf->vma->vm_file);
	}

	if (IS_DAX(inode)) {
		pfn_t pfn;

		xfs_ilock(XFS_I(inode), XFS_MMAPLOCK_SHARED);
		ret = xfs_dax_fault(vmf, order, write_fault, &pfn);
		if (ret & VM_FAULT_NEEDDSYNC)
			ret = dax_finish_sync_fault(vmf, order, pfn);
		xfs_iunlock(XFS_I(inode), XFS_MMAPLOCK_SHARED);
	} else {
		if (write_fault) {
			xfs_ilock(XFS_I(inode), XFS_MMAPLOCK_SHARED);
			ret = iomap_page_mkwrite(vmf,
					&xfs_page_mkwrite_iomap_ops);
			xfs_iunlock(XFS_I(inode), XFS_MMAPLOCK_SHARED);
		} else {
			ret = filemap_fault(vmf);
		}
	}

	if (write_fault)
		sb_end_pagefault(inode->i_sb);
	return ret;
}

static inline bool
xfs_is_write_fault(
	struct vm_fault		*vmf)
{
	return (vmf->flags & FAULT_FLAG_WRITE) &&
	       (vmf->vma->vm_flags & VM_SHARED);
}

static vm_fault_t
xfs_filemap_fault(
	struct vm_fault		*vmf)
{
	 
	return __xfs_filemap_fault(vmf, 0,
			IS_DAX(file_inode(vmf->vma->vm_file)) &&
			xfs_is_write_fault(vmf));
}

static vm_fault_t
xfs_filemap_huge_fault(
	struct vm_fault		*vmf,
	unsigned int		order)
{
	if (!IS_DAX(file_inode(vmf->vma->vm_file)))
		return VM_FAULT_FALLBACK;

	 
	return __xfs_filemap_fault(vmf, order,
			xfs_is_write_fault(vmf));
}

static vm_fault_t
xfs_filemap_page_mkwrite(
	struct vm_fault		*vmf)
{
	return __xfs_filemap_fault(vmf, 0, true);
}

 
static vm_fault_t
xfs_filemap_pfn_mkwrite(
	struct vm_fault		*vmf)
{

	return __xfs_filemap_fault(vmf, 0, true);
}

static const struct vm_operations_struct xfs_file_vm_ops = {
	.fault		= xfs_filemap_fault,
	.huge_fault	= xfs_filemap_huge_fault,
	.map_pages	= filemap_map_pages,
	.page_mkwrite	= xfs_filemap_page_mkwrite,
	.pfn_mkwrite	= xfs_filemap_pfn_mkwrite,
};

STATIC int
xfs_file_mmap(
	struct file		*file,
	struct vm_area_struct	*vma)
{
	struct inode		*inode = file_inode(file);
	struct xfs_buftarg	*target = xfs_inode_buftarg(XFS_I(inode));

	 
	if (!daxdev_mapping_supported(vma, target->bt_daxdev))
		return -EOPNOTSUPP;

	file_accessed(file);
	vma->vm_ops = &xfs_file_vm_ops;
	if (IS_DAX(inode))
		vm_flags_set(vma, VM_HUGEPAGE);
	return 0;
}

const struct file_operations xfs_file_operations = {
	.llseek		= xfs_file_llseek,
	.read_iter	= xfs_file_read_iter,
	.write_iter	= xfs_file_write_iter,
	.splice_read	= xfs_file_splice_read,
	.splice_write	= iter_file_splice_write,
	.iopoll		= iocb_bio_iopoll,
	.unlocked_ioctl	= xfs_file_ioctl,
#ifdef CONFIG_COMPAT
	.compat_ioctl	= xfs_file_compat_ioctl,
#endif
	.mmap		= xfs_file_mmap,
	.mmap_supported_flags = MAP_SYNC,
	.open		= xfs_file_open,
	.release	= xfs_file_release,
	.fsync		= xfs_file_fsync,
	.get_unmapped_area = thp_get_unmapped_area,
	.fallocate	= xfs_file_fallocate,
	.fadvise	= xfs_file_fadvise,
	.remap_file_range = xfs_file_remap_range,
};

const struct file_operations xfs_dir_file_operations = {
	.open		= xfs_dir_open,
	.read		= generic_read_dir,
	.iterate_shared	= xfs_file_readdir,
	.llseek		= generic_file_llseek,
	.unlocked_ioctl	= xfs_file_ioctl,
#ifdef CONFIG_COMPAT
	.compat_ioctl	= xfs_file_compat_ioctl,
#endif
	.fsync		= xfs_dir_fsync,
};
