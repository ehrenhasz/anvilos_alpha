
 
#include <linux/mount.h>
#include <linux/fsmap.h>
#include "xfs.h"
#include "xfs_fs.h"
#include "xfs_shared.h"
#include "xfs_format.h"
#include "xfs_log_format.h"
#include "xfs_trans_resv.h"
#include "xfs_mount.h"
#include "xfs_inode.h"
#include "xfs_iwalk.h"
#include "xfs_itable.h"
#include "xfs_fsops.h"
#include "xfs_rtalloc.h"
#include "xfs_da_format.h"
#include "xfs_da_btree.h"
#include "xfs_attr.h"
#include "xfs_ioctl.h"
#include "xfs_ioctl32.h"
#include "xfs_trace.h"
#include "xfs_sb.h"

#define  _NATIVE_IOC(cmd, type) \
	  _IOC(_IOC_DIR(cmd), _IOC_TYPE(cmd), _IOC_NR(cmd), sizeof(type))

#ifdef BROKEN_X86_ALIGNMENT
STATIC int
xfs_compat_ioc_fsgeometry_v1(
	struct xfs_mount	  *mp,
	compat_xfs_fsop_geom_v1_t __user *arg32)
{
	struct xfs_fsop_geom	  fsgeo;

	xfs_fs_geometry(mp, &fsgeo, 3);
	 
	if (copy_to_user(arg32, &fsgeo, sizeof(struct compat_xfs_fsop_geom_v1)))
		return -EFAULT;
	return 0;
}

STATIC int
xfs_compat_growfs_data_copyin(
	struct xfs_growfs_data	 *in,
	compat_xfs_growfs_data_t __user *arg32)
{
	if (get_user(in->newblocks, &arg32->newblocks) ||
	    get_user(in->imaxpct,   &arg32->imaxpct))
		return -EFAULT;
	return 0;
}

STATIC int
xfs_compat_growfs_rt_copyin(
	struct xfs_growfs_rt	 *in,
	compat_xfs_growfs_rt_t	__user *arg32)
{
	if (get_user(in->newblocks, &arg32->newblocks) ||
	    get_user(in->extsize,   &arg32->extsize))
		return -EFAULT;
	return 0;
}

STATIC int
xfs_fsinumbers_fmt_compat(
	struct xfs_ibulk		*breq,
	const struct xfs_inumbers	*ig)
{
	struct compat_xfs_inogrp __user	*p32 = breq->ubuffer;
	struct xfs_inogrp		ig1;
	struct xfs_inogrp		*igrp = &ig1;

	xfs_inumbers_to_inogrp(&ig1, ig);

	if (put_user(igrp->xi_startino,   &p32->xi_startino) ||
	    put_user(igrp->xi_alloccount, &p32->xi_alloccount) ||
	    put_user(igrp->xi_allocmask,  &p32->xi_allocmask))
		return -EFAULT;

	return xfs_ibulk_advance(breq, sizeof(struct compat_xfs_inogrp));
}

#else
#define xfs_fsinumbers_fmt_compat xfs_fsinumbers_fmt
#endif	 

STATIC int
xfs_ioctl32_bstime_copyin(
	xfs_bstime_t		*bstime,
	compat_xfs_bstime_t	__user *bstime32)
{
	old_time32_t		sec32;	 

	if (get_user(sec32,		&bstime32->tv_sec)	||
	    get_user(bstime->tv_nsec,	&bstime32->tv_nsec))
		return -EFAULT;
	bstime->tv_sec = sec32;
	return 0;
}

 
STATIC int
xfs_ioctl32_bstat_copyin(
	struct xfs_bstat		*bstat,
	struct compat_xfs_bstat	__user	*bstat32)
{
	if (get_user(bstat->bs_ino,	&bstat32->bs_ino)	||
	    get_user(bstat->bs_mode,	&bstat32->bs_mode)	||
	    get_user(bstat->bs_nlink,	&bstat32->bs_nlink)	||
	    get_user(bstat->bs_uid,	&bstat32->bs_uid)	||
	    get_user(bstat->bs_gid,	&bstat32->bs_gid)	||
	    get_user(bstat->bs_rdev,	&bstat32->bs_rdev)	||
	    get_user(bstat->bs_blksize,	&bstat32->bs_blksize)	||
	    get_user(bstat->bs_size,	&bstat32->bs_size)	||
	    xfs_ioctl32_bstime_copyin(&bstat->bs_atime, &bstat32->bs_atime) ||
	    xfs_ioctl32_bstime_copyin(&bstat->bs_mtime, &bstat32->bs_mtime) ||
	    xfs_ioctl32_bstime_copyin(&bstat->bs_ctime, &bstat32->bs_ctime) ||
	    get_user(bstat->bs_blocks,	&bstat32->bs_size)	||
	    get_user(bstat->bs_xflags,	&bstat32->bs_size)	||
	    get_user(bstat->bs_extsize,	&bstat32->bs_extsize)	||
	    get_user(bstat->bs_extents,	&bstat32->bs_extents)	||
	    get_user(bstat->bs_gen,	&bstat32->bs_gen)	||
	    get_user(bstat->bs_projid_lo, &bstat32->bs_projid_lo) ||
	    get_user(bstat->bs_projid_hi, &bstat32->bs_projid_hi) ||
	    get_user(bstat->bs_forkoff,	&bstat32->bs_forkoff)	||
	    get_user(bstat->bs_dmevmask, &bstat32->bs_dmevmask)	||
	    get_user(bstat->bs_dmstate,	&bstat32->bs_dmstate)	||
	    get_user(bstat->bs_aextents, &bstat32->bs_aextents))
		return -EFAULT;
	return 0;
}

 

STATIC int
xfs_bstime_store_compat(
	compat_xfs_bstime_t	__user *p32,
	const xfs_bstime_t	*p)
{
	__s32			sec32;

	sec32 = p->tv_sec;
	if (put_user(sec32, &p32->tv_sec) ||
	    put_user(p->tv_nsec, &p32->tv_nsec))
		return -EFAULT;
	return 0;
}

 
STATIC int
xfs_fsbulkstat_one_fmt_compat(
	struct xfs_ibulk		*breq,
	const struct xfs_bulkstat	*bstat)
{
	struct compat_xfs_bstat	__user	*p32 = breq->ubuffer;
	struct xfs_bstat		bs1;
	struct xfs_bstat		*buffer = &bs1;

	xfs_bulkstat_to_bstat(breq->mp, &bs1, bstat);

	if (put_user(buffer->bs_ino,	  &p32->bs_ino)		||
	    put_user(buffer->bs_mode,	  &p32->bs_mode)	||
	    put_user(buffer->bs_nlink,	  &p32->bs_nlink)	||
	    put_user(buffer->bs_uid,	  &p32->bs_uid)		||
	    put_user(buffer->bs_gid,	  &p32->bs_gid)		||
	    put_user(buffer->bs_rdev,	  &p32->bs_rdev)	||
	    put_user(buffer->bs_blksize,  &p32->bs_blksize)	||
	    put_user(buffer->bs_size,	  &p32->bs_size)	||
	    xfs_bstime_store_compat(&p32->bs_atime, &buffer->bs_atime) ||
	    xfs_bstime_store_compat(&p32->bs_mtime, &buffer->bs_mtime) ||
	    xfs_bstime_store_compat(&p32->bs_ctime, &buffer->bs_ctime) ||
	    put_user(buffer->bs_blocks,	  &p32->bs_blocks)	||
	    put_user(buffer->bs_xflags,	  &p32->bs_xflags)	||
	    put_user(buffer->bs_extsize,  &p32->bs_extsize)	||
	    put_user(buffer->bs_extents,  &p32->bs_extents)	||
	    put_user(buffer->bs_gen,	  &p32->bs_gen)		||
	    put_user(buffer->bs_projid,	  &p32->bs_projid)	||
	    put_user(buffer->bs_projid_hi,	&p32->bs_projid_hi)	||
	    put_user(buffer->bs_forkoff,  &p32->bs_forkoff)	||
	    put_user(buffer->bs_dmevmask, &p32->bs_dmevmask)	||
	    put_user(buffer->bs_dmstate,  &p32->bs_dmstate)	||
	    put_user(buffer->bs_aextents, &p32->bs_aextents))
		return -EFAULT;

	return xfs_ibulk_advance(breq, sizeof(struct compat_xfs_bstat));
}

 
STATIC int
xfs_compat_ioc_fsbulkstat(
	struct file		*file,
	unsigned int		  cmd,
	struct compat_xfs_fsop_bulkreq __user *p32)
{
	struct xfs_mount	*mp = XFS_I(file_inode(file))->i_mount;
	u32			addr;
	struct xfs_fsop_bulkreq	bulkreq;
	struct xfs_ibulk	breq = {
		.mp		= mp,
		.idmap		= file_mnt_idmap(file),
		.ocount		= 0,
	};
	xfs_ino_t		lastino;
	int			error;

	 
	inumbers_fmt_pf		inumbers_func = xfs_fsinumbers_fmt_compat;
	bulkstat_one_fmt_pf	bs_one_func = xfs_fsbulkstat_one_fmt_compat;

#ifdef CONFIG_X86_X32_ABI
	if (in_x32_syscall()) {
		 
		inumbers_func = xfs_fsinumbers_fmt;
		bs_one_func = xfs_fsbulkstat_one_fmt;
	}
#endif

	 
	 

	if (!capable(CAP_SYS_ADMIN))
		return -EPERM;

	if (xfs_is_shutdown(mp))
		return -EIO;

	if (get_user(addr, &p32->lastip))
		return -EFAULT;
	bulkreq.lastip = compat_ptr(addr);
	if (get_user(bulkreq.icount, &p32->icount) ||
	    get_user(addr, &p32->ubuffer))
		return -EFAULT;
	bulkreq.ubuffer = compat_ptr(addr);
	if (get_user(addr, &p32->ocount))
		return -EFAULT;
	bulkreq.ocount = compat_ptr(addr);

	if (copy_from_user(&lastino, bulkreq.lastip, sizeof(__s64)))
		return -EFAULT;

	if (bulkreq.icount <= 0)
		return -EINVAL;

	if (bulkreq.ubuffer == NULL)
		return -EINVAL;

	breq.ubuffer = bulkreq.ubuffer;
	breq.icount = bulkreq.icount;

	 
	if (cmd == XFS_IOC_FSINUMBERS_32) {
		breq.startino = lastino ? lastino + 1 : 0;
		error = xfs_inumbers(&breq, inumbers_func);
		lastino = breq.startino - 1;
	} else if (cmd == XFS_IOC_FSBULKSTAT_SINGLE_32) {
		breq.startino = lastino;
		breq.icount = 1;
		error = xfs_bulkstat_one(&breq, bs_one_func);
		lastino = breq.startino;
	} else if (cmd == XFS_IOC_FSBULKSTAT_32) {
		breq.startino = lastino ? lastino + 1 : 0;
		error = xfs_bulkstat(&breq, bs_one_func);
		lastino = breq.startino - 1;
	} else {
		error = -EINVAL;
	}
	if (error)
		return error;

	if (bulkreq.lastip != NULL &&
	    copy_to_user(bulkreq.lastip, &lastino, sizeof(xfs_ino_t)))
		return -EFAULT;

	if (bulkreq.ocount != NULL &&
	    copy_to_user(bulkreq.ocount, &breq.ocount, sizeof(__s32)))
		return -EFAULT;

	return 0;
}

STATIC int
xfs_compat_handlereq_copyin(
	xfs_fsop_handlereq_t		*hreq,
	compat_xfs_fsop_handlereq_t	__user *arg32)
{
	compat_xfs_fsop_handlereq_t	hreq32;

	if (copy_from_user(&hreq32, arg32, sizeof(compat_xfs_fsop_handlereq_t)))
		return -EFAULT;

	hreq->fd = hreq32.fd;
	hreq->path = compat_ptr(hreq32.path);
	hreq->oflags = hreq32.oflags;
	hreq->ihandle = compat_ptr(hreq32.ihandle);
	hreq->ihandlen = hreq32.ihandlen;
	hreq->ohandle = compat_ptr(hreq32.ohandle);
	hreq->ohandlen = compat_ptr(hreq32.ohandlen);

	return 0;
}

STATIC struct dentry *
xfs_compat_handlereq_to_dentry(
	struct file		*parfilp,
	compat_xfs_fsop_handlereq_t *hreq)
{
	return xfs_handle_to_dentry(parfilp,
			compat_ptr(hreq->ihandle), hreq->ihandlen);
}

STATIC int
xfs_compat_attrlist_by_handle(
	struct file		*parfilp,
	compat_xfs_fsop_attrlist_handlereq_t __user *p)
{
	compat_xfs_fsop_attrlist_handlereq_t al_hreq;
	struct dentry		*dentry;
	int			error;

	if (!capable(CAP_SYS_ADMIN))
		return -EPERM;
	if (copy_from_user(&al_hreq, p, sizeof(al_hreq)))
		return -EFAULT;

	dentry = xfs_compat_handlereq_to_dentry(parfilp, &al_hreq.hreq);
	if (IS_ERR(dentry))
		return PTR_ERR(dentry);

	error = xfs_ioc_attr_list(XFS_I(d_inode(dentry)),
			compat_ptr(al_hreq.buffer), al_hreq.buflen,
			al_hreq.flags, &p->pos);
	dput(dentry);
	return error;
}

STATIC int
xfs_compat_attrmulti_by_handle(
	struct file				*parfilp,
	void					__user *arg)
{
	int					error;
	compat_xfs_attr_multiop_t		*ops;
	compat_xfs_fsop_attrmulti_handlereq_t	am_hreq;
	struct dentry				*dentry;
	unsigned int				i, size;

	if (!capable(CAP_SYS_ADMIN))
		return -EPERM;
	if (copy_from_user(&am_hreq, arg,
			   sizeof(compat_xfs_fsop_attrmulti_handlereq_t)))
		return -EFAULT;

	 
	if (am_hreq.opcount >= INT_MAX / sizeof(compat_xfs_attr_multiop_t))
		return -E2BIG;

	dentry = xfs_compat_handlereq_to_dentry(parfilp, &am_hreq.hreq);
	if (IS_ERR(dentry))
		return PTR_ERR(dentry);

	error = -E2BIG;
	size = am_hreq.opcount * sizeof(compat_xfs_attr_multiop_t);
	if (!size || size > 16 * PAGE_SIZE)
		goto out_dput;

	ops = memdup_user(compat_ptr(am_hreq.ops), size);
	if (IS_ERR(ops)) {
		error = PTR_ERR(ops);
		goto out_dput;
	}

	error = 0;
	for (i = 0; i < am_hreq.opcount; i++) {
		ops[i].am_error = xfs_ioc_attrmulti_one(parfilp,
				d_inode(dentry), ops[i].am_opcode,
				compat_ptr(ops[i].am_attrname),
				compat_ptr(ops[i].am_attrvalue),
				&ops[i].am_length, ops[i].am_flags);
	}

	if (copy_to_user(compat_ptr(am_hreq.ops), ops, size))
		error = -EFAULT;

	kfree(ops);
 out_dput:
	dput(dentry);
	return error;
}

long
xfs_file_compat_ioctl(
	struct file		*filp,
	unsigned		cmd,
	unsigned long		p)
{
	struct inode		*inode = file_inode(filp);
	struct xfs_inode	*ip = XFS_I(inode);
	void			__user *arg = compat_ptr(p);
	int			error;

	trace_xfs_file_compat_ioctl(ip);

	switch (cmd) {
#if defined(BROKEN_X86_ALIGNMENT)
	case XFS_IOC_FSGEOMETRY_V1_32:
		return xfs_compat_ioc_fsgeometry_v1(ip->i_mount, arg);
	case XFS_IOC_FSGROWFSDATA_32: {
		struct xfs_growfs_data	in;

		if (xfs_compat_growfs_data_copyin(&in, arg))
			return -EFAULT;
		error = mnt_want_write_file(filp);
		if (error)
			return error;
		error = xfs_growfs_data(ip->i_mount, &in);
		mnt_drop_write_file(filp);
		return error;
	}
	case XFS_IOC_FSGROWFSRT_32: {
		struct xfs_growfs_rt	in;

		if (xfs_compat_growfs_rt_copyin(&in, arg))
			return -EFAULT;
		error = mnt_want_write_file(filp);
		if (error)
			return error;
		error = xfs_growfs_rt(ip->i_mount, &in);
		mnt_drop_write_file(filp);
		return error;
	}
#endif
	 
	case XFS_IOC_GETVERSION_32:
		cmd = _NATIVE_IOC(cmd, long);
		return xfs_file_ioctl(filp, cmd, p);
	case XFS_IOC_SWAPEXT_32: {
		struct xfs_swapext	  sxp;
		struct compat_xfs_swapext __user *sxu = arg;

		 
		if (copy_from_user(&sxp, sxu,
				   offsetof(struct xfs_swapext, sx_stat)) ||
		    xfs_ioctl32_bstat_copyin(&sxp.sx_stat, &sxu->sx_stat))
			return -EFAULT;
		error = mnt_want_write_file(filp);
		if (error)
			return error;
		error = xfs_ioc_swapext(&sxp);
		mnt_drop_write_file(filp);
		return error;
	}
	case XFS_IOC_FSBULKSTAT_32:
	case XFS_IOC_FSBULKSTAT_SINGLE_32:
	case XFS_IOC_FSINUMBERS_32:
		return xfs_compat_ioc_fsbulkstat(filp, cmd, arg);
	case XFS_IOC_FD_TO_HANDLE_32:
	case XFS_IOC_PATH_TO_HANDLE_32:
	case XFS_IOC_PATH_TO_FSHANDLE_32: {
		struct xfs_fsop_handlereq	hreq;

		if (xfs_compat_handlereq_copyin(&hreq, arg))
			return -EFAULT;
		cmd = _NATIVE_IOC(cmd, struct xfs_fsop_handlereq);
		return xfs_find_handle(cmd, &hreq);
	}
	case XFS_IOC_OPEN_BY_HANDLE_32: {
		struct xfs_fsop_handlereq	hreq;

		if (xfs_compat_handlereq_copyin(&hreq, arg))
			return -EFAULT;
		return xfs_open_by_handle(filp, &hreq);
	}
	case XFS_IOC_READLINK_BY_HANDLE_32: {
		struct xfs_fsop_handlereq	hreq;

		if (xfs_compat_handlereq_copyin(&hreq, arg))
			return -EFAULT;
		return xfs_readlink_by_handle(filp, &hreq);
	}
	case XFS_IOC_ATTRLIST_BY_HANDLE_32:
		return xfs_compat_attrlist_by_handle(filp, arg);
	case XFS_IOC_ATTRMULTI_BY_HANDLE_32:
		return xfs_compat_attrmulti_by_handle(filp, arg);
	default:
		 
		return xfs_file_ioctl(filp, cmd, (unsigned long)arg);
	}
}
