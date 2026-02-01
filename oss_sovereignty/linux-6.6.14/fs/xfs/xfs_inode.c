
 
#include <linux/iversion.h>

#include "xfs.h"
#include "xfs_fs.h"
#include "xfs_shared.h"
#include "xfs_format.h"
#include "xfs_log_format.h"
#include "xfs_trans_resv.h"
#include "xfs_mount.h"
#include "xfs_defer.h"
#include "xfs_inode.h"
#include "xfs_dir2.h"
#include "xfs_attr.h"
#include "xfs_trans_space.h"
#include "xfs_trans.h"
#include "xfs_buf_item.h"
#include "xfs_inode_item.h"
#include "xfs_iunlink_item.h"
#include "xfs_ialloc.h"
#include "xfs_bmap.h"
#include "xfs_bmap_util.h"
#include "xfs_errortag.h"
#include "xfs_error.h"
#include "xfs_quota.h"
#include "xfs_filestream.h"
#include "xfs_trace.h"
#include "xfs_icache.h"
#include "xfs_symlink.h"
#include "xfs_trans_priv.h"
#include "xfs_log.h"
#include "xfs_bmap_btree.h"
#include "xfs_reflink.h"
#include "xfs_ag.h"
#include "xfs_log_priv.h"

struct kmem_cache *xfs_inode_cache;

 
#define	XFS_ITRUNC_MAX_EXTENTS	2

STATIC int xfs_iunlink(struct xfs_trans *, struct xfs_inode *);
STATIC int xfs_iunlink_remove(struct xfs_trans *tp, struct xfs_perag *pag,
	struct xfs_inode *);

 
xfs_extlen_t
xfs_get_extsz_hint(
	struct xfs_inode	*ip)
{
	 
	if (xfs_is_always_cow_inode(ip))
		return 0;
	if ((ip->i_diflags & XFS_DIFLAG_EXTSIZE) && ip->i_extsize)
		return ip->i_extsize;
	if (XFS_IS_REALTIME_INODE(ip))
		return ip->i_mount->m_sb.sb_rextsize;
	return 0;
}

 
xfs_extlen_t
xfs_get_cowextsz_hint(
	struct xfs_inode	*ip)
{
	xfs_extlen_t		a, b;

	a = 0;
	if (ip->i_diflags2 & XFS_DIFLAG2_COWEXTSIZE)
		a = ip->i_cowextsize;
	b = xfs_get_extsz_hint(ip);

	a = max(a, b);
	if (a == 0)
		return XFS_DEFAULT_COWEXTSZ_HINT;
	return a;
}

 
uint
xfs_ilock_data_map_shared(
	struct xfs_inode	*ip)
{
	uint			lock_mode = XFS_ILOCK_SHARED;

	if (xfs_need_iread_extents(&ip->i_df))
		lock_mode = XFS_ILOCK_EXCL;
	xfs_ilock(ip, lock_mode);
	return lock_mode;
}

uint
xfs_ilock_attr_map_shared(
	struct xfs_inode	*ip)
{
	uint			lock_mode = XFS_ILOCK_SHARED;

	if (xfs_inode_has_attr_fork(ip) && xfs_need_iread_extents(&ip->i_af))
		lock_mode = XFS_ILOCK_EXCL;
	xfs_ilock(ip, lock_mode);
	return lock_mode;
}

 
static inline void
xfs_lock_flags_assert(
	uint		lock_flags)
{
	ASSERT((lock_flags & (XFS_IOLOCK_SHARED | XFS_IOLOCK_EXCL)) !=
		(XFS_IOLOCK_SHARED | XFS_IOLOCK_EXCL));
	ASSERT((lock_flags & (XFS_MMAPLOCK_SHARED | XFS_MMAPLOCK_EXCL)) !=
		(XFS_MMAPLOCK_SHARED | XFS_MMAPLOCK_EXCL));
	ASSERT((lock_flags & (XFS_ILOCK_SHARED | XFS_ILOCK_EXCL)) !=
		(XFS_ILOCK_SHARED | XFS_ILOCK_EXCL));
	ASSERT((lock_flags & ~(XFS_LOCK_MASK | XFS_LOCK_SUBCLASS_MASK)) == 0);
	ASSERT(lock_flags != 0);
}

 
void
xfs_ilock(
	xfs_inode_t		*ip,
	uint			lock_flags)
{
	trace_xfs_ilock(ip, lock_flags, _RET_IP_);

	xfs_lock_flags_assert(lock_flags);

	if (lock_flags & XFS_IOLOCK_EXCL) {
		down_write_nested(&VFS_I(ip)->i_rwsem,
				  XFS_IOLOCK_DEP(lock_flags));
	} else if (lock_flags & XFS_IOLOCK_SHARED) {
		down_read_nested(&VFS_I(ip)->i_rwsem,
				 XFS_IOLOCK_DEP(lock_flags));
	}

	if (lock_flags & XFS_MMAPLOCK_EXCL) {
		down_write_nested(&VFS_I(ip)->i_mapping->invalidate_lock,
				  XFS_MMAPLOCK_DEP(lock_flags));
	} else if (lock_flags & XFS_MMAPLOCK_SHARED) {
		down_read_nested(&VFS_I(ip)->i_mapping->invalidate_lock,
				 XFS_MMAPLOCK_DEP(lock_flags));
	}

	if (lock_flags & XFS_ILOCK_EXCL)
		mrupdate_nested(&ip->i_lock, XFS_ILOCK_DEP(lock_flags));
	else if (lock_flags & XFS_ILOCK_SHARED)
		mraccess_nested(&ip->i_lock, XFS_ILOCK_DEP(lock_flags));
}

 
int
xfs_ilock_nowait(
	xfs_inode_t		*ip,
	uint			lock_flags)
{
	trace_xfs_ilock_nowait(ip, lock_flags, _RET_IP_);

	xfs_lock_flags_assert(lock_flags);

	if (lock_flags & XFS_IOLOCK_EXCL) {
		if (!down_write_trylock(&VFS_I(ip)->i_rwsem))
			goto out;
	} else if (lock_flags & XFS_IOLOCK_SHARED) {
		if (!down_read_trylock(&VFS_I(ip)->i_rwsem))
			goto out;
	}

	if (lock_flags & XFS_MMAPLOCK_EXCL) {
		if (!down_write_trylock(&VFS_I(ip)->i_mapping->invalidate_lock))
			goto out_undo_iolock;
	} else if (lock_flags & XFS_MMAPLOCK_SHARED) {
		if (!down_read_trylock(&VFS_I(ip)->i_mapping->invalidate_lock))
			goto out_undo_iolock;
	}

	if (lock_flags & XFS_ILOCK_EXCL) {
		if (!mrtryupdate(&ip->i_lock))
			goto out_undo_mmaplock;
	} else if (lock_flags & XFS_ILOCK_SHARED) {
		if (!mrtryaccess(&ip->i_lock))
			goto out_undo_mmaplock;
	}
	return 1;

out_undo_mmaplock:
	if (lock_flags & XFS_MMAPLOCK_EXCL)
		up_write(&VFS_I(ip)->i_mapping->invalidate_lock);
	else if (lock_flags & XFS_MMAPLOCK_SHARED)
		up_read(&VFS_I(ip)->i_mapping->invalidate_lock);
out_undo_iolock:
	if (lock_flags & XFS_IOLOCK_EXCL)
		up_write(&VFS_I(ip)->i_rwsem);
	else if (lock_flags & XFS_IOLOCK_SHARED)
		up_read(&VFS_I(ip)->i_rwsem);
out:
	return 0;
}

 
void
xfs_iunlock(
	xfs_inode_t		*ip,
	uint			lock_flags)
{
	xfs_lock_flags_assert(lock_flags);

	if (lock_flags & XFS_IOLOCK_EXCL)
		up_write(&VFS_I(ip)->i_rwsem);
	else if (lock_flags & XFS_IOLOCK_SHARED)
		up_read(&VFS_I(ip)->i_rwsem);

	if (lock_flags & XFS_MMAPLOCK_EXCL)
		up_write(&VFS_I(ip)->i_mapping->invalidate_lock);
	else if (lock_flags & XFS_MMAPLOCK_SHARED)
		up_read(&VFS_I(ip)->i_mapping->invalidate_lock);

	if (lock_flags & XFS_ILOCK_EXCL)
		mrunlock_excl(&ip->i_lock);
	else if (lock_flags & XFS_ILOCK_SHARED)
		mrunlock_shared(&ip->i_lock);

	trace_xfs_iunlock(ip, lock_flags, _RET_IP_);
}

 
void
xfs_ilock_demote(
	xfs_inode_t		*ip,
	uint			lock_flags)
{
	ASSERT(lock_flags & (XFS_IOLOCK_EXCL|XFS_MMAPLOCK_EXCL|XFS_ILOCK_EXCL));
	ASSERT((lock_flags &
		~(XFS_IOLOCK_EXCL|XFS_MMAPLOCK_EXCL|XFS_ILOCK_EXCL)) == 0);

	if (lock_flags & XFS_ILOCK_EXCL)
		mrdemote(&ip->i_lock);
	if (lock_flags & XFS_MMAPLOCK_EXCL)
		downgrade_write(&VFS_I(ip)->i_mapping->invalidate_lock);
	if (lock_flags & XFS_IOLOCK_EXCL)
		downgrade_write(&VFS_I(ip)->i_rwsem);

	trace_xfs_ilock_demote(ip, lock_flags, _RET_IP_);
}

#if defined(DEBUG) || defined(XFS_WARN)
static inline bool
__xfs_rwsem_islocked(
	struct rw_semaphore	*rwsem,
	bool			shared)
{
	if (!debug_locks)
		return rwsem_is_locked(rwsem);

	if (!shared)
		return lockdep_is_held_type(rwsem, 0);

	 
	return lockdep_is_held_type(rwsem, -1);
}

bool
xfs_isilocked(
	struct xfs_inode	*ip,
	uint			lock_flags)
{
	if (lock_flags & (XFS_ILOCK_EXCL|XFS_ILOCK_SHARED)) {
		if (!(lock_flags & XFS_ILOCK_SHARED))
			return !!ip->i_lock.mr_writer;
		return rwsem_is_locked(&ip->i_lock.mr_lock);
	}

	if (lock_flags & (XFS_MMAPLOCK_EXCL|XFS_MMAPLOCK_SHARED)) {
		return __xfs_rwsem_islocked(&VFS_I(ip)->i_mapping->invalidate_lock,
				(lock_flags & XFS_MMAPLOCK_SHARED));
	}

	if (lock_flags & (XFS_IOLOCK_EXCL | XFS_IOLOCK_SHARED)) {
		return __xfs_rwsem_islocked(&VFS_I(ip)->i_rwsem,
				(lock_flags & XFS_IOLOCK_SHARED));
	}

	ASSERT(0);
	return false;
}
#endif

 
#if (defined(DEBUG) || defined(XFS_WARN)) && defined(CONFIG_LOCKDEP)
static bool
xfs_lockdep_subclass_ok(
	int subclass)
{
	return subclass < MAX_LOCKDEP_SUBCLASSES;
}
#else
#define xfs_lockdep_subclass_ok(subclass)	(true)
#endif

 
static inline uint
xfs_lock_inumorder(
	uint	lock_mode,
	uint	subclass)
{
	uint	class = 0;

	ASSERT(!(lock_mode & (XFS_ILOCK_PARENT | XFS_ILOCK_RTBITMAP |
			      XFS_ILOCK_RTSUM)));
	ASSERT(xfs_lockdep_subclass_ok(subclass));

	if (lock_mode & (XFS_IOLOCK_SHARED|XFS_IOLOCK_EXCL)) {
		ASSERT(subclass <= XFS_IOLOCK_MAX_SUBCLASS);
		class += subclass << XFS_IOLOCK_SHIFT;
	}

	if (lock_mode & (XFS_MMAPLOCK_SHARED|XFS_MMAPLOCK_EXCL)) {
		ASSERT(subclass <= XFS_MMAPLOCK_MAX_SUBCLASS);
		class += subclass << XFS_MMAPLOCK_SHIFT;
	}

	if (lock_mode & (XFS_ILOCK_SHARED|XFS_ILOCK_EXCL)) {
		ASSERT(subclass <= XFS_ILOCK_MAX_SUBCLASS);
		class += subclass << XFS_ILOCK_SHIFT;
	}

	return (lock_mode & ~XFS_LOCK_SUBCLASS_MASK) | class;
}

 
static void
xfs_lock_inodes(
	struct xfs_inode	**ips,
	int			inodes,
	uint			lock_mode)
{
	int			attempts = 0;
	uint			i;
	int			j;
	bool			try_lock;
	struct xfs_log_item	*lp;

	 
	ASSERT(ips && inodes >= 2 && inodes <= 5);
	ASSERT(lock_mode & (XFS_IOLOCK_EXCL | XFS_MMAPLOCK_EXCL |
			    XFS_ILOCK_EXCL));
	ASSERT(!(lock_mode & (XFS_IOLOCK_SHARED | XFS_MMAPLOCK_SHARED |
			      XFS_ILOCK_SHARED)));
	ASSERT(!(lock_mode & XFS_MMAPLOCK_EXCL) ||
		inodes <= XFS_MMAPLOCK_MAX_SUBCLASS + 1);
	ASSERT(!(lock_mode & XFS_ILOCK_EXCL) ||
		inodes <= XFS_ILOCK_MAX_SUBCLASS + 1);

	if (lock_mode & XFS_IOLOCK_EXCL) {
		ASSERT(!(lock_mode & (XFS_MMAPLOCK_EXCL | XFS_ILOCK_EXCL)));
	} else if (lock_mode & XFS_MMAPLOCK_EXCL)
		ASSERT(!(lock_mode & XFS_ILOCK_EXCL));

again:
	try_lock = false;
	i = 0;
	for (; i < inodes; i++) {
		ASSERT(ips[i]);

		if (i && (ips[i] == ips[i - 1]))	 
			continue;

		 
		if (!try_lock) {
			for (j = (i - 1); j >= 0 && !try_lock; j--) {
				lp = &ips[j]->i_itemp->ili_item;
				if (lp && test_bit(XFS_LI_IN_AIL, &lp->li_flags))
					try_lock = true;
			}
		}

		 
		if (!try_lock) {
			xfs_ilock(ips[i], xfs_lock_inumorder(lock_mode, i));
			continue;
		}

		 
		ASSERT(i != 0);
		if (xfs_ilock_nowait(ips[i], xfs_lock_inumorder(lock_mode, i)))
			continue;

		 
		attempts++;
		for (j = i - 1; j >= 0; j--) {
			 
			if (j != (i - 1) && ips[j] == ips[j + 1])
				continue;

			xfs_iunlock(ips[j], lock_mode);
		}

		if ((attempts % 5) == 0) {
			delay(1);  
		}
		goto again;
	}
}

 
void
xfs_lock_two_inodes(
	struct xfs_inode	*ip0,
	uint			ip0_mode,
	struct xfs_inode	*ip1,
	uint			ip1_mode)
{
	int			attempts = 0;
	struct xfs_log_item	*lp;

	ASSERT(hweight32(ip0_mode) == 1);
	ASSERT(hweight32(ip1_mode) == 1);
	ASSERT(!(ip0_mode & (XFS_IOLOCK_SHARED|XFS_IOLOCK_EXCL)));
	ASSERT(!(ip1_mode & (XFS_IOLOCK_SHARED|XFS_IOLOCK_EXCL)));
	ASSERT(!(ip0_mode & (XFS_MMAPLOCK_SHARED|XFS_MMAPLOCK_EXCL)));
	ASSERT(!(ip1_mode & (XFS_MMAPLOCK_SHARED|XFS_MMAPLOCK_EXCL)));
	ASSERT(ip0->i_ino != ip1->i_ino);

	if (ip0->i_ino > ip1->i_ino) {
		swap(ip0, ip1);
		swap(ip0_mode, ip1_mode);
	}

 again:
	xfs_ilock(ip0, xfs_lock_inumorder(ip0_mode, 0));

	 
	lp = &ip0->i_itemp->ili_item;
	if (lp && test_bit(XFS_LI_IN_AIL, &lp->li_flags)) {
		if (!xfs_ilock_nowait(ip1, xfs_lock_inumorder(ip1_mode, 1))) {
			xfs_iunlock(ip0, ip0_mode);
			if ((++attempts % 5) == 0)
				delay(1);  
			goto again;
		}
	} else {
		xfs_ilock(ip1, xfs_lock_inumorder(ip1_mode, 1));
	}
}

uint
xfs_ip2xflags(
	struct xfs_inode	*ip)
{
	uint			flags = 0;

	if (ip->i_diflags & XFS_DIFLAG_ANY) {
		if (ip->i_diflags & XFS_DIFLAG_REALTIME)
			flags |= FS_XFLAG_REALTIME;
		if (ip->i_diflags & XFS_DIFLAG_PREALLOC)
			flags |= FS_XFLAG_PREALLOC;
		if (ip->i_diflags & XFS_DIFLAG_IMMUTABLE)
			flags |= FS_XFLAG_IMMUTABLE;
		if (ip->i_diflags & XFS_DIFLAG_APPEND)
			flags |= FS_XFLAG_APPEND;
		if (ip->i_diflags & XFS_DIFLAG_SYNC)
			flags |= FS_XFLAG_SYNC;
		if (ip->i_diflags & XFS_DIFLAG_NOATIME)
			flags |= FS_XFLAG_NOATIME;
		if (ip->i_diflags & XFS_DIFLAG_NODUMP)
			flags |= FS_XFLAG_NODUMP;
		if (ip->i_diflags & XFS_DIFLAG_RTINHERIT)
			flags |= FS_XFLAG_RTINHERIT;
		if (ip->i_diflags & XFS_DIFLAG_PROJINHERIT)
			flags |= FS_XFLAG_PROJINHERIT;
		if (ip->i_diflags & XFS_DIFLAG_NOSYMLINKS)
			flags |= FS_XFLAG_NOSYMLINKS;
		if (ip->i_diflags & XFS_DIFLAG_EXTSIZE)
			flags |= FS_XFLAG_EXTSIZE;
		if (ip->i_diflags & XFS_DIFLAG_EXTSZINHERIT)
			flags |= FS_XFLAG_EXTSZINHERIT;
		if (ip->i_diflags & XFS_DIFLAG_NODEFRAG)
			flags |= FS_XFLAG_NODEFRAG;
		if (ip->i_diflags & XFS_DIFLAG_FILESTREAM)
			flags |= FS_XFLAG_FILESTREAM;
	}

	if (ip->i_diflags2 & XFS_DIFLAG2_ANY) {
		if (ip->i_diflags2 & XFS_DIFLAG2_DAX)
			flags |= FS_XFLAG_DAX;
		if (ip->i_diflags2 & XFS_DIFLAG2_COWEXTSIZE)
			flags |= FS_XFLAG_COWEXTSIZE;
	}

	if (xfs_inode_has_attr_fork(ip))
		flags |= FS_XFLAG_HASATTR;
	return flags;
}

 
int
xfs_lookup(
	struct xfs_inode	*dp,
	const struct xfs_name	*name,
	struct xfs_inode	**ipp,
	struct xfs_name		*ci_name)
{
	xfs_ino_t		inum;
	int			error;

	trace_xfs_lookup(dp, name);

	if (xfs_is_shutdown(dp->i_mount))
		return -EIO;

	error = xfs_dir_lookup(NULL, dp, name, &inum, ci_name);
	if (error)
		goto out_unlock;

	error = xfs_iget(dp->i_mount, NULL, inum, 0, 0, ipp);
	if (error)
		goto out_free_name;

	return 0;

out_free_name:
	if (ci_name)
		kmem_free(ci_name->name);
out_unlock:
	*ipp = NULL;
	return error;
}

 
static void
xfs_inode_inherit_flags(
	struct xfs_inode	*ip,
	const struct xfs_inode	*pip)
{
	unsigned int		di_flags = 0;
	xfs_failaddr_t		failaddr;
	umode_t			mode = VFS_I(ip)->i_mode;

	if (S_ISDIR(mode)) {
		if (pip->i_diflags & XFS_DIFLAG_RTINHERIT)
			di_flags |= XFS_DIFLAG_RTINHERIT;
		if (pip->i_diflags & XFS_DIFLAG_EXTSZINHERIT) {
			di_flags |= XFS_DIFLAG_EXTSZINHERIT;
			ip->i_extsize = pip->i_extsize;
		}
		if (pip->i_diflags & XFS_DIFLAG_PROJINHERIT)
			di_flags |= XFS_DIFLAG_PROJINHERIT;
	} else if (S_ISREG(mode)) {
		if ((pip->i_diflags & XFS_DIFLAG_RTINHERIT) &&
		    xfs_has_realtime(ip->i_mount))
			di_flags |= XFS_DIFLAG_REALTIME;
		if (pip->i_diflags & XFS_DIFLAG_EXTSZINHERIT) {
			di_flags |= XFS_DIFLAG_EXTSIZE;
			ip->i_extsize = pip->i_extsize;
		}
	}
	if ((pip->i_diflags & XFS_DIFLAG_NOATIME) &&
	    xfs_inherit_noatime)
		di_flags |= XFS_DIFLAG_NOATIME;
	if ((pip->i_diflags & XFS_DIFLAG_NODUMP) &&
	    xfs_inherit_nodump)
		di_flags |= XFS_DIFLAG_NODUMP;
	if ((pip->i_diflags & XFS_DIFLAG_SYNC) &&
	    xfs_inherit_sync)
		di_flags |= XFS_DIFLAG_SYNC;
	if ((pip->i_diflags & XFS_DIFLAG_NOSYMLINKS) &&
	    xfs_inherit_nosymlinks)
		di_flags |= XFS_DIFLAG_NOSYMLINKS;
	if ((pip->i_diflags & XFS_DIFLAG_NODEFRAG) &&
	    xfs_inherit_nodefrag)
		di_flags |= XFS_DIFLAG_NODEFRAG;
	if (pip->i_diflags & XFS_DIFLAG_FILESTREAM)
		di_flags |= XFS_DIFLAG_FILESTREAM;

	ip->i_diflags |= di_flags;

	 
	failaddr = xfs_inode_validate_extsize(ip->i_mount, ip->i_extsize,
			VFS_I(ip)->i_mode, ip->i_diflags);
	if (failaddr) {
		ip->i_diflags &= ~(XFS_DIFLAG_EXTSIZE |
				   XFS_DIFLAG_EXTSZINHERIT);
		ip->i_extsize = 0;
	}
}

 
static void
xfs_inode_inherit_flags2(
	struct xfs_inode	*ip,
	const struct xfs_inode	*pip)
{
	xfs_failaddr_t		failaddr;

	if (pip->i_diflags2 & XFS_DIFLAG2_COWEXTSIZE) {
		ip->i_diflags2 |= XFS_DIFLAG2_COWEXTSIZE;
		ip->i_cowextsize = pip->i_cowextsize;
	}
	if (pip->i_diflags2 & XFS_DIFLAG2_DAX)
		ip->i_diflags2 |= XFS_DIFLAG2_DAX;

	 
	failaddr = xfs_inode_validate_cowextsize(ip->i_mount, ip->i_cowextsize,
			VFS_I(ip)->i_mode, ip->i_diflags, ip->i_diflags2);
	if (failaddr) {
		ip->i_diflags2 &= ~XFS_DIFLAG2_COWEXTSIZE;
		ip->i_cowextsize = 0;
	}
}

 
int
xfs_init_new_inode(
	struct mnt_idmap	*idmap,
	struct xfs_trans	*tp,
	struct xfs_inode	*pip,
	xfs_ino_t		ino,
	umode_t			mode,
	xfs_nlink_t		nlink,
	dev_t			rdev,
	prid_t			prid,
	bool			init_xattrs,
	struct xfs_inode	**ipp)
{
	struct inode		*dir = pip ? VFS_I(pip) : NULL;
	struct xfs_mount	*mp = tp->t_mountp;
	struct xfs_inode	*ip;
	unsigned int		flags;
	int			error;
	struct timespec64	tv;
	struct inode		*inode;

	 
	if ((pip && ino == pip->i_ino) || !xfs_verify_dir_ino(mp, ino)) {
		xfs_alert(mp, "Allocated a known in-use inode 0x%llx!", ino);
		return -EFSCORRUPTED;
	}

	 
	error = xfs_iget(mp, tp, ino, XFS_IGET_CREATE, XFS_ILOCK_EXCL, &ip);
	if (error)
		return error;

	ASSERT(ip != NULL);
	inode = VFS_I(ip);
	set_nlink(inode, nlink);
	inode->i_rdev = rdev;
	ip->i_projid = prid;

	if (dir && !(dir->i_mode & S_ISGID) && xfs_has_grpid(mp)) {
		inode_fsuid_set(inode, idmap);
		inode->i_gid = dir->i_gid;
		inode->i_mode = mode;
	} else {
		inode_init_owner(idmap, inode, dir, mode);
	}

	 
	if (irix_sgid_inherit && (inode->i_mode & S_ISGID) &&
	    !vfsgid_in_group_p(i_gid_into_vfsgid(idmap, inode)))
		inode->i_mode &= ~S_ISGID;

	ip->i_disk_size = 0;
	ip->i_df.if_nextents = 0;
	ASSERT(ip->i_nblocks == 0);

	tv = inode_set_ctime_current(inode);
	inode->i_mtime = tv;
	inode->i_atime = tv;

	ip->i_extsize = 0;
	ip->i_diflags = 0;

	if (xfs_has_v3inodes(mp)) {
		inode_set_iversion(inode, 1);
		ip->i_cowextsize = 0;
		ip->i_crtime = tv;
	}

	flags = XFS_ILOG_CORE;
	switch (mode & S_IFMT) {
	case S_IFIFO:
	case S_IFCHR:
	case S_IFBLK:
	case S_IFSOCK:
		ip->i_df.if_format = XFS_DINODE_FMT_DEV;
		flags |= XFS_ILOG_DEV;
		break;
	case S_IFREG:
	case S_IFDIR:
		if (pip && (pip->i_diflags & XFS_DIFLAG_ANY))
			xfs_inode_inherit_flags(ip, pip);
		if (pip && (pip->i_diflags2 & XFS_DIFLAG2_ANY))
			xfs_inode_inherit_flags2(ip, pip);
		fallthrough;
	case S_IFLNK:
		ip->i_df.if_format = XFS_DINODE_FMT_EXTENTS;
		ip->i_df.if_bytes = 0;
		ip->i_df.if_u1.if_root = NULL;
		break;
	default:
		ASSERT(0);
	}

	 
	if (init_xattrs && xfs_has_attr(mp)) {
		ip->i_forkoff = xfs_default_attroffset(ip) >> 3;
		xfs_ifork_init_attr(ip, XFS_DINODE_FMT_EXTENTS, 0);
	}

	 
	xfs_trans_ijoin(tp, ip, XFS_ILOCK_EXCL);
	xfs_trans_log_inode(tp, ip, flags);

	 
	xfs_setup_inode(ip);

	*ipp = ip;
	return 0;
}

 
static int			 
xfs_droplink(
	xfs_trans_t *tp,
	xfs_inode_t *ip)
{
	xfs_trans_ichgtime(tp, ip, XFS_ICHGTIME_CHG);

	drop_nlink(VFS_I(ip));
	xfs_trans_log_inode(tp, ip, XFS_ILOG_CORE);

	if (VFS_I(ip)->i_nlink)
		return 0;

	return xfs_iunlink(tp, ip);
}

 
static void
xfs_bumplink(
	xfs_trans_t *tp,
	xfs_inode_t *ip)
{
	xfs_trans_ichgtime(tp, ip, XFS_ICHGTIME_CHG);

	inc_nlink(VFS_I(ip));
	xfs_trans_log_inode(tp, ip, XFS_ILOG_CORE);
}

int
xfs_create(
	struct mnt_idmap	*idmap,
	xfs_inode_t		*dp,
	struct xfs_name		*name,
	umode_t			mode,
	dev_t			rdev,
	bool			init_xattrs,
	xfs_inode_t		**ipp)
{
	int			is_dir = S_ISDIR(mode);
	struct xfs_mount	*mp = dp->i_mount;
	struct xfs_inode	*ip = NULL;
	struct xfs_trans	*tp = NULL;
	int			error;
	bool                    unlock_dp_on_error = false;
	prid_t			prid;
	struct xfs_dquot	*udqp = NULL;
	struct xfs_dquot	*gdqp = NULL;
	struct xfs_dquot	*pdqp = NULL;
	struct xfs_trans_res	*tres;
	uint			resblks;
	xfs_ino_t		ino;

	trace_xfs_create(dp, name);

	if (xfs_is_shutdown(mp))
		return -EIO;

	prid = xfs_get_initial_prid(dp);

	 
	error = xfs_qm_vop_dqalloc(dp, mapped_fsuid(idmap, &init_user_ns),
			mapped_fsgid(idmap, &init_user_ns), prid,
			XFS_QMOPT_QUOTALL | XFS_QMOPT_INHERIT,
			&udqp, &gdqp, &pdqp);
	if (error)
		return error;

	if (is_dir) {
		resblks = XFS_MKDIR_SPACE_RES(mp, name->len);
		tres = &M_RES(mp)->tr_mkdir;
	} else {
		resblks = XFS_CREATE_SPACE_RES(mp, name->len);
		tres = &M_RES(mp)->tr_create;
	}

	 
	error = xfs_trans_alloc_icreate(mp, tres, udqp, gdqp, pdqp, resblks,
			&tp);
	if (error == -ENOSPC) {
		 
		xfs_flush_inodes(mp);
		error = xfs_trans_alloc_icreate(mp, tres, udqp, gdqp, pdqp,
				resblks, &tp);
	}
	if (error)
		goto out_release_dquots;

	xfs_ilock(dp, XFS_ILOCK_EXCL | XFS_ILOCK_PARENT);
	unlock_dp_on_error = true;

	 
	error = xfs_dialloc(&tp, dp->i_ino, mode, &ino);
	if (!error)
		error = xfs_init_new_inode(idmap, tp, dp, ino, mode,
				is_dir ? 2 : 1, rdev, prid, init_xattrs, &ip);
	if (error)
		goto out_trans_cancel;

	 
	xfs_trans_ijoin(tp, dp, XFS_ILOCK_EXCL);
	unlock_dp_on_error = false;

	error = xfs_dir_createname(tp, dp, name, ip->i_ino,
					resblks - XFS_IALLOC_SPACE_RES(mp));
	if (error) {
		ASSERT(error != -ENOSPC);
		goto out_trans_cancel;
	}
	xfs_trans_ichgtime(tp, dp, XFS_ICHGTIME_MOD | XFS_ICHGTIME_CHG);
	xfs_trans_log_inode(tp, dp, XFS_ILOG_CORE);

	if (is_dir) {
		error = xfs_dir_init(tp, ip, dp);
		if (error)
			goto out_trans_cancel;

		xfs_bumplink(tp, dp);
	}

	 
	if (xfs_has_wsync(mp) || xfs_has_dirsync(mp))
		xfs_trans_set_sync(tp);

	 
	xfs_qm_vop_create_dqattach(tp, ip, udqp, gdqp, pdqp);

	error = xfs_trans_commit(tp);
	if (error)
		goto out_release_inode;

	xfs_qm_dqrele(udqp);
	xfs_qm_dqrele(gdqp);
	xfs_qm_dqrele(pdqp);

	*ipp = ip;
	return 0;

 out_trans_cancel:
	xfs_trans_cancel(tp);
 out_release_inode:
	 
	if (ip) {
		xfs_finish_inode_setup(ip);
		xfs_irele(ip);
	}
 out_release_dquots:
	xfs_qm_dqrele(udqp);
	xfs_qm_dqrele(gdqp);
	xfs_qm_dqrele(pdqp);

	if (unlock_dp_on_error)
		xfs_iunlock(dp, XFS_ILOCK_EXCL);
	return error;
}

int
xfs_create_tmpfile(
	struct mnt_idmap	*idmap,
	struct xfs_inode	*dp,
	umode_t			mode,
	struct xfs_inode	**ipp)
{
	struct xfs_mount	*mp = dp->i_mount;
	struct xfs_inode	*ip = NULL;
	struct xfs_trans	*tp = NULL;
	int			error;
	prid_t                  prid;
	struct xfs_dquot	*udqp = NULL;
	struct xfs_dquot	*gdqp = NULL;
	struct xfs_dquot	*pdqp = NULL;
	struct xfs_trans_res	*tres;
	uint			resblks;
	xfs_ino_t		ino;

	if (xfs_is_shutdown(mp))
		return -EIO;

	prid = xfs_get_initial_prid(dp);

	 
	error = xfs_qm_vop_dqalloc(dp, mapped_fsuid(idmap, &init_user_ns),
			mapped_fsgid(idmap, &init_user_ns), prid,
			XFS_QMOPT_QUOTALL | XFS_QMOPT_INHERIT,
			&udqp, &gdqp, &pdqp);
	if (error)
		return error;

	resblks = XFS_IALLOC_SPACE_RES(mp);
	tres = &M_RES(mp)->tr_create_tmpfile;

	error = xfs_trans_alloc_icreate(mp, tres, udqp, gdqp, pdqp, resblks,
			&tp);
	if (error)
		goto out_release_dquots;

	error = xfs_dialloc(&tp, dp->i_ino, mode, &ino);
	if (!error)
		error = xfs_init_new_inode(idmap, tp, dp, ino, mode,
				0, 0, prid, false, &ip);
	if (error)
		goto out_trans_cancel;

	if (xfs_has_wsync(mp))
		xfs_trans_set_sync(tp);

	 
	xfs_qm_vop_create_dqattach(tp, ip, udqp, gdqp, pdqp);

	error = xfs_iunlink(tp, ip);
	if (error)
		goto out_trans_cancel;

	error = xfs_trans_commit(tp);
	if (error)
		goto out_release_inode;

	xfs_qm_dqrele(udqp);
	xfs_qm_dqrele(gdqp);
	xfs_qm_dqrele(pdqp);

	*ipp = ip;
	return 0;

 out_trans_cancel:
	xfs_trans_cancel(tp);
 out_release_inode:
	 
	if (ip) {
		xfs_finish_inode_setup(ip);
		xfs_irele(ip);
	}
 out_release_dquots:
	xfs_qm_dqrele(udqp);
	xfs_qm_dqrele(gdqp);
	xfs_qm_dqrele(pdqp);

	return error;
}

int
xfs_link(
	xfs_inode_t		*tdp,
	xfs_inode_t		*sip,
	struct xfs_name		*target_name)
{
	xfs_mount_t		*mp = tdp->i_mount;
	xfs_trans_t		*tp;
	int			error, nospace_error = 0;
	int			resblks;

	trace_xfs_link(tdp, target_name);

	ASSERT(!S_ISDIR(VFS_I(sip)->i_mode));

	if (xfs_is_shutdown(mp))
		return -EIO;

	error = xfs_qm_dqattach(sip);
	if (error)
		goto std_return;

	error = xfs_qm_dqattach(tdp);
	if (error)
		goto std_return;

	resblks = XFS_LINK_SPACE_RES(mp, target_name->len);
	error = xfs_trans_alloc_dir(tdp, &M_RES(mp)->tr_link, sip, &resblks,
			&tp, &nospace_error);
	if (error)
		goto std_return;

	 
	if (unlikely((tdp->i_diflags & XFS_DIFLAG_PROJINHERIT) &&
		     tdp->i_projid != sip->i_projid)) {
		error = -EXDEV;
		goto error_return;
	}

	if (!resblks) {
		error = xfs_dir_canenter(tp, tdp, target_name);
		if (error)
			goto error_return;
	}

	 
	if (VFS_I(sip)->i_nlink == 0) {
		struct xfs_perag	*pag;

		pag = xfs_perag_get(mp, XFS_INO_TO_AGNO(mp, sip->i_ino));
		error = xfs_iunlink_remove(tp, pag, sip);
		xfs_perag_put(pag);
		if (error)
			goto error_return;
	}

	error = xfs_dir_createname(tp, tdp, target_name, sip->i_ino,
				   resblks);
	if (error)
		goto error_return;
	xfs_trans_ichgtime(tp, tdp, XFS_ICHGTIME_MOD | XFS_ICHGTIME_CHG);
	xfs_trans_log_inode(tp, tdp, XFS_ILOG_CORE);

	xfs_bumplink(tp, sip);

	 
	if (xfs_has_wsync(mp) || xfs_has_dirsync(mp))
		xfs_trans_set_sync(tp);

	return xfs_trans_commit(tp);

 error_return:
	xfs_trans_cancel(tp);
 std_return:
	if (error == -ENOSPC && nospace_error)
		error = nospace_error;
	return error;
}

 
static void
xfs_itruncate_clear_reflink_flags(
	struct xfs_inode	*ip)
{
	struct xfs_ifork	*dfork;
	struct xfs_ifork	*cfork;

	if (!xfs_is_reflink_inode(ip))
		return;
	dfork = xfs_ifork_ptr(ip, XFS_DATA_FORK);
	cfork = xfs_ifork_ptr(ip, XFS_COW_FORK);
	if (dfork->if_bytes == 0 && cfork->if_bytes == 0)
		ip->i_diflags2 &= ~XFS_DIFLAG2_REFLINK;
	if (cfork->if_bytes == 0)
		xfs_inode_clear_cowblocks_tag(ip);
}

 
int
xfs_itruncate_extents_flags(
	struct xfs_trans	**tpp,
	struct xfs_inode	*ip,
	int			whichfork,
	xfs_fsize_t		new_size,
	int			flags)
{
	struct xfs_mount	*mp = ip->i_mount;
	struct xfs_trans	*tp = *tpp;
	xfs_fileoff_t		first_unmap_block;
	xfs_filblks_t		unmap_len;
	int			error = 0;

	ASSERT(xfs_isilocked(ip, XFS_ILOCK_EXCL));
	ASSERT(!atomic_read(&VFS_I(ip)->i_count) ||
	       xfs_isilocked(ip, XFS_IOLOCK_EXCL));
	ASSERT(new_size <= XFS_ISIZE(ip));
	ASSERT(tp->t_flags & XFS_TRANS_PERM_LOG_RES);
	ASSERT(ip->i_itemp != NULL);
	ASSERT(ip->i_itemp->ili_lock_flags == 0);
	ASSERT(!XFS_NOT_DQATTACHED(mp, ip));

	trace_xfs_itruncate_extents_start(ip, new_size);

	flags |= xfs_bmapi_aflag(whichfork);

	 
	first_unmap_block = XFS_B_TO_FSB(mp, (xfs_ufsize_t)new_size);
	if (!xfs_verify_fileoff(mp, first_unmap_block)) {
		WARN_ON_ONCE(first_unmap_block > XFS_MAX_FILEOFF);
		return 0;
	}

	unmap_len = XFS_MAX_FILEOFF - first_unmap_block + 1;
	while (unmap_len > 0) {
		ASSERT(tp->t_highest_agno == NULLAGNUMBER);
		error = __xfs_bunmapi(tp, ip, first_unmap_block, &unmap_len,
				flags, XFS_ITRUNC_MAX_EXTENTS);
		if (error)
			goto out;

		 
		error = xfs_defer_finish(&tp);
		if (error)
			goto out;
	}

	if (whichfork == XFS_DATA_FORK) {
		 
		error = xfs_reflink_cancel_cow_blocks(ip, &tp,
				first_unmap_block, XFS_MAX_FILEOFF, true);
		if (error)
			goto out;

		xfs_itruncate_clear_reflink_flags(ip);
	}

	 
	xfs_trans_log_inode(tp, ip, XFS_ILOG_CORE);

	trace_xfs_itruncate_extents_end(ip, new_size);

out:
	*tpp = tp;
	return error;
}

int
xfs_release(
	xfs_inode_t	*ip)
{
	xfs_mount_t	*mp = ip->i_mount;
	int		error = 0;

	if (!S_ISREG(VFS_I(ip)->i_mode) || (VFS_I(ip)->i_mode == 0))
		return 0;

	 
	if (xfs_is_readonly(mp))
		return 0;

	if (!xfs_is_shutdown(mp)) {
		int truncated;

		 
		truncated = xfs_iflags_test_and_clear(ip, XFS_ITRUNCATED);
		if (truncated) {
			xfs_iflags_clear(ip, XFS_IDIRTY_RELEASE);
			if (ip->i_delayed_blks > 0) {
				error = filemap_flush(VFS_I(ip)->i_mapping);
				if (error)
					return error;
			}
		}
	}

	if (VFS_I(ip)->i_nlink == 0)
		return 0;

	 
	if (!xfs_ilock_nowait(ip, XFS_IOLOCK_EXCL))
		return 0;

	if (xfs_can_free_eofblocks(ip, false)) {
		 
		if (xfs_iflags_test(ip, XFS_IDIRTY_RELEASE))
			goto out_unlock;

		error = xfs_free_eofblocks(ip);
		if (error)
			goto out_unlock;

		 
		if (ip->i_delayed_blks)
			xfs_iflags_set(ip, XFS_IDIRTY_RELEASE);
	}

out_unlock:
	xfs_iunlock(ip, XFS_IOLOCK_EXCL);
	return error;
}

 
STATIC int
xfs_inactive_truncate(
	struct xfs_inode *ip)
{
	struct xfs_mount	*mp = ip->i_mount;
	struct xfs_trans	*tp;
	int			error;

	error = xfs_trans_alloc(mp, &M_RES(mp)->tr_itruncate, 0, 0, 0, &tp);
	if (error) {
		ASSERT(xfs_is_shutdown(mp));
		return error;
	}
	xfs_ilock(ip, XFS_ILOCK_EXCL);
	xfs_trans_ijoin(tp, ip, 0);

	 
	ip->i_disk_size = 0;
	xfs_trans_log_inode(tp, ip, XFS_ILOG_CORE);

	error = xfs_itruncate_extents(&tp, ip, XFS_DATA_FORK, 0);
	if (error)
		goto error_trans_cancel;

	ASSERT(ip->i_df.if_nextents == 0);

	error = xfs_trans_commit(tp);
	if (error)
		goto error_unlock;

	xfs_iunlock(ip, XFS_ILOCK_EXCL);
	return 0;

error_trans_cancel:
	xfs_trans_cancel(tp);
error_unlock:
	xfs_iunlock(ip, XFS_ILOCK_EXCL);
	return error;
}

 
STATIC int
xfs_inactive_ifree(
	struct xfs_inode *ip)
{
	struct xfs_mount	*mp = ip->i_mount;
	struct xfs_trans	*tp;
	int			error;

	 
	if (unlikely(mp->m_finobt_nores)) {
		error = xfs_trans_alloc(mp, &M_RES(mp)->tr_ifree,
				XFS_IFREE_SPACE_RES(mp), 0, XFS_TRANS_RESERVE,
				&tp);
	} else {
		error = xfs_trans_alloc(mp, &M_RES(mp)->tr_ifree, 0, 0, 0, &tp);
	}
	if (error) {
		if (error == -ENOSPC) {
			xfs_warn_ratelimited(mp,
			"Failed to remove inode(s) from unlinked list. "
			"Please free space, unmount and run xfs_repair.");
		} else {
			ASSERT(xfs_is_shutdown(mp));
		}
		return error;
	}

	 
	xfs_ilock(ip, XFS_ILOCK_EXCL);
	xfs_trans_ijoin(tp, ip, XFS_ILOCK_EXCL);

	error = xfs_ifree(tp, ip);
	ASSERT(xfs_isilocked(ip, XFS_ILOCK_EXCL));
	if (error) {
		 
		if (!xfs_is_shutdown(mp)) {
			xfs_notice(mp, "%s: xfs_ifree returned error %d",
				__func__, error);
			xfs_force_shutdown(mp, SHUTDOWN_META_IO_ERROR);
		}
		xfs_trans_cancel(tp);
		return error;
	}

	 
	xfs_trans_mod_dquot_byino(tp, ip, XFS_TRANS_DQ_ICOUNT, -1);

	return xfs_trans_commit(tp);
}

 
bool
xfs_inode_needs_inactive(
	struct xfs_inode	*ip)
{
	struct xfs_mount	*mp = ip->i_mount;
	struct xfs_ifork	*cow_ifp = xfs_ifork_ptr(ip, XFS_COW_FORK);

	 
	if (VFS_I(ip)->i_mode == 0)
		return false;

	 
	if (xfs_is_readonly(mp) && !xlog_recovery_needed(mp->m_log))
		return false;

	 
	if (xfs_is_shutdown(mp) || xfs_has_norecovery(mp))
		return false;

	 
	if (xfs_is_metadata_inode(ip))
		return false;

	 
	if (cow_ifp && cow_ifp->if_bytes > 0)
		return true;

	 
	if (VFS_I(ip)->i_nlink == 0)
		return true;

	 
	return xfs_can_free_eofblocks(ip, true);
}

 
int
xfs_inactive(
	xfs_inode_t	*ip)
{
	struct xfs_mount	*mp;
	int			error = 0;
	int			truncate = 0;

	 
	if (VFS_I(ip)->i_mode == 0) {
		ASSERT(ip->i_df.if_broot_bytes == 0);
		goto out;
	}

	mp = ip->i_mount;
	ASSERT(!xfs_iflags_test(ip, XFS_IRECOVERY));

	 
	if (xfs_is_readonly(mp) && !xlog_recovery_needed(mp->m_log))
		goto out;

	 
	if (xfs_is_metadata_inode(ip))
		goto out;

	 
	if (xfs_inode_has_cow_data(ip))
		xfs_reflink_cancel_cow_range(ip, 0, NULLFILEOFF, true);

	if (VFS_I(ip)->i_nlink != 0) {
		 
		if (xfs_can_free_eofblocks(ip, true))
			error = xfs_free_eofblocks(ip);

		goto out;
	}

	if (S_ISREG(VFS_I(ip)->i_mode) &&
	    (ip->i_disk_size != 0 || XFS_ISIZE(ip) != 0 ||
	     ip->i_df.if_nextents > 0 || ip->i_delayed_blks > 0))
		truncate = 1;

	if (xfs_iflags_test(ip, XFS_IQUOTAUNCHECKED)) {
		 
		xfs_qm_dqdetach(ip);
	} else {
		error = xfs_qm_dqattach(ip);
		if (error)
			goto out;
	}

	if (S_ISLNK(VFS_I(ip)->i_mode))
		error = xfs_inactive_symlink(ip);
	else if (truncate)
		error = xfs_inactive_truncate(ip);
	if (error)
		goto out;

	 
	if (xfs_inode_has_attr_fork(ip)) {
		error = xfs_attr_inactive(ip);
		if (error)
			goto out;
	}

	ASSERT(ip->i_forkoff == 0);

	 
	error = xfs_inactive_ifree(ip);

out:
	 
	xfs_qm_dqdetach(ip);
	return error;
}

 

 
static struct xfs_inode *
xfs_iunlink_lookup(
	struct xfs_perag	*pag,
	xfs_agino_t		agino)
{
	struct xfs_inode	*ip;

	rcu_read_lock();
	ip = radix_tree_lookup(&pag->pag_ici_root, agino);
	if (!ip) {
		 
		rcu_read_unlock();
		return NULL;
	}

	 
	if (WARN_ON_ONCE(!ip->i_ino)) {
		rcu_read_unlock();
		return NULL;
	}
	ASSERT(!xfs_iflags_test(ip, XFS_IRECLAIMABLE | XFS_IRECLAIM));
	rcu_read_unlock();
	return ip;
}

 
static int
xfs_iunlink_update_backref(
	struct xfs_perag	*pag,
	xfs_agino_t		prev_agino,
	xfs_agino_t		next_agino)
{
	struct xfs_inode	*ip;

	 
	if (next_agino == NULLAGINO)
		return 0;

	ip = xfs_iunlink_lookup(pag, next_agino);
	if (!ip)
		return -ENOLINK;

	ip->i_prev_unlinked = prev_agino;
	return 0;
}

 
STATIC int
xfs_iunlink_update_bucket(
	struct xfs_trans	*tp,
	struct xfs_perag	*pag,
	struct xfs_buf		*agibp,
	unsigned int		bucket_index,
	xfs_agino_t		new_agino)
{
	struct xfs_agi		*agi = agibp->b_addr;
	xfs_agino_t		old_value;
	int			offset;

	ASSERT(xfs_verify_agino_or_null(pag, new_agino));

	old_value = be32_to_cpu(agi->agi_unlinked[bucket_index]);
	trace_xfs_iunlink_update_bucket(tp->t_mountp, pag->pag_agno, bucket_index,
			old_value, new_agino);

	 
	if (old_value == new_agino) {
		xfs_buf_mark_corrupt(agibp);
		return -EFSCORRUPTED;
	}

	agi->agi_unlinked[bucket_index] = cpu_to_be32(new_agino);
	offset = offsetof(struct xfs_agi, agi_unlinked) +
			(sizeof(xfs_agino_t) * bucket_index);
	xfs_trans_log_buf(tp, agibp, offset, offset + sizeof(xfs_agino_t) - 1);
	return 0;
}

 
STATIC int
xfs_iunlink_reload_next(
	struct xfs_trans	*tp,
	struct xfs_buf		*agibp,
	xfs_agino_t		prev_agino,
	xfs_agino_t		next_agino)
{
	struct xfs_perag	*pag = agibp->b_pag;
	struct xfs_mount	*mp = pag->pag_mount;
	struct xfs_inode	*next_ip = NULL;
	xfs_ino_t		ino;
	int			error;

	ASSERT(next_agino != NULLAGINO);

#ifdef DEBUG
	rcu_read_lock();
	next_ip = radix_tree_lookup(&pag->pag_ici_root, next_agino);
	ASSERT(next_ip == NULL);
	rcu_read_unlock();
#endif

	xfs_info_ratelimited(mp,
 "Found unrecovered unlinked inode 0x%x in AG 0x%x.  Initiating recovery.",
			next_agino, pag->pag_agno);

	 
	ino = XFS_AGINO_TO_INO(mp, pag->pag_agno, next_agino);
	error = xfs_iget(mp, tp, ino, XFS_IGET_UNTRUSTED, 0, &next_ip);
	if (error)
		return error;

	 
	if (VFS_I(next_ip)->i_nlink != 0) {
		error = -EFSCORRUPTED;
		goto rele;
	}

	next_ip->i_prev_unlinked = prev_agino;
	trace_xfs_iunlink_reload_next(next_ip);
rele:
	ASSERT(!(VFS_I(next_ip)->i_state & I_DONTCACHE));
	if (xfs_is_quotacheck_running(mp) && next_ip)
		xfs_iflags_set(next_ip, XFS_IQUOTAUNCHECKED);
	xfs_irele(next_ip);
	return error;
}

static int
xfs_iunlink_insert_inode(
	struct xfs_trans	*tp,
	struct xfs_perag	*pag,
	struct xfs_buf		*agibp,
	struct xfs_inode	*ip)
{
	struct xfs_mount	*mp = tp->t_mountp;
	struct xfs_agi		*agi = agibp->b_addr;
	xfs_agino_t		next_agino;
	xfs_agino_t		agino = XFS_INO_TO_AGINO(mp, ip->i_ino);
	short			bucket_index = agino % XFS_AGI_UNLINKED_BUCKETS;
	int			error;

	 
	next_agino = be32_to_cpu(agi->agi_unlinked[bucket_index]);
	if (next_agino == agino ||
	    !xfs_verify_agino_or_null(pag, next_agino)) {
		xfs_buf_mark_corrupt(agibp);
		return -EFSCORRUPTED;
	}

	 
	error = xfs_iunlink_update_backref(pag, agino, next_agino);
	if (error == -ENOLINK)
		error = xfs_iunlink_reload_next(tp, agibp, agino, next_agino);
	if (error)
		return error;

	if (next_agino != NULLAGINO) {
		 
		error = xfs_iunlink_log_inode(tp, ip, pag, next_agino);
		if (error)
			return error;
		ip->i_next_unlinked = next_agino;
	}

	 
	ip->i_prev_unlinked = NULLAGINO;
	return xfs_iunlink_update_bucket(tp, pag, agibp, bucket_index, agino);
}

 
STATIC int
xfs_iunlink(
	struct xfs_trans	*tp,
	struct xfs_inode	*ip)
{
	struct xfs_mount	*mp = tp->t_mountp;
	struct xfs_perag	*pag;
	struct xfs_buf		*agibp;
	int			error;

	ASSERT(VFS_I(ip)->i_nlink == 0);
	ASSERT(VFS_I(ip)->i_mode != 0);
	trace_xfs_iunlink(ip);

	pag = xfs_perag_get(mp, XFS_INO_TO_AGNO(mp, ip->i_ino));

	 
	error = xfs_read_agi(pag, tp, &agibp);
	if (error)
		goto out;

	error = xfs_iunlink_insert_inode(tp, pag, agibp, ip);
out:
	xfs_perag_put(pag);
	return error;
}

static int
xfs_iunlink_remove_inode(
	struct xfs_trans	*tp,
	struct xfs_perag	*pag,
	struct xfs_buf		*agibp,
	struct xfs_inode	*ip)
{
	struct xfs_mount	*mp = tp->t_mountp;
	struct xfs_agi		*agi = agibp->b_addr;
	xfs_agino_t		agino = XFS_INO_TO_AGINO(mp, ip->i_ino);
	xfs_agino_t		head_agino;
	short			bucket_index = agino % XFS_AGI_UNLINKED_BUCKETS;
	int			error;

	trace_xfs_iunlink_remove(ip);

	 
	head_agino = be32_to_cpu(agi->agi_unlinked[bucket_index]);
	if (!xfs_verify_agino(pag, head_agino)) {
		XFS_CORRUPTION_ERROR(__func__, XFS_ERRLEVEL_LOW, mp,
				agi, sizeof(*agi));
		return -EFSCORRUPTED;
	}

	 
	error = xfs_iunlink_log_inode(tp, ip, pag, NULLAGINO);
	if (error)
		return error;

	 
	error = xfs_iunlink_update_backref(pag, ip->i_prev_unlinked,
			ip->i_next_unlinked);
	if (error == -ENOLINK)
		error = xfs_iunlink_reload_next(tp, agibp, ip->i_prev_unlinked,
				ip->i_next_unlinked);
	if (error)
		return error;

	if (head_agino != agino) {
		struct xfs_inode	*prev_ip;

		prev_ip = xfs_iunlink_lookup(pag, ip->i_prev_unlinked);
		if (!prev_ip)
			return -EFSCORRUPTED;

		error = xfs_iunlink_log_inode(tp, prev_ip, pag,
				ip->i_next_unlinked);
		prev_ip->i_next_unlinked = ip->i_next_unlinked;
	} else {
		 
		error = xfs_iunlink_update_bucket(tp, pag, agibp, bucket_index,
				ip->i_next_unlinked);
	}

	ip->i_next_unlinked = NULLAGINO;
	ip->i_prev_unlinked = 0;
	return error;
}

 
STATIC int
xfs_iunlink_remove(
	struct xfs_trans	*tp,
	struct xfs_perag	*pag,
	struct xfs_inode	*ip)
{
	struct xfs_buf		*agibp;
	int			error;

	trace_xfs_iunlink_remove(ip);

	 
	error = xfs_read_agi(pag, tp, &agibp);
	if (error)
		return error;

	return xfs_iunlink_remove_inode(tp, pag, agibp, ip);
}

 
static void
xfs_ifree_mark_inode_stale(
	struct xfs_perag	*pag,
	struct xfs_inode	*free_ip,
	xfs_ino_t		inum)
{
	struct xfs_mount	*mp = pag->pag_mount;
	struct xfs_inode_log_item *iip;
	struct xfs_inode	*ip;

retry:
	rcu_read_lock();
	ip = radix_tree_lookup(&pag->pag_ici_root, XFS_INO_TO_AGINO(mp, inum));

	 
	if (!ip) {
		rcu_read_unlock();
		return;
	}

	 
	spin_lock(&ip->i_flags_lock);
	if (ip->i_ino != inum || __xfs_iflags_test(ip, XFS_ISTALE))
		goto out_iflags_unlock;

	 
	if (ip != free_ip) {
		if (!xfs_ilock_nowait(ip, XFS_ILOCK_EXCL)) {
			spin_unlock(&ip->i_flags_lock);
			rcu_read_unlock();
			delay(1);
			goto retry;
		}
	}
	ip->i_flags |= XFS_ISTALE;

	 
	iip = ip->i_itemp;
	if (__xfs_iflags_test(ip, XFS_IFLUSHING)) {
		ASSERT(!list_empty(&iip->ili_item.li_bio_list));
		ASSERT(iip->ili_last_fields);
		goto out_iunlock;
	}

	 
	if (!iip || list_empty(&iip->ili_item.li_bio_list))
		goto out_iunlock;

	__xfs_iflags_set(ip, XFS_IFLUSHING);
	spin_unlock(&ip->i_flags_lock);
	rcu_read_unlock();

	 
	spin_lock(&iip->ili_lock);
	iip->ili_last_fields = iip->ili_fields;
	iip->ili_fields = 0;
	iip->ili_fsync_fields = 0;
	spin_unlock(&iip->ili_lock);
	ASSERT(iip->ili_last_fields);

	if (ip != free_ip)
		xfs_iunlock(ip, XFS_ILOCK_EXCL);
	return;

out_iunlock:
	if (ip != free_ip)
		xfs_iunlock(ip, XFS_ILOCK_EXCL);
out_iflags_unlock:
	spin_unlock(&ip->i_flags_lock);
	rcu_read_unlock();
}

 
static int
xfs_ifree_cluster(
	struct xfs_trans	*tp,
	struct xfs_perag	*pag,
	struct xfs_inode	*free_ip,
	struct xfs_icluster	*xic)
{
	struct xfs_mount	*mp = free_ip->i_mount;
	struct xfs_ino_geometry	*igeo = M_IGEO(mp);
	struct xfs_buf		*bp;
	xfs_daddr_t		blkno;
	xfs_ino_t		inum = xic->first_ino;
	int			nbufs;
	int			i, j;
	int			ioffset;
	int			error;

	nbufs = igeo->ialloc_blks / igeo->blocks_per_cluster;

	for (j = 0; j < nbufs; j++, inum += igeo->inodes_per_cluster) {
		 
		ioffset = inum - xic->first_ino;
		if ((xic->alloc & XFS_INOBT_MASK(ioffset)) == 0) {
			ASSERT(ioffset % igeo->inodes_per_cluster == 0);
			continue;
		}

		blkno = XFS_AGB_TO_DADDR(mp, XFS_INO_TO_AGNO(mp, inum),
					 XFS_INO_TO_AGBNO(mp, inum));

		 
		error = xfs_trans_get_buf(tp, mp->m_ddev_targp, blkno,
				mp->m_bsize * igeo->blocks_per_cluster,
				XBF_UNMAPPED, &bp);
		if (error)
			return error;

		 
		bp->b_ops = &xfs_inode_buf_ops;

		 
		for (i = 0; i < igeo->inodes_per_cluster; i++)
			xfs_ifree_mark_inode_stale(pag, free_ip, inum + i);

		xfs_trans_stale_inode_buf(tp, bp);
		xfs_trans_binval(tp, bp);
	}
	return 0;
}

 
int
xfs_ifree(
	struct xfs_trans	*tp,
	struct xfs_inode	*ip)
{
	struct xfs_mount	*mp = ip->i_mount;
	struct xfs_perag	*pag;
	struct xfs_icluster	xic = { 0 };
	struct xfs_inode_log_item *iip = ip->i_itemp;
	int			error;

	ASSERT(xfs_isilocked(ip, XFS_ILOCK_EXCL));
	ASSERT(VFS_I(ip)->i_nlink == 0);
	ASSERT(ip->i_df.if_nextents == 0);
	ASSERT(ip->i_disk_size == 0 || !S_ISREG(VFS_I(ip)->i_mode));
	ASSERT(ip->i_nblocks == 0);

	pag = xfs_perag_get(mp, XFS_INO_TO_AGNO(mp, ip->i_ino));

	 
	error = xfs_difree(tp, pag, ip->i_ino, &xic);
	if (error)
		goto out;

	error = xfs_iunlink_remove(tp, pag, ip);
	if (error)
		goto out;

	 
	if (ip->i_df.if_format == XFS_DINODE_FMT_LOCAL) {
		kmem_free(ip->i_df.if_u1.if_data);
		ip->i_df.if_u1.if_data = NULL;
		ip->i_df.if_bytes = 0;
	}

	VFS_I(ip)->i_mode = 0;		 
	ip->i_diflags = 0;
	ip->i_diflags2 = mp->m_ino_geo.new_diflags2;
	ip->i_forkoff = 0;		 
	ip->i_df.if_format = XFS_DINODE_FMT_EXTENTS;
	if (xfs_iflags_test(ip, XFS_IPRESERVE_DM_FIELDS))
		xfs_iflags_clear(ip, XFS_IPRESERVE_DM_FIELDS);

	 
	spin_lock(&iip->ili_lock);
	iip->ili_fields &= ~(XFS_ILOG_AOWNER | XFS_ILOG_DOWNER);
	spin_unlock(&iip->ili_lock);

	 
	VFS_I(ip)->i_generation++;
	xfs_trans_log_inode(tp, ip, XFS_ILOG_CORE);

	if (xic.deleted)
		error = xfs_ifree_cluster(tp, pag, ip, &xic);
out:
	xfs_perag_put(pag);
	return error;
}

 
static void
xfs_iunpin(
	struct xfs_inode	*ip)
{
	ASSERT(xfs_isilocked(ip, XFS_ILOCK_EXCL|XFS_ILOCK_SHARED));

	trace_xfs_inode_unpin_nowait(ip, _RET_IP_);

	 
	xfs_log_force_seq(ip->i_mount, ip->i_itemp->ili_commit_seq, 0, NULL);

}

static void
__xfs_iunpin_wait(
	struct xfs_inode	*ip)
{
	wait_queue_head_t *wq = bit_waitqueue(&ip->i_flags, __XFS_IPINNED_BIT);
	DEFINE_WAIT_BIT(wait, &ip->i_flags, __XFS_IPINNED_BIT);

	xfs_iunpin(ip);

	do {
		prepare_to_wait(wq, &wait.wq_entry, TASK_UNINTERRUPTIBLE);
		if (xfs_ipincount(ip))
			io_schedule();
	} while (xfs_ipincount(ip));
	finish_wait(wq, &wait.wq_entry);
}

void
xfs_iunpin_wait(
	struct xfs_inode	*ip)
{
	if (xfs_ipincount(ip))
		__xfs_iunpin_wait(ip);
}

 
int
xfs_remove(
	xfs_inode_t             *dp,
	struct xfs_name		*name,
	xfs_inode_t		*ip)
{
	xfs_mount_t		*mp = dp->i_mount;
	xfs_trans_t             *tp = NULL;
	int			is_dir = S_ISDIR(VFS_I(ip)->i_mode);
	int			dontcare;
	int                     error = 0;
	uint			resblks;

	trace_xfs_remove(dp, name);

	if (xfs_is_shutdown(mp))
		return -EIO;

	error = xfs_qm_dqattach(dp);
	if (error)
		goto std_return;

	error = xfs_qm_dqattach(ip);
	if (error)
		goto std_return;

	 
	resblks = XFS_REMOVE_SPACE_RES(mp);
	error = xfs_trans_alloc_dir(dp, &M_RES(mp)->tr_remove, ip, &resblks,
			&tp, &dontcare);
	if (error) {
		ASSERT(error != -ENOSPC);
		goto std_return;
	}

	 
	if (is_dir) {
		ASSERT(VFS_I(ip)->i_nlink >= 2);
		if (VFS_I(ip)->i_nlink != 2) {
			error = -ENOTEMPTY;
			goto out_trans_cancel;
		}
		if (!xfs_dir_isempty(ip)) {
			error = -ENOTEMPTY;
			goto out_trans_cancel;
		}

		 
		error = xfs_droplink(tp, dp);
		if (error)
			goto out_trans_cancel;

		 
		error = xfs_droplink(tp, ip);
		if (error)
			goto out_trans_cancel;

		 
		if (dp->i_ino != tp->t_mountp->m_sb.sb_rootino) {
			error = xfs_dir_replace(tp, ip, &xfs_name_dotdot,
					tp->t_mountp->m_sb.sb_rootino, 0);
			if (error)
				goto out_trans_cancel;
		}
	} else {
		 
		xfs_trans_log_inode(tp, dp, XFS_ILOG_CORE);
	}
	xfs_trans_ichgtime(tp, dp, XFS_ICHGTIME_MOD | XFS_ICHGTIME_CHG);

	 
	error = xfs_droplink(tp, ip);
	if (error)
		goto out_trans_cancel;

	error = xfs_dir_removename(tp, dp, name, ip->i_ino, resblks);
	if (error) {
		ASSERT(error != -ENOENT);
		goto out_trans_cancel;
	}

	 
	if (xfs_has_wsync(mp) || xfs_has_dirsync(mp))
		xfs_trans_set_sync(tp);

	error = xfs_trans_commit(tp);
	if (error)
		goto std_return;

	if (is_dir && xfs_inode_is_filestream(ip))
		xfs_filestream_deassociate(ip);

	return 0;

 out_trans_cancel:
	xfs_trans_cancel(tp);
 std_return:
	return error;
}

 
#define __XFS_SORT_INODES	5
STATIC void
xfs_sort_for_rename(
	struct xfs_inode	*dp1,	 
	struct xfs_inode	*dp2,	 
	struct xfs_inode	*ip1,	 
	struct xfs_inode	*ip2,	 
	struct xfs_inode	*wip,	 
	struct xfs_inode	**i_tab, 
	int			*num_inodes)   
{
	int			i, j;

	ASSERT(*num_inodes == __XFS_SORT_INODES);
	memset(i_tab, 0, *num_inodes * sizeof(struct xfs_inode *));

	 
	i = 0;
	i_tab[i++] = dp1;
	i_tab[i++] = dp2;
	i_tab[i++] = ip1;
	if (ip2)
		i_tab[i++] = ip2;
	if (wip)
		i_tab[i++] = wip;
	*num_inodes = i;

	 
	for (i = 0; i < *num_inodes; i++) {
		for (j = 1; j < *num_inodes; j++) {
			if (i_tab[j]->i_ino < i_tab[j-1]->i_ino) {
				struct xfs_inode *temp = i_tab[j];
				i_tab[j] = i_tab[j-1];
				i_tab[j-1] = temp;
			}
		}
	}
}

static int
xfs_finish_rename(
	struct xfs_trans	*tp)
{
	 
	if (xfs_has_wsync(tp->t_mountp) || xfs_has_dirsync(tp->t_mountp))
		xfs_trans_set_sync(tp);

	return xfs_trans_commit(tp);
}

 
STATIC int
xfs_cross_rename(
	struct xfs_trans	*tp,
	struct xfs_inode	*dp1,
	struct xfs_name		*name1,
	struct xfs_inode	*ip1,
	struct xfs_inode	*dp2,
	struct xfs_name		*name2,
	struct xfs_inode	*ip2,
	int			spaceres)
{
	int		error = 0;
	int		ip1_flags = 0;
	int		ip2_flags = 0;
	int		dp2_flags = 0;

	 
	error = xfs_dir_replace(tp, dp1, name1, ip2->i_ino, spaceres);
	if (error)
		goto out_trans_abort;

	 
	error = xfs_dir_replace(tp, dp2, name2, ip1->i_ino, spaceres);
	if (error)
		goto out_trans_abort;

	 
	if (dp1 != dp2) {
		dp2_flags = XFS_ICHGTIME_MOD | XFS_ICHGTIME_CHG;

		if (S_ISDIR(VFS_I(ip2)->i_mode)) {
			error = xfs_dir_replace(tp, ip2, &xfs_name_dotdot,
						dp1->i_ino, spaceres);
			if (error)
				goto out_trans_abort;

			 
			if (!S_ISDIR(VFS_I(ip1)->i_mode)) {
				error = xfs_droplink(tp, dp2);
				if (error)
					goto out_trans_abort;
				xfs_bumplink(tp, dp1);
			}

			 
			ip1_flags |= XFS_ICHGTIME_CHG;
			ip2_flags |= XFS_ICHGTIME_MOD | XFS_ICHGTIME_CHG;
		}

		if (S_ISDIR(VFS_I(ip1)->i_mode)) {
			error = xfs_dir_replace(tp, ip1, &xfs_name_dotdot,
						dp2->i_ino, spaceres);
			if (error)
				goto out_trans_abort;

			 
			if (!S_ISDIR(VFS_I(ip2)->i_mode)) {
				error = xfs_droplink(tp, dp1);
				if (error)
					goto out_trans_abort;
				xfs_bumplink(tp, dp2);
			}

			 
			ip1_flags |= XFS_ICHGTIME_MOD | XFS_ICHGTIME_CHG;
			ip2_flags |= XFS_ICHGTIME_CHG;
		}
	}

	if (ip1_flags) {
		xfs_trans_ichgtime(tp, ip1, ip1_flags);
		xfs_trans_log_inode(tp, ip1, XFS_ILOG_CORE);
	}
	if (ip2_flags) {
		xfs_trans_ichgtime(tp, ip2, ip2_flags);
		xfs_trans_log_inode(tp, ip2, XFS_ILOG_CORE);
	}
	if (dp2_flags) {
		xfs_trans_ichgtime(tp, dp2, dp2_flags);
		xfs_trans_log_inode(tp, dp2, XFS_ILOG_CORE);
	}
	xfs_trans_ichgtime(tp, dp1, XFS_ICHGTIME_MOD | XFS_ICHGTIME_CHG);
	xfs_trans_log_inode(tp, dp1, XFS_ILOG_CORE);
	return xfs_finish_rename(tp);

out_trans_abort:
	xfs_trans_cancel(tp);
	return error;
}

 
static int
xfs_rename_alloc_whiteout(
	struct mnt_idmap	*idmap,
	struct xfs_name		*src_name,
	struct xfs_inode	*dp,
	struct xfs_inode	**wip)
{
	struct xfs_inode	*tmpfile;
	struct qstr		name;
	int			error;

	error = xfs_create_tmpfile(idmap, dp, S_IFCHR | WHITEOUT_MODE,
				   &tmpfile);
	if (error)
		return error;

	name.name = src_name->name;
	name.len = src_name->len;
	error = xfs_inode_init_security(VFS_I(tmpfile), VFS_I(dp), &name);
	if (error) {
		xfs_finish_inode_setup(tmpfile);
		xfs_irele(tmpfile);
		return error;
	}

	 
	xfs_setup_iops(tmpfile);
	xfs_finish_inode_setup(tmpfile);
	VFS_I(tmpfile)->i_state |= I_LINKABLE;

	*wip = tmpfile;
	return 0;
}

 
int
xfs_rename(
	struct mnt_idmap	*idmap,
	struct xfs_inode	*src_dp,
	struct xfs_name		*src_name,
	struct xfs_inode	*src_ip,
	struct xfs_inode	*target_dp,
	struct xfs_name		*target_name,
	struct xfs_inode	*target_ip,
	unsigned int		flags)
{
	struct xfs_mount	*mp = src_dp->i_mount;
	struct xfs_trans	*tp;
	struct xfs_inode	*wip = NULL;		 
	struct xfs_inode	*inodes[__XFS_SORT_INODES];
	int			i;
	int			num_inodes = __XFS_SORT_INODES;
	bool			new_parent = (src_dp != target_dp);
	bool			src_is_directory = S_ISDIR(VFS_I(src_ip)->i_mode);
	int			spaceres;
	bool			retried = false;
	int			error, nospace_error = 0;

	trace_xfs_rename(src_dp, target_dp, src_name, target_name);

	if ((flags & RENAME_EXCHANGE) && !target_ip)
		return -EINVAL;

	 
	if (flags & RENAME_WHITEOUT) {
		error = xfs_rename_alloc_whiteout(idmap, src_name,
						  target_dp, &wip);
		if (error)
			return error;

		 
		src_name->type = XFS_DIR3_FT_CHRDEV;
	}

	xfs_sort_for_rename(src_dp, target_dp, src_ip, target_ip, wip,
				inodes, &num_inodes);

retry:
	nospace_error = 0;
	spaceres = XFS_RENAME_SPACE_RES(mp, target_name->len);
	error = xfs_trans_alloc(mp, &M_RES(mp)->tr_rename, spaceres, 0, 0, &tp);
	if (error == -ENOSPC) {
		nospace_error = error;
		spaceres = 0;
		error = xfs_trans_alloc(mp, &M_RES(mp)->tr_rename, 0, 0, 0,
				&tp);
	}
	if (error)
		goto out_release_wip;

	 
	error = xfs_qm_vop_rename_dqattach(inodes);
	if (error)
		goto out_trans_cancel;

	 
	xfs_lock_inodes(inodes, num_inodes, XFS_ILOCK_EXCL);

	 
	xfs_trans_ijoin(tp, src_dp, XFS_ILOCK_EXCL);
	if (new_parent)
		xfs_trans_ijoin(tp, target_dp, XFS_ILOCK_EXCL);
	xfs_trans_ijoin(tp, src_ip, XFS_ILOCK_EXCL);
	if (target_ip)
		xfs_trans_ijoin(tp, target_ip, XFS_ILOCK_EXCL);
	if (wip)
		xfs_trans_ijoin(tp, wip, XFS_ILOCK_EXCL);

	 
	if (unlikely((target_dp->i_diflags & XFS_DIFLAG_PROJINHERIT) &&
		     target_dp->i_projid != src_ip->i_projid)) {
		error = -EXDEV;
		goto out_trans_cancel;
	}

	 
	if (flags & RENAME_EXCHANGE)
		return xfs_cross_rename(tp, src_dp, src_name, src_ip,
					target_dp, target_name, target_ip,
					spaceres);

	 
	if (spaceres != 0) {
		error = xfs_trans_reserve_quota_nblks(tp, target_dp, spaceres,
				0, false);
		if (error == -EDQUOT || error == -ENOSPC) {
			if (!retried) {
				xfs_trans_cancel(tp);
				xfs_blockgc_free_quota(target_dp, 0);
				retried = true;
				goto retry;
			}

			nospace_error = error;
			spaceres = 0;
			error = 0;
		}
		if (error)
			goto out_trans_cancel;
	}

	 
	if (target_ip == NULL) {
		 
		if (!spaceres) {
			error = xfs_dir_canenter(tp, target_dp, target_name);
			if (error)
				goto out_trans_cancel;
		}
	} else {
		 
		if (S_ISDIR(VFS_I(target_ip)->i_mode) &&
		    (!xfs_dir_isempty(target_ip) ||
		     (VFS_I(target_ip)->i_nlink > 2))) {
			error = -EEXIST;
			goto out_trans_cancel;
		}
	}

	 
	for (i = 0; i < num_inodes && inodes[i] != NULL; i++) {
		if (inodes[i] == wip ||
		    (inodes[i] == target_ip &&
		     (VFS_I(target_ip)->i_nlink == 1 || src_is_directory))) {
			struct xfs_perag	*pag;
			struct xfs_buf		*bp;

			pag = xfs_perag_get(mp,
					XFS_INO_TO_AGNO(mp, inodes[i]->i_ino));
			error = xfs_read_agi(pag, tp, &bp);
			xfs_perag_put(pag);
			if (error)
				goto out_trans_cancel;
		}
	}

	 
	if (wip) {
		struct xfs_perag	*pag;

		ASSERT(VFS_I(wip)->i_nlink == 0);

		pag = xfs_perag_get(mp, XFS_INO_TO_AGNO(mp, wip->i_ino));
		error = xfs_iunlink_remove(tp, pag, wip);
		xfs_perag_put(pag);
		if (error)
			goto out_trans_cancel;

		xfs_bumplink(tp, wip);
		VFS_I(wip)->i_state &= ~I_LINKABLE;
	}

	 
	if (target_ip == NULL) {
		 
		error = xfs_dir_createname(tp, target_dp, target_name,
					   src_ip->i_ino, spaceres);
		if (error)
			goto out_trans_cancel;

		xfs_trans_ichgtime(tp, target_dp,
					XFS_ICHGTIME_MOD | XFS_ICHGTIME_CHG);

		if (new_parent && src_is_directory) {
			xfs_bumplink(tp, target_dp);
		}
	} else {  
		 
		error = xfs_dir_replace(tp, target_dp, target_name,
					src_ip->i_ino, spaceres);
		if (error)
			goto out_trans_cancel;

		xfs_trans_ichgtime(tp, target_dp,
					XFS_ICHGTIME_MOD | XFS_ICHGTIME_CHG);

		 
		error = xfs_droplink(tp, target_ip);
		if (error)
			goto out_trans_cancel;

		if (src_is_directory) {
			 
			error = xfs_droplink(tp, target_ip);
			if (error)
				goto out_trans_cancel;
		}
	}  

	 
	if (new_parent && src_is_directory) {
		 
		error = xfs_dir_replace(tp, src_ip, &xfs_name_dotdot,
					target_dp->i_ino, spaceres);
		ASSERT(error != -EEXIST);
		if (error)
			goto out_trans_cancel;
	}

	 
	xfs_trans_ichgtime(tp, src_ip, XFS_ICHGTIME_CHG);
	xfs_trans_log_inode(tp, src_ip, XFS_ILOG_CORE);

	 
	if (src_is_directory && (new_parent || target_ip != NULL)) {

		 
		error = xfs_droplink(tp, src_dp);
		if (error)
			goto out_trans_cancel;
	}

	 
	if (wip)
		error = xfs_dir_replace(tp, src_dp, src_name, wip->i_ino,
					spaceres);
	else
		error = xfs_dir_removename(tp, src_dp, src_name, src_ip->i_ino,
					   spaceres);

	if (error)
		goto out_trans_cancel;

	xfs_trans_ichgtime(tp, src_dp, XFS_ICHGTIME_MOD | XFS_ICHGTIME_CHG);
	xfs_trans_log_inode(tp, src_dp, XFS_ILOG_CORE);
	if (new_parent)
		xfs_trans_log_inode(tp, target_dp, XFS_ILOG_CORE);

	error = xfs_finish_rename(tp);
	if (wip)
		xfs_irele(wip);
	return error;

out_trans_cancel:
	xfs_trans_cancel(tp);
out_release_wip:
	if (wip)
		xfs_irele(wip);
	if (error == -ENOSPC && nospace_error)
		error = nospace_error;
	return error;
}

static int
xfs_iflush(
	struct xfs_inode	*ip,
	struct xfs_buf		*bp)
{
	struct xfs_inode_log_item *iip = ip->i_itemp;
	struct xfs_dinode	*dip;
	struct xfs_mount	*mp = ip->i_mount;
	int			error;

	ASSERT(xfs_isilocked(ip, XFS_ILOCK_EXCL|XFS_ILOCK_SHARED));
	ASSERT(xfs_iflags_test(ip, XFS_IFLUSHING));
	ASSERT(ip->i_df.if_format != XFS_DINODE_FMT_BTREE ||
	       ip->i_df.if_nextents > XFS_IFORK_MAXEXT(ip, XFS_DATA_FORK));
	ASSERT(iip->ili_item.li_buf == bp);

	dip = xfs_buf_offset(bp, ip->i_imap.im_boffset);

	 
	error = -EFSCORRUPTED;
	if (XFS_TEST_ERROR(dip->di_magic != cpu_to_be16(XFS_DINODE_MAGIC),
			       mp, XFS_ERRTAG_IFLUSH_1)) {
		xfs_alert_tag(mp, XFS_PTAG_IFLUSH,
			"%s: Bad inode %llu magic number 0x%x, ptr "PTR_FMT,
			__func__, ip->i_ino, be16_to_cpu(dip->di_magic), dip);
		goto flush_out;
	}
	if (S_ISREG(VFS_I(ip)->i_mode)) {
		if (XFS_TEST_ERROR(
		    ip->i_df.if_format != XFS_DINODE_FMT_EXTENTS &&
		    ip->i_df.if_format != XFS_DINODE_FMT_BTREE,
		    mp, XFS_ERRTAG_IFLUSH_3)) {
			xfs_alert_tag(mp, XFS_PTAG_IFLUSH,
				"%s: Bad regular inode %llu, ptr "PTR_FMT,
				__func__, ip->i_ino, ip);
			goto flush_out;
		}
	} else if (S_ISDIR(VFS_I(ip)->i_mode)) {
		if (XFS_TEST_ERROR(
		    ip->i_df.if_format != XFS_DINODE_FMT_EXTENTS &&
		    ip->i_df.if_format != XFS_DINODE_FMT_BTREE &&
		    ip->i_df.if_format != XFS_DINODE_FMT_LOCAL,
		    mp, XFS_ERRTAG_IFLUSH_4)) {
			xfs_alert_tag(mp, XFS_PTAG_IFLUSH,
				"%s: Bad directory inode %llu, ptr "PTR_FMT,
				__func__, ip->i_ino, ip);
			goto flush_out;
		}
	}
	if (XFS_TEST_ERROR(ip->i_df.if_nextents + xfs_ifork_nextents(&ip->i_af) >
				ip->i_nblocks, mp, XFS_ERRTAG_IFLUSH_5)) {
		xfs_alert_tag(mp, XFS_PTAG_IFLUSH,
			"%s: detected corrupt incore inode %llu, "
			"total extents = %llu nblocks = %lld, ptr "PTR_FMT,
			__func__, ip->i_ino,
			ip->i_df.if_nextents + xfs_ifork_nextents(&ip->i_af),
			ip->i_nblocks, ip);
		goto flush_out;
	}
	if (XFS_TEST_ERROR(ip->i_forkoff > mp->m_sb.sb_inodesize,
				mp, XFS_ERRTAG_IFLUSH_6)) {
		xfs_alert_tag(mp, XFS_PTAG_IFLUSH,
			"%s: bad inode %llu, forkoff 0x%x, ptr "PTR_FMT,
			__func__, ip->i_ino, ip->i_forkoff, ip);
		goto flush_out;
	}

	 
	if (!xfs_has_v3inodes(mp))
		ip->i_flushiter++;

	 
	if (ip->i_df.if_format == XFS_DINODE_FMT_LOCAL &&
	    xfs_ifork_verify_local_data(ip))
		goto flush_out;
	if (xfs_inode_has_attr_fork(ip) &&
	    ip->i_af.if_format == XFS_DINODE_FMT_LOCAL &&
	    xfs_ifork_verify_local_attr(ip))
		goto flush_out;

	 
	xfs_inode_to_disk(ip, dip, iip->ili_item.li_lsn);

	 
	if (!xfs_has_v3inodes(mp)) {
		if (ip->i_flushiter == DI_MAX_FLUSH)
			ip->i_flushiter = 0;
	}

	xfs_iflush_fork(ip, dip, iip, XFS_DATA_FORK);
	if (xfs_inode_has_attr_fork(ip))
		xfs_iflush_fork(ip, dip, iip, XFS_ATTR_FORK);

	 
	error = 0;
flush_out:
	spin_lock(&iip->ili_lock);
	iip->ili_last_fields = iip->ili_fields;
	iip->ili_fields = 0;
	iip->ili_fsync_fields = 0;
	spin_unlock(&iip->ili_lock);

	 
	xfs_trans_ail_copy_lsn(mp->m_ail, &iip->ili_flush_lsn,
				&iip->ili_item.li_lsn);

	 
	xfs_dinode_calc_crc(mp, dip);
	return error;
}

 
int
xfs_iflush_cluster(
	struct xfs_buf		*bp)
{
	struct xfs_mount	*mp = bp->b_mount;
	struct xfs_log_item	*lip, *n;
	struct xfs_inode	*ip;
	struct xfs_inode_log_item *iip;
	int			clcount = 0;
	int			error = 0;

	 
	list_for_each_entry_safe(lip, n, &bp->b_li_list, li_bio_list) {
		iip = (struct xfs_inode_log_item *)lip;
		ip = iip->ili_inode;

		 
		if (__xfs_iflags_test(ip, XFS_IRECLAIM | XFS_IFLUSHING))
			continue;
		if (xfs_ipincount(ip))
			continue;

		 
		spin_lock(&ip->i_flags_lock);
		ASSERT(!__xfs_iflags_test(ip, XFS_ISTALE));
		if (__xfs_iflags_test(ip, XFS_IRECLAIM | XFS_IFLUSHING)) {
			spin_unlock(&ip->i_flags_lock);
			continue;
		}

		 
		if (!xfs_ilock_nowait(ip, XFS_ILOCK_SHARED)) {
			spin_unlock(&ip->i_flags_lock);
			continue;
		}
		__xfs_iflags_set(ip, XFS_IFLUSHING);
		spin_unlock(&ip->i_flags_lock);

		 
		if (xlog_is_shutdown(mp->m_log)) {
			xfs_iunpin_wait(ip);
			xfs_iflush_abort(ip);
			xfs_iunlock(ip, XFS_ILOCK_SHARED);
			error = -EIO;
			continue;
		}

		 
		if (xfs_ipincount(ip)) {
			xfs_iflags_clear(ip, XFS_IFLUSHING);
			xfs_iunlock(ip, XFS_ILOCK_SHARED);
			continue;
		}

		if (!xfs_inode_clean(ip))
			error = xfs_iflush(ip, bp);
		else
			xfs_iflags_clear(ip, XFS_IFLUSHING);
		xfs_iunlock(ip, XFS_ILOCK_SHARED);
		if (error)
			break;
		clcount++;
	}

	if (error) {
		 
		xfs_force_shutdown(mp, SHUTDOWN_CORRUPT_INCORE);
		bp->b_flags |= XBF_ASYNC;
		xfs_buf_ioend_fail(bp);
		return error;
	}

	if (!clcount)
		return -EAGAIN;

	XFS_STATS_INC(mp, xs_icluster_flushcnt);
	XFS_STATS_ADD(mp, xs_icluster_flushinode, clcount);
	return 0;

}

 
void
xfs_irele(
	struct xfs_inode	*ip)
{
	trace_xfs_irele(ip, _RET_IP_);
	iput(VFS_I(ip));
}

 
int
xfs_log_force_inode(
	struct xfs_inode	*ip)
{
	xfs_csn_t		seq = 0;

	xfs_ilock(ip, XFS_ILOCK_SHARED);
	if (xfs_ipincount(ip))
		seq = ip->i_itemp->ili_commit_seq;
	xfs_iunlock(ip, XFS_ILOCK_SHARED);

	if (!seq)
		return 0;
	return xfs_log_force_seq(ip->i_mount, seq, XFS_LOG_SYNC, NULL);
}

 
static int
xfs_iolock_two_inodes_and_break_layout(
	struct inode		*src,
	struct inode		*dest)
{
	int			error;

	if (src > dest)
		swap(src, dest);

retry:
	 
	error = break_layout(src, true);
	if (error)
		return error;
	if (src != dest) {
		error = break_layout(dest, true);
		if (error)
			return error;
	}

	 
	inode_lock(src);
	error = break_layout(src, false);
	if (error) {
		inode_unlock(src);
		if (error == -EWOULDBLOCK)
			goto retry;
		return error;
	}

	if (src == dest)
		return 0;

	 
	inode_lock_nested(dest, I_MUTEX_NONDIR2);
	error = break_layout(dest, false);
	if (error) {
		inode_unlock(src);
		inode_unlock(dest);
		if (error == -EWOULDBLOCK)
			goto retry;
		return error;
	}

	return 0;
}

static int
xfs_mmaplock_two_inodes_and_break_dax_layout(
	struct xfs_inode	*ip1,
	struct xfs_inode	*ip2)
{
	int			error;
	bool			retry;
	struct page		*page;

	if (ip1->i_ino > ip2->i_ino)
		swap(ip1, ip2);

again:
	retry = false;
	 
	xfs_ilock(ip1, XFS_MMAPLOCK_EXCL);
	error = xfs_break_dax_layouts(VFS_I(ip1), &retry);
	if (error || retry) {
		xfs_iunlock(ip1, XFS_MMAPLOCK_EXCL);
		if (error == 0 && retry)
			goto again;
		return error;
	}

	if (ip1 == ip2)
		return 0;

	 
	xfs_ilock(ip2, xfs_lock_inumorder(XFS_MMAPLOCK_EXCL, 1));
	 
	page = dax_layout_busy_page(VFS_I(ip2)->i_mapping);
	if (page && page_ref_count(page) != 1) {
		xfs_iunlock(ip2, XFS_MMAPLOCK_EXCL);
		xfs_iunlock(ip1, XFS_MMAPLOCK_EXCL);
		goto again;
	}

	return 0;
}

 
int
xfs_ilock2_io_mmap(
	struct xfs_inode	*ip1,
	struct xfs_inode	*ip2)
{
	int			ret;

	ret = xfs_iolock_two_inodes_and_break_layout(VFS_I(ip1), VFS_I(ip2));
	if (ret)
		return ret;

	if (IS_DAX(VFS_I(ip1)) && IS_DAX(VFS_I(ip2))) {
		ret = xfs_mmaplock_two_inodes_and_break_dax_layout(ip1, ip2);
		if (ret) {
			inode_unlock(VFS_I(ip2));
			if (ip1 != ip2)
				inode_unlock(VFS_I(ip1));
			return ret;
		}
	} else
		filemap_invalidate_lock_two(VFS_I(ip1)->i_mapping,
					    VFS_I(ip2)->i_mapping);

	return 0;
}

 
void
xfs_iunlock2_io_mmap(
	struct xfs_inode	*ip1,
	struct xfs_inode	*ip2)
{
	if (IS_DAX(VFS_I(ip1)) && IS_DAX(VFS_I(ip2))) {
		xfs_iunlock(ip2, XFS_MMAPLOCK_EXCL);
		if (ip1 != ip2)
			xfs_iunlock(ip1, XFS_MMAPLOCK_EXCL);
	} else
		filemap_invalidate_unlock_two(VFS_I(ip1)->i_mapping,
					      VFS_I(ip2)->i_mapping);

	inode_unlock(VFS_I(ip2));
	if (ip1 != ip2)
		inode_unlock(VFS_I(ip1));
}

 
int
xfs_inode_reload_unlinked_bucket(
	struct xfs_trans	*tp,
	struct xfs_inode	*ip)
{
	struct xfs_mount	*mp = tp->t_mountp;
	struct xfs_buf		*agibp;
	struct xfs_agi		*agi;
	struct xfs_perag	*pag;
	xfs_agnumber_t		agno = XFS_INO_TO_AGNO(mp, ip->i_ino);
	xfs_agino_t		agino = XFS_INO_TO_AGINO(mp, ip->i_ino);
	xfs_agino_t		prev_agino, next_agino;
	unsigned int		bucket;
	bool			foundit = false;
	int			error;

	 
	pag = xfs_perag_get(mp, agno);
	error = xfs_ialloc_read_agi(pag, tp, &agibp);
	xfs_perag_put(pag);
	if (error)
		return error;

	 
	if (!xfs_inode_unlinked_incomplete(ip)) {
		foundit = true;
		goto out_agibp;
	}

	bucket = agino % XFS_AGI_UNLINKED_BUCKETS;
	agi = agibp->b_addr;

	trace_xfs_inode_reload_unlinked_bucket(ip);

	xfs_info_ratelimited(mp,
 "Found unrecovered unlinked inode 0x%x in AG 0x%x.  Initiating list recovery.",
			agino, agno);

	prev_agino = NULLAGINO;
	next_agino = be32_to_cpu(agi->agi_unlinked[bucket]);
	while (next_agino != NULLAGINO) {
		struct xfs_inode	*next_ip = NULL;

		 
		if (next_agino == agino) {
			next_ip = ip;
			next_ip->i_prev_unlinked = prev_agino;
			foundit = true;
			goto next_inode;
		}

		 
		next_ip = xfs_iunlink_lookup(pag, next_agino);
		if (next_ip)
			goto next_inode;

		 
		error = xfs_iunlink_reload_next(tp, agibp, prev_agino,
				next_agino);
		if (error)
			break;

		 
		next_ip = xfs_iunlink_lookup(pag, next_agino);
		if (!next_ip) {
			 
			ASSERT(next_ip != NULL);
			error = -EFSCORRUPTED;
			break;
		}

next_inode:
		prev_agino = next_agino;
		next_agino = next_ip->i_next_unlinked;
	}

out_agibp:
	xfs_trans_brelse(tp, agibp);
	 
	if (!error && !foundit)
		error = -EFSCORRUPTED;
	return error;
}

 
int
xfs_inode_reload_unlinked(
	struct xfs_inode	*ip)
{
	struct xfs_trans	*tp;
	int			error;

	error = xfs_trans_alloc_empty(ip->i_mount, &tp);
	if (error)
		return error;

	xfs_ilock(ip, XFS_ILOCK_SHARED);
	if (xfs_inode_unlinked_incomplete(ip))
		error = xfs_inode_reload_unlinked_bucket(tp, ip);
	xfs_iunlock(ip, XFS_ILOCK_SHARED);
	xfs_trans_cancel(tp);

	return error;
}
