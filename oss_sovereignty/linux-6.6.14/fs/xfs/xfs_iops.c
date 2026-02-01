
 
#include "xfs.h"
#include "xfs_fs.h"
#include "xfs_shared.h"
#include "xfs_format.h"
#include "xfs_log_format.h"
#include "xfs_trans_resv.h"
#include "xfs_mount.h"
#include "xfs_inode.h"
#include "xfs_acl.h"
#include "xfs_quota.h"
#include "xfs_da_format.h"
#include "xfs_da_btree.h"
#include "xfs_attr.h"
#include "xfs_trans.h"
#include "xfs_trace.h"
#include "xfs_icache.h"
#include "xfs_symlink.h"
#include "xfs_dir2.h"
#include "xfs_iomap.h"
#include "xfs_error.h"
#include "xfs_ioctl.h"
#include "xfs_xattr.h"

#include <linux/posix_acl.h>
#include <linux/security.h>
#include <linux/iversion.h>
#include <linux/fiemap.h>

 
static struct lock_class_key xfs_nondir_ilock_class;
static struct lock_class_key xfs_dir_ilock_class;

static int
xfs_initxattrs(
	struct inode		*inode,
	const struct xattr	*xattr_array,
	void			*fs_info)
{
	const struct xattr	*xattr;
	struct xfs_inode	*ip = XFS_I(inode);
	int			error = 0;

	for (xattr = xattr_array; xattr->name != NULL; xattr++) {
		struct xfs_da_args	args = {
			.dp		= ip,
			.attr_filter	= XFS_ATTR_SECURE,
			.name		= xattr->name,
			.namelen	= strlen(xattr->name),
			.value		= xattr->value,
			.valuelen	= xattr->value_len,
		};
		error = xfs_attr_change(&args);
		if (error < 0)
			break;
	}
	return error;
}

 
int
xfs_inode_init_security(
	struct inode	*inode,
	struct inode	*dir,
	const struct qstr *qstr)
{
	return security_inode_init_security(inode, dir, qstr,
					     &xfs_initxattrs, NULL);
}

static void
xfs_dentry_to_name(
	struct xfs_name	*namep,
	struct dentry	*dentry)
{
	namep->name = dentry->d_name.name;
	namep->len = dentry->d_name.len;
	namep->type = XFS_DIR3_FT_UNKNOWN;
}

static int
xfs_dentry_mode_to_name(
	struct xfs_name	*namep,
	struct dentry	*dentry,
	int		mode)
{
	namep->name = dentry->d_name.name;
	namep->len = dentry->d_name.len;
	namep->type = xfs_mode_to_ftype(mode);

	if (unlikely(namep->type == XFS_DIR3_FT_UNKNOWN))
		return -EFSCORRUPTED;

	return 0;
}

STATIC void
xfs_cleanup_inode(
	struct inode	*dir,
	struct inode	*inode,
	struct dentry	*dentry)
{
	struct xfs_name	teardown;

	 
	xfs_dentry_to_name(&teardown, dentry);

	xfs_remove(XFS_I(dir), &teardown, XFS_I(inode));
}

 
static inline bool
xfs_create_need_xattr(
	struct inode	*dir,
	struct posix_acl *default_acl,
	struct posix_acl *acl)
{
	if (acl)
		return true;
	if (default_acl)
		return true;
#if IS_ENABLED(CONFIG_SECURITY)
	if (dir->i_sb->s_security)
		return true;
#endif
	return false;
}


STATIC int
xfs_generic_create(
	struct mnt_idmap	*idmap,
	struct inode		*dir,
	struct dentry		*dentry,
	umode_t			mode,
	dev_t			rdev,
	struct file		*tmpfile)	 
{
	struct inode	*inode;
	struct xfs_inode *ip = NULL;
	struct posix_acl *default_acl, *acl;
	struct xfs_name	name;
	int		error;

	 
	if (S_ISCHR(mode) || S_ISBLK(mode)) {
		if (unlikely(!sysv_valid_dev(rdev) || MAJOR(rdev) & ~0x1ff))
			return -EINVAL;
	} else {
		rdev = 0;
	}

	error = posix_acl_create(dir, &mode, &default_acl, &acl);
	if (error)
		return error;

	 
	error = xfs_dentry_mode_to_name(&name, dentry, mode);
	if (unlikely(error))
		goto out_free_acl;

	if (!tmpfile) {
		error = xfs_create(idmap, XFS_I(dir), &name, mode, rdev,
				xfs_create_need_xattr(dir, default_acl, acl),
				&ip);
	} else {
		error = xfs_create_tmpfile(idmap, XFS_I(dir), mode, &ip);
	}
	if (unlikely(error))
		goto out_free_acl;

	inode = VFS_I(ip);

	error = xfs_inode_init_security(inode, dir, &dentry->d_name);
	if (unlikely(error))
		goto out_cleanup_inode;

	if (default_acl) {
		error = __xfs_set_acl(inode, default_acl, ACL_TYPE_DEFAULT);
		if (error)
			goto out_cleanup_inode;
	}
	if (acl) {
		error = __xfs_set_acl(inode, acl, ACL_TYPE_ACCESS);
		if (error)
			goto out_cleanup_inode;
	}

	xfs_setup_iops(ip);

	if (tmpfile) {
		 
		set_nlink(inode, 1);
		d_tmpfile(tmpfile, inode);
	} else
		d_instantiate(dentry, inode);

	xfs_finish_inode_setup(ip);

 out_free_acl:
	posix_acl_release(default_acl);
	posix_acl_release(acl);
	return error;

 out_cleanup_inode:
	xfs_finish_inode_setup(ip);
	if (!tmpfile)
		xfs_cleanup_inode(dir, inode, dentry);
	xfs_irele(ip);
	goto out_free_acl;
}

STATIC int
xfs_vn_mknod(
	struct mnt_idmap	*idmap,
	struct inode		*dir,
	struct dentry		*dentry,
	umode_t			mode,
	dev_t			rdev)
{
	return xfs_generic_create(idmap, dir, dentry, mode, rdev, NULL);
}

STATIC int
xfs_vn_create(
	struct mnt_idmap	*idmap,
	struct inode		*dir,
	struct dentry		*dentry,
	umode_t			mode,
	bool			flags)
{
	return xfs_generic_create(idmap, dir, dentry, mode, 0, NULL);
}

STATIC int
xfs_vn_mkdir(
	struct mnt_idmap	*idmap,
	struct inode		*dir,
	struct dentry		*dentry,
	umode_t			mode)
{
	return xfs_generic_create(idmap, dir, dentry, mode | S_IFDIR, 0, NULL);
}

STATIC struct dentry *
xfs_vn_lookup(
	struct inode	*dir,
	struct dentry	*dentry,
	unsigned int flags)
{
	struct inode *inode;
	struct xfs_inode *cip;
	struct xfs_name	name;
	int		error;

	if (dentry->d_name.len >= MAXNAMELEN)
		return ERR_PTR(-ENAMETOOLONG);

	xfs_dentry_to_name(&name, dentry);
	error = xfs_lookup(XFS_I(dir), &name, &cip, NULL);
	if (likely(!error))
		inode = VFS_I(cip);
	else if (likely(error == -ENOENT))
		inode = NULL;
	else
		inode = ERR_PTR(error);
	return d_splice_alias(inode, dentry);
}

STATIC struct dentry *
xfs_vn_ci_lookup(
	struct inode	*dir,
	struct dentry	*dentry,
	unsigned int flags)
{
	struct xfs_inode *ip;
	struct xfs_name	xname;
	struct xfs_name ci_name;
	struct qstr	dname;
	int		error;

	if (dentry->d_name.len >= MAXNAMELEN)
		return ERR_PTR(-ENAMETOOLONG);

	xfs_dentry_to_name(&xname, dentry);
	error = xfs_lookup(XFS_I(dir), &xname, &ip, &ci_name);
	if (unlikely(error)) {
		if (unlikely(error != -ENOENT))
			return ERR_PTR(error);
		 
		return NULL;
	}

	 
	if (!ci_name.name)
		return d_splice_alias(VFS_I(ip), dentry);

	 
	dname.name = ci_name.name;
	dname.len = ci_name.len;
	dentry = d_add_ci(dentry, VFS_I(ip), &dname);
	kmem_free(ci_name.name);
	return dentry;
}

STATIC int
xfs_vn_link(
	struct dentry	*old_dentry,
	struct inode	*dir,
	struct dentry	*dentry)
{
	struct inode	*inode = d_inode(old_dentry);
	struct xfs_name	name;
	int		error;

	error = xfs_dentry_mode_to_name(&name, dentry, inode->i_mode);
	if (unlikely(error))
		return error;

	error = xfs_link(XFS_I(dir), XFS_I(inode), &name);
	if (unlikely(error))
		return error;

	ihold(inode);
	d_instantiate(dentry, inode);
	return 0;
}

STATIC int
xfs_vn_unlink(
	struct inode	*dir,
	struct dentry	*dentry)
{
	struct xfs_name	name;
	int		error;

	xfs_dentry_to_name(&name, dentry);

	error = xfs_remove(XFS_I(dir), &name, XFS_I(d_inode(dentry)));
	if (error)
		return error;

	 
	if (xfs_has_asciici(XFS_M(dir->i_sb)))
		d_invalidate(dentry);
	return 0;
}

STATIC int
xfs_vn_symlink(
	struct mnt_idmap	*idmap,
	struct inode		*dir,
	struct dentry		*dentry,
	const char		*symname)
{
	struct inode	*inode;
	struct xfs_inode *cip = NULL;
	struct xfs_name	name;
	int		error;
	umode_t		mode;

	mode = S_IFLNK |
		(irix_symlink_mode ? 0777 & ~current_umask() : S_IRWXUGO);
	error = xfs_dentry_mode_to_name(&name, dentry, mode);
	if (unlikely(error))
		goto out;

	error = xfs_symlink(idmap, XFS_I(dir), &name, symname, mode, &cip);
	if (unlikely(error))
		goto out;

	inode = VFS_I(cip);

	error = xfs_inode_init_security(inode, dir, &dentry->d_name);
	if (unlikely(error))
		goto out_cleanup_inode;

	xfs_setup_iops(cip);

	d_instantiate(dentry, inode);
	xfs_finish_inode_setup(cip);
	return 0;

 out_cleanup_inode:
	xfs_finish_inode_setup(cip);
	xfs_cleanup_inode(dir, inode, dentry);
	xfs_irele(cip);
 out:
	return error;
}

STATIC int
xfs_vn_rename(
	struct mnt_idmap	*idmap,
	struct inode		*odir,
	struct dentry		*odentry,
	struct inode		*ndir,
	struct dentry		*ndentry,
	unsigned int		flags)
{
	struct inode	*new_inode = d_inode(ndentry);
	int		omode = 0;
	int		error;
	struct xfs_name	oname;
	struct xfs_name	nname;

	if (flags & ~(RENAME_NOREPLACE | RENAME_EXCHANGE | RENAME_WHITEOUT))
		return -EINVAL;

	 
	if (flags & RENAME_EXCHANGE)
		omode = d_inode(ndentry)->i_mode;

	error = xfs_dentry_mode_to_name(&oname, odentry, omode);
	if (omode && unlikely(error))
		return error;

	error = xfs_dentry_mode_to_name(&nname, ndentry,
					d_inode(odentry)->i_mode);
	if (unlikely(error))
		return error;

	return xfs_rename(idmap, XFS_I(odir), &oname,
			  XFS_I(d_inode(odentry)), XFS_I(ndir), &nname,
			  new_inode ? XFS_I(new_inode) : NULL, flags);
}

 
STATIC const char *
xfs_vn_get_link(
	struct dentry		*dentry,
	struct inode		*inode,
	struct delayed_call	*done)
{
	char			*link;
	int			error = -ENOMEM;

	if (!dentry)
		return ERR_PTR(-ECHILD);

	link = kmalloc(XFS_SYMLINK_MAXLEN+1, GFP_KERNEL);
	if (!link)
		goto out_err;

	error = xfs_readlink(XFS_I(d_inode(dentry)), link);
	if (unlikely(error))
		goto out_kfree;

	set_delayed_call(done, kfree_link, link);
	return link;

 out_kfree:
	kfree(link);
 out_err:
	return ERR_PTR(error);
}

static uint32_t
xfs_stat_blksize(
	struct xfs_inode	*ip)
{
	struct xfs_mount	*mp = ip->i_mount;

	 
	if (XFS_IS_REALTIME_INODE(ip))
		return XFS_FSB_TO_B(mp, xfs_get_extsz_hint(ip));

	 
	if (xfs_has_large_iosize(mp)) {
		if (mp->m_swidth)
			return XFS_FSB_TO_B(mp, mp->m_swidth);
		if (xfs_has_allocsize(mp))
			return 1U << mp->m_allocsize_log;
	}

	return PAGE_SIZE;
}

STATIC int
xfs_vn_getattr(
	struct mnt_idmap	*idmap,
	const struct path	*path,
	struct kstat		*stat,
	u32			request_mask,
	unsigned int		query_flags)
{
	struct inode		*inode = d_inode(path->dentry);
	struct xfs_inode	*ip = XFS_I(inode);
	struct xfs_mount	*mp = ip->i_mount;
	vfsuid_t		vfsuid = i_uid_into_vfsuid(idmap, inode);
	vfsgid_t		vfsgid = i_gid_into_vfsgid(idmap, inode);

	trace_xfs_getattr(ip);

	if (xfs_is_shutdown(mp))
		return -EIO;

	stat->size = XFS_ISIZE(ip);
	stat->dev = inode->i_sb->s_dev;
	stat->mode = inode->i_mode;
	stat->nlink = inode->i_nlink;
	stat->uid = vfsuid_into_kuid(vfsuid);
	stat->gid = vfsgid_into_kgid(vfsgid);
	stat->ino = ip->i_ino;
	stat->atime = inode->i_atime;
	stat->mtime = inode->i_mtime;
	stat->ctime = inode_get_ctime(inode);
	stat->blocks = XFS_FSB_TO_BB(mp, ip->i_nblocks + ip->i_delayed_blks);

	if (xfs_has_v3inodes(mp)) {
		if (request_mask & STATX_BTIME) {
			stat->result_mask |= STATX_BTIME;
			stat->btime = ip->i_crtime;
		}
	}

	if ((request_mask & STATX_CHANGE_COOKIE) && IS_I_VERSION(inode)) {
		stat->change_cookie = inode_query_iversion(inode);
		stat->result_mask |= STATX_CHANGE_COOKIE;
	}

	 
	if (ip->i_diflags & XFS_DIFLAG_IMMUTABLE)
		stat->attributes |= STATX_ATTR_IMMUTABLE;
	if (ip->i_diflags & XFS_DIFLAG_APPEND)
		stat->attributes |= STATX_ATTR_APPEND;
	if (ip->i_diflags & XFS_DIFLAG_NODUMP)
		stat->attributes |= STATX_ATTR_NODUMP;

	stat->attributes_mask |= (STATX_ATTR_IMMUTABLE |
				  STATX_ATTR_APPEND |
				  STATX_ATTR_NODUMP);

	switch (inode->i_mode & S_IFMT) {
	case S_IFBLK:
	case S_IFCHR:
		stat->blksize = BLKDEV_IOSIZE;
		stat->rdev = inode->i_rdev;
		break;
	case S_IFREG:
		if (request_mask & STATX_DIOALIGN) {
			struct xfs_buftarg	*target = xfs_inode_buftarg(ip);
			struct block_device	*bdev = target->bt_bdev;

			stat->result_mask |= STATX_DIOALIGN;
			stat->dio_mem_align = bdev_dma_alignment(bdev) + 1;
			stat->dio_offset_align = bdev_logical_block_size(bdev);
		}
		fallthrough;
	default:
		stat->blksize = xfs_stat_blksize(ip);
		stat->rdev = 0;
		break;
	}

	return 0;
}

static int
xfs_vn_change_ok(
	struct mnt_idmap	*idmap,
	struct dentry		*dentry,
	struct iattr		*iattr)
{
	struct xfs_mount	*mp = XFS_I(d_inode(dentry))->i_mount;

	if (xfs_is_readonly(mp))
		return -EROFS;

	if (xfs_is_shutdown(mp))
		return -EIO;

	return setattr_prepare(idmap, dentry, iattr);
}

 
static int
xfs_setattr_nonsize(
	struct mnt_idmap	*idmap,
	struct dentry		*dentry,
	struct xfs_inode	*ip,
	struct iattr		*iattr)
{
	xfs_mount_t		*mp = ip->i_mount;
	struct inode		*inode = VFS_I(ip);
	int			mask = iattr->ia_valid;
	xfs_trans_t		*tp;
	int			error;
	kuid_t			uid = GLOBAL_ROOT_UID;
	kgid_t			gid = GLOBAL_ROOT_GID;
	struct xfs_dquot	*udqp = NULL, *gdqp = NULL;
	struct xfs_dquot	*old_udqp = NULL, *old_gdqp = NULL;

	ASSERT((mask & ATTR_SIZE) == 0);

	 
	if (XFS_IS_QUOTA_ON(mp) && (mask & (ATTR_UID|ATTR_GID))) {
		uint	qflags = 0;

		if ((mask & ATTR_UID) && XFS_IS_UQUOTA_ON(mp)) {
			uid = from_vfsuid(idmap, i_user_ns(inode),
					  iattr->ia_vfsuid);
			qflags |= XFS_QMOPT_UQUOTA;
		} else {
			uid = inode->i_uid;
		}
		if ((mask & ATTR_GID) && XFS_IS_GQUOTA_ON(mp)) {
			gid = from_vfsgid(idmap, i_user_ns(inode),
					  iattr->ia_vfsgid);
			qflags |= XFS_QMOPT_GQUOTA;
		}  else {
			gid = inode->i_gid;
		}

		 
		ASSERT(udqp == NULL);
		ASSERT(gdqp == NULL);
		error = xfs_qm_vop_dqalloc(ip, uid, gid, ip->i_projid,
					   qflags, &udqp, &gdqp, NULL);
		if (error)
			return error;
	}

	error = xfs_trans_alloc_ichange(ip, udqp, gdqp, NULL,
			has_capability_noaudit(current, CAP_FOWNER), &tp);
	if (error)
		goto out_dqrele;

	 
	if (XFS_IS_UQUOTA_ON(mp) &&
	    i_uid_needs_update(idmap, iattr, inode)) {
		ASSERT(udqp);
		old_udqp = xfs_qm_vop_chown(tp, ip, &ip->i_udquot, udqp);
	}
	if (XFS_IS_GQUOTA_ON(mp) &&
	    i_gid_needs_update(idmap, iattr, inode)) {
		ASSERT(xfs_has_pquotino(mp) || !XFS_IS_PQUOTA_ON(mp));
		ASSERT(gdqp);
		old_gdqp = xfs_qm_vop_chown(tp, ip, &ip->i_gdquot, gdqp);
	}

	setattr_copy(idmap, inode, iattr);
	xfs_trans_log_inode(tp, ip, XFS_ILOG_CORE);

	XFS_STATS_INC(mp, xs_ig_attrchg);

	if (xfs_has_wsync(mp))
		xfs_trans_set_sync(tp);
	error = xfs_trans_commit(tp);

	 
	xfs_qm_dqrele(old_udqp);
	xfs_qm_dqrele(old_gdqp);
	xfs_qm_dqrele(udqp);
	xfs_qm_dqrele(gdqp);

	if (error)
		return error;

	 
	if (mask & ATTR_MODE) {
		error = posix_acl_chmod(idmap, dentry, inode->i_mode);
		if (error)
			return error;
	}

	return 0;

out_dqrele:
	xfs_qm_dqrele(udqp);
	xfs_qm_dqrele(gdqp);
	return error;
}

 
STATIC int
xfs_setattr_size(
	struct mnt_idmap	*idmap,
	struct dentry		*dentry,
	struct xfs_inode	*ip,
	struct iattr		*iattr)
{
	struct xfs_mount	*mp = ip->i_mount;
	struct inode		*inode = VFS_I(ip);
	xfs_off_t		oldsize, newsize;
	struct xfs_trans	*tp;
	int			error;
	uint			lock_flags = 0;
	bool			did_zeroing = false;

	ASSERT(xfs_isilocked(ip, XFS_IOLOCK_EXCL));
	ASSERT(xfs_isilocked(ip, XFS_MMAPLOCK_EXCL));
	ASSERT(S_ISREG(inode->i_mode));
	ASSERT((iattr->ia_valid & (ATTR_UID|ATTR_GID|ATTR_ATIME|ATTR_ATIME_SET|
		ATTR_MTIME_SET|ATTR_TIMES_SET)) == 0);

	oldsize = inode->i_size;
	newsize = iattr->ia_size;

	 
	if (newsize == 0 && oldsize == 0 && ip->i_df.if_nextents == 0) {
		if (!(iattr->ia_valid & (ATTR_CTIME|ATTR_MTIME)))
			return 0;

		 
		iattr->ia_valid &= ~ATTR_SIZE;
		return xfs_setattr_nonsize(idmap, dentry, ip, iattr);
	}

	 
	error = xfs_qm_dqattach(ip);
	if (error)
		return error;

	 
	inode_dio_wait(inode);

	 
	if (newsize > oldsize) {
		trace_xfs_zero_eof(ip, oldsize, newsize - oldsize);
		error = xfs_zero_range(ip, oldsize, newsize - oldsize,
				&did_zeroing);
	} else {
		 
		error = filemap_write_and_wait_range(inode->i_mapping, newsize,
						     newsize);
		if (error)
			return error;
		error = xfs_truncate_page(ip, newsize, &did_zeroing);
	}

	if (error)
		return error;

	 
	truncate_setsize(inode, newsize);

	 
	if (did_zeroing ||
	    (newsize > ip->i_disk_size && oldsize != ip->i_disk_size)) {
		error = filemap_write_and_wait_range(VFS_I(ip)->i_mapping,
						ip->i_disk_size, newsize - 1);
		if (error)
			return error;
	}

	error = xfs_trans_alloc(mp, &M_RES(mp)->tr_itruncate, 0, 0, 0, &tp);
	if (error)
		return error;

	lock_flags |= XFS_ILOCK_EXCL;
	xfs_ilock(ip, XFS_ILOCK_EXCL);
	xfs_trans_ijoin(tp, ip, 0);

	 
	if (newsize != oldsize &&
	    !(iattr->ia_valid & (ATTR_CTIME | ATTR_MTIME))) {
		iattr->ia_ctime = iattr->ia_mtime =
			current_time(inode);
		iattr->ia_valid |= ATTR_CTIME | ATTR_MTIME;
	}

	 
	ip->i_disk_size = newsize;
	xfs_trans_log_inode(tp, ip, XFS_ILOG_CORE);

	if (newsize <= oldsize) {
		error = xfs_itruncate_extents(&tp, ip, XFS_DATA_FORK, newsize);
		if (error)
			goto out_trans_cancel;

		 
		xfs_iflags_set(ip, XFS_ITRUNCATED);

		 
		xfs_inode_clear_eofblocks_tag(ip);
	}

	ASSERT(!(iattr->ia_valid & (ATTR_UID | ATTR_GID)));
	setattr_copy(idmap, inode, iattr);
	xfs_trans_log_inode(tp, ip, XFS_ILOG_CORE);

	XFS_STATS_INC(mp, xs_ig_attrchg);

	if (xfs_has_wsync(mp))
		xfs_trans_set_sync(tp);

	error = xfs_trans_commit(tp);
out_unlock:
	if (lock_flags)
		xfs_iunlock(ip, lock_flags);
	return error;

out_trans_cancel:
	xfs_trans_cancel(tp);
	goto out_unlock;
}

int
xfs_vn_setattr_size(
	struct mnt_idmap	*idmap,
	struct dentry		*dentry,
	struct iattr		*iattr)
{
	struct xfs_inode	*ip = XFS_I(d_inode(dentry));
	int error;

	trace_xfs_setattr(ip);

	error = xfs_vn_change_ok(idmap, dentry, iattr);
	if (error)
		return error;
	return xfs_setattr_size(idmap, dentry, ip, iattr);
}

STATIC int
xfs_vn_setattr(
	struct mnt_idmap	*idmap,
	struct dentry		*dentry,
	struct iattr		*iattr)
{
	struct inode		*inode = d_inode(dentry);
	struct xfs_inode	*ip = XFS_I(inode);
	int			error;

	if (iattr->ia_valid & ATTR_SIZE) {
		uint			iolock;

		xfs_ilock(ip, XFS_MMAPLOCK_EXCL);
		iolock = XFS_IOLOCK_EXCL | XFS_MMAPLOCK_EXCL;

		error = xfs_break_layouts(inode, &iolock, BREAK_UNMAP);
		if (error) {
			xfs_iunlock(ip, XFS_MMAPLOCK_EXCL);
			return error;
		}

		error = xfs_vn_setattr_size(idmap, dentry, iattr);
		xfs_iunlock(ip, XFS_MMAPLOCK_EXCL);
	} else {
		trace_xfs_setattr(ip);

		error = xfs_vn_change_ok(idmap, dentry, iattr);
		if (!error)
			error = xfs_setattr_nonsize(idmap, dentry, ip, iattr);
	}

	return error;
}

STATIC int
xfs_vn_update_time(
	struct inode		*inode,
	int			flags)
{
	struct xfs_inode	*ip = XFS_I(inode);
	struct xfs_mount	*mp = ip->i_mount;
	int			log_flags = XFS_ILOG_TIMESTAMP;
	struct xfs_trans	*tp;
	int			error;
	struct timespec64	now;

	trace_xfs_update_time(ip);

	if (inode->i_sb->s_flags & SB_LAZYTIME) {
		if (!((flags & S_VERSION) &&
		      inode_maybe_inc_iversion(inode, false))) {
			generic_update_time(inode, flags);
			return 0;
		}

		 
		log_flags |= XFS_ILOG_CORE;
	}

	error = xfs_trans_alloc(mp, &M_RES(mp)->tr_fsyncts, 0, 0, 0, &tp);
	if (error)
		return error;

	xfs_ilock(ip, XFS_ILOCK_EXCL);
	if (flags & (S_CTIME|S_MTIME))
		now = inode_set_ctime_current(inode);
	else
		now = current_time(inode);

	if (flags & S_MTIME)
		inode->i_mtime = now;
	if (flags & S_ATIME)
		inode->i_atime = now;

	xfs_trans_ijoin(tp, ip, XFS_ILOCK_EXCL);
	xfs_trans_log_inode(tp, ip, log_flags);
	return xfs_trans_commit(tp);
}

STATIC int
xfs_vn_fiemap(
	struct inode		*inode,
	struct fiemap_extent_info *fieinfo,
	u64			start,
	u64			length)
{
	int			error;

	xfs_ilock(XFS_I(inode), XFS_IOLOCK_SHARED);
	if (fieinfo->fi_flags & FIEMAP_FLAG_XATTR) {
		fieinfo->fi_flags &= ~FIEMAP_FLAG_XATTR;
		error = iomap_fiemap(inode, fieinfo, start, length,
				&xfs_xattr_iomap_ops);
	} else {
		error = iomap_fiemap(inode, fieinfo, start, length,
				&xfs_read_iomap_ops);
	}
	xfs_iunlock(XFS_I(inode), XFS_IOLOCK_SHARED);

	return error;
}

STATIC int
xfs_vn_tmpfile(
	struct mnt_idmap	*idmap,
	struct inode		*dir,
	struct file		*file,
	umode_t			mode)
{
	int err = xfs_generic_create(idmap, dir, file->f_path.dentry, mode, 0, file);

	return finish_open_simple(file, err);
}

static const struct inode_operations xfs_inode_operations = {
	.get_inode_acl		= xfs_get_acl,
	.set_acl		= xfs_set_acl,
	.getattr		= xfs_vn_getattr,
	.setattr		= xfs_vn_setattr,
	.listxattr		= xfs_vn_listxattr,
	.fiemap			= xfs_vn_fiemap,
	.update_time		= xfs_vn_update_time,
	.fileattr_get		= xfs_fileattr_get,
	.fileattr_set		= xfs_fileattr_set,
};

static const struct inode_operations xfs_dir_inode_operations = {
	.create			= xfs_vn_create,
	.lookup			= xfs_vn_lookup,
	.link			= xfs_vn_link,
	.unlink			= xfs_vn_unlink,
	.symlink		= xfs_vn_symlink,
	.mkdir			= xfs_vn_mkdir,
	 
	.rmdir			= xfs_vn_unlink,
	.mknod			= xfs_vn_mknod,
	.rename			= xfs_vn_rename,
	.get_inode_acl		= xfs_get_acl,
	.set_acl		= xfs_set_acl,
	.getattr		= xfs_vn_getattr,
	.setattr		= xfs_vn_setattr,
	.listxattr		= xfs_vn_listxattr,
	.update_time		= xfs_vn_update_time,
	.tmpfile		= xfs_vn_tmpfile,
	.fileattr_get		= xfs_fileattr_get,
	.fileattr_set		= xfs_fileattr_set,
};

static const struct inode_operations xfs_dir_ci_inode_operations = {
	.create			= xfs_vn_create,
	.lookup			= xfs_vn_ci_lookup,
	.link			= xfs_vn_link,
	.unlink			= xfs_vn_unlink,
	.symlink		= xfs_vn_symlink,
	.mkdir			= xfs_vn_mkdir,
	 
	.rmdir			= xfs_vn_unlink,
	.mknod			= xfs_vn_mknod,
	.rename			= xfs_vn_rename,
	.get_inode_acl		= xfs_get_acl,
	.set_acl		= xfs_set_acl,
	.getattr		= xfs_vn_getattr,
	.setattr		= xfs_vn_setattr,
	.listxattr		= xfs_vn_listxattr,
	.update_time		= xfs_vn_update_time,
	.tmpfile		= xfs_vn_tmpfile,
	.fileattr_get		= xfs_fileattr_get,
	.fileattr_set		= xfs_fileattr_set,
};

static const struct inode_operations xfs_symlink_inode_operations = {
	.get_link		= xfs_vn_get_link,
	.getattr		= xfs_vn_getattr,
	.setattr		= xfs_vn_setattr,
	.listxattr		= xfs_vn_listxattr,
	.update_time		= xfs_vn_update_time,
};

 
static bool
xfs_inode_supports_dax(
	struct xfs_inode	*ip)
{
	struct xfs_mount	*mp = ip->i_mount;

	 
	if (!S_ISREG(VFS_I(ip)->i_mode))
		return false;

	 
	if (mp->m_sb.sb_blocksize != PAGE_SIZE)
		return false;

	 
	return xfs_inode_buftarg(ip)->bt_daxdev != NULL;
}

static bool
xfs_inode_should_enable_dax(
	struct xfs_inode *ip)
{
	if (!IS_ENABLED(CONFIG_FS_DAX))
		return false;
	if (xfs_has_dax_never(ip->i_mount))
		return false;
	if (!xfs_inode_supports_dax(ip))
		return false;
	if (xfs_has_dax_always(ip->i_mount))
		return true;
	if (ip->i_diflags2 & XFS_DIFLAG2_DAX)
		return true;
	return false;
}

void
xfs_diflags_to_iflags(
	struct xfs_inode	*ip,
	bool init)
{
	struct inode            *inode = VFS_I(ip);
	unsigned int            xflags = xfs_ip2xflags(ip);
	unsigned int            flags = 0;

	ASSERT(!(IS_DAX(inode) && init));

	if (xflags & FS_XFLAG_IMMUTABLE)
		flags |= S_IMMUTABLE;
	if (xflags & FS_XFLAG_APPEND)
		flags |= S_APPEND;
	if (xflags & FS_XFLAG_SYNC)
		flags |= S_SYNC;
	if (xflags & FS_XFLAG_NOATIME)
		flags |= S_NOATIME;
	if (init && xfs_inode_should_enable_dax(ip))
		flags |= S_DAX;

	 
	inode->i_flags &= ~(S_IMMUTABLE | S_APPEND | S_SYNC | S_NOATIME);
	inode->i_flags |= flags;
}

 
void
xfs_setup_inode(
	struct xfs_inode	*ip)
{
	struct inode		*inode = &ip->i_vnode;
	gfp_t			gfp_mask;

	inode->i_ino = ip->i_ino;
	inode->i_state |= I_NEW;

	inode_sb_list_add(inode);
	 
	inode_fake_hash(inode);

	i_size_write(inode, ip->i_disk_size);
	xfs_diflags_to_iflags(ip, true);

	if (S_ISDIR(inode->i_mode)) {
		 
		lockdep_set_class(&inode->i_rwsem,
				  &inode->i_sb->s_type->i_mutex_dir_key);
		lockdep_set_class(&ip->i_lock.mr_lock, &xfs_dir_ilock_class);
	} else {
		lockdep_set_class(&ip->i_lock.mr_lock, &xfs_nondir_ilock_class);
	}

	 
	gfp_mask = mapping_gfp_mask(inode->i_mapping);
	mapping_set_gfp_mask(inode->i_mapping, (gfp_mask & ~(__GFP_FS)));

	 
	if (!xfs_inode_has_attr_fork(ip)) {
		inode_has_no_xattr(inode);
		cache_no_acl(inode);
	}
}

void
xfs_setup_iops(
	struct xfs_inode	*ip)
{
	struct inode		*inode = &ip->i_vnode;

	switch (inode->i_mode & S_IFMT) {
	case S_IFREG:
		inode->i_op = &xfs_inode_operations;
		inode->i_fop = &xfs_file_operations;
		if (IS_DAX(inode))
			inode->i_mapping->a_ops = &xfs_dax_aops;
		else
			inode->i_mapping->a_ops = &xfs_address_space_operations;
		break;
	case S_IFDIR:
		if (xfs_has_asciici(XFS_M(inode->i_sb)))
			inode->i_op = &xfs_dir_ci_inode_operations;
		else
			inode->i_op = &xfs_dir_inode_operations;
		inode->i_fop = &xfs_dir_file_operations;
		break;
	case S_IFLNK:
		inode->i_op = &xfs_symlink_inode_operations;
		break;
	default:
		inode->i_op = &xfs_inode_operations;
		init_special_inode(inode, inode->i_mode, inode->i_rdev);
		break;
	}
}
