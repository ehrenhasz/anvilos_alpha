
 

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
#include "xfs_btree.h"
#include "xfs_bmap_btree.h"
#include "xfs_bmap.h"
#include "xfs_error.h"
#include "xfs_trace.h"
#include "xfs_da_format.h"
#include "xfs_da_btree.h"
#include "xfs_dir2_priv.h"
#include "xfs_attr_leaf.h"
#include "xfs_types.h"
#include "xfs_errortag.h"

struct kmem_cache *xfs_ifork_cache;

void
xfs_init_local_fork(
	struct xfs_inode	*ip,
	int			whichfork,
	const void		*data,
	int64_t			size)
{
	struct xfs_ifork	*ifp = xfs_ifork_ptr(ip, whichfork);
	int			mem_size = size;
	bool			zero_terminate;

	 
	zero_terminate = S_ISLNK(VFS_I(ip)->i_mode);
	if (zero_terminate)
		mem_size++;

	if (size) {
		ifp->if_u1.if_data = kmem_alloc(mem_size, KM_NOFS);
		memcpy(ifp->if_u1.if_data, data, size);
		if (zero_terminate)
			ifp->if_u1.if_data[size] = '\0';
	} else {
		ifp->if_u1.if_data = NULL;
	}

	ifp->if_bytes = size;
}

 
STATIC int
xfs_iformat_local(
	struct xfs_inode	*ip,
	struct xfs_dinode	*dip,
	int			whichfork,
	int			size)
{
	 
	if (unlikely(size > XFS_DFORK_SIZE(dip, ip->i_mount, whichfork))) {
		xfs_warn(ip->i_mount,
	"corrupt inode %llu (bad size %d for local fork, size = %zd).",
			(unsigned long long) ip->i_ino, size,
			XFS_DFORK_SIZE(dip, ip->i_mount, whichfork));
		xfs_inode_verifier_error(ip, -EFSCORRUPTED,
				"xfs_iformat_local", dip, sizeof(*dip),
				__this_address);
		return -EFSCORRUPTED;
	}

	xfs_init_local_fork(ip, whichfork, XFS_DFORK_PTR(dip, whichfork), size);
	return 0;
}

 
STATIC int
xfs_iformat_extents(
	struct xfs_inode	*ip,
	struct xfs_dinode	*dip,
	int			whichfork)
{
	struct xfs_mount	*mp = ip->i_mount;
	struct xfs_ifork	*ifp = xfs_ifork_ptr(ip, whichfork);
	int			state = xfs_bmap_fork_to_state(whichfork);
	xfs_extnum_t		nex = xfs_dfork_nextents(dip, whichfork);
	int			size = nex * sizeof(xfs_bmbt_rec_t);
	struct xfs_iext_cursor	icur;
	struct xfs_bmbt_rec	*dp;
	struct xfs_bmbt_irec	new;
	int			i;

	 
	if (unlikely(size < 0 || size > XFS_DFORK_SIZE(dip, mp, whichfork))) {
		xfs_warn(ip->i_mount, "corrupt inode %llu ((a)extents = %llu).",
			ip->i_ino, nex);
		xfs_inode_verifier_error(ip, -EFSCORRUPTED,
				"xfs_iformat_extents(1)", dip, sizeof(*dip),
				__this_address);
		return -EFSCORRUPTED;
	}

	ifp->if_bytes = 0;
	ifp->if_u1.if_root = NULL;
	ifp->if_height = 0;
	if (size) {
		dp = (xfs_bmbt_rec_t *) XFS_DFORK_PTR(dip, whichfork);

		xfs_iext_first(ifp, &icur);
		for (i = 0; i < nex; i++, dp++) {
			xfs_failaddr_t	fa;

			xfs_bmbt_disk_get_all(dp, &new);
			fa = xfs_bmap_validate_extent(ip, whichfork, &new);
			if (fa) {
				xfs_inode_verifier_error(ip, -EFSCORRUPTED,
						"xfs_iformat_extents(2)",
						dp, sizeof(*dp), fa);
				return xfs_bmap_complain_bad_rec(ip, whichfork,
						fa, &new);
			}

			xfs_iext_insert(ip, &icur, &new, state);
			trace_xfs_read_extent(ip, &icur, state, _THIS_IP_);
			xfs_iext_next(ifp, &icur);
		}
	}
	return 0;
}

 
STATIC int
xfs_iformat_btree(
	struct xfs_inode	*ip,
	struct xfs_dinode	*dip,
	int			whichfork)
{
	struct xfs_mount	*mp = ip->i_mount;
	xfs_bmdr_block_t	*dfp;
	struct xfs_ifork	*ifp;
	 
	int			nrecs;
	int			size;
	int			level;

	ifp = xfs_ifork_ptr(ip, whichfork);
	dfp = (xfs_bmdr_block_t *)XFS_DFORK_PTR(dip, whichfork);
	size = XFS_BMAP_BROOT_SPACE(mp, dfp);
	nrecs = be16_to_cpu(dfp->bb_numrecs);
	level = be16_to_cpu(dfp->bb_level);

	 
	if (unlikely(ifp->if_nextents <= XFS_IFORK_MAXEXT(ip, whichfork) ||
		     nrecs == 0 ||
		     XFS_BMDR_SPACE_CALC(nrecs) >
					XFS_DFORK_SIZE(dip, mp, whichfork) ||
		     ifp->if_nextents > ip->i_nblocks) ||
		     level == 0 || level > XFS_BM_MAXLEVELS(mp, whichfork)) {
		xfs_warn(mp, "corrupt inode %llu (btree).",
					(unsigned long long) ip->i_ino);
		xfs_inode_verifier_error(ip, -EFSCORRUPTED,
				"xfs_iformat_btree", dfp, size,
				__this_address);
		return -EFSCORRUPTED;
	}

	ifp->if_broot_bytes = size;
	ifp->if_broot = kmem_alloc(size, KM_NOFS);
	ASSERT(ifp->if_broot != NULL);
	 
	xfs_bmdr_to_bmbt(ip, dfp, XFS_DFORK_SIZE(dip, ip->i_mount, whichfork),
			 ifp->if_broot, size);

	ifp->if_bytes = 0;
	ifp->if_u1.if_root = NULL;
	ifp->if_height = 0;
	return 0;
}

int
xfs_iformat_data_fork(
	struct xfs_inode	*ip,
	struct xfs_dinode	*dip)
{
	struct inode		*inode = VFS_I(ip);
	int			error;

	 
	ip->i_df.if_format = dip->di_format;
	ip->i_df.if_nextents = xfs_dfork_data_extents(dip);
	smp_store_release(&ip->i_df.if_needextents,
			   ip->i_df.if_format == XFS_DINODE_FMT_BTREE ? 1 : 0);

	switch (inode->i_mode & S_IFMT) {
	case S_IFIFO:
	case S_IFCHR:
	case S_IFBLK:
	case S_IFSOCK:
		ip->i_disk_size = 0;
		inode->i_rdev = xfs_to_linux_dev_t(xfs_dinode_get_rdev(dip));
		return 0;
	case S_IFREG:
	case S_IFLNK:
	case S_IFDIR:
		switch (ip->i_df.if_format) {
		case XFS_DINODE_FMT_LOCAL:
			error = xfs_iformat_local(ip, dip, XFS_DATA_FORK,
					be64_to_cpu(dip->di_size));
			if (!error)
				error = xfs_ifork_verify_local_data(ip);
			return error;
		case XFS_DINODE_FMT_EXTENTS:
			return xfs_iformat_extents(ip, dip, XFS_DATA_FORK);
		case XFS_DINODE_FMT_BTREE:
			return xfs_iformat_btree(ip, dip, XFS_DATA_FORK);
		default:
			xfs_inode_verifier_error(ip, -EFSCORRUPTED, __func__,
					dip, sizeof(*dip), __this_address);
			return -EFSCORRUPTED;
		}
		break;
	default:
		xfs_inode_verifier_error(ip, -EFSCORRUPTED, __func__, dip,
				sizeof(*dip), __this_address);
		return -EFSCORRUPTED;
	}
}

static uint16_t
xfs_dfork_attr_shortform_size(
	struct xfs_dinode		*dip)
{
	struct xfs_attr_shortform	*atp =
		(struct xfs_attr_shortform *)XFS_DFORK_APTR(dip);

	return be16_to_cpu(atp->hdr.totsize);
}

void
xfs_ifork_init_attr(
	struct xfs_inode	*ip,
	enum xfs_dinode_fmt	format,
	xfs_extnum_t		nextents)
{
	 
	ip->i_af.if_format = format;
	ip->i_af.if_nextents = nextents;
	smp_store_release(&ip->i_af.if_needextents,
			   ip->i_af.if_format == XFS_DINODE_FMT_BTREE ? 1 : 0);
}

void
xfs_ifork_zap_attr(
	struct xfs_inode	*ip)
{
	xfs_idestroy_fork(&ip->i_af);
	memset(&ip->i_af, 0, sizeof(struct xfs_ifork));
	ip->i_af.if_format = XFS_DINODE_FMT_EXTENTS;
}

int
xfs_iformat_attr_fork(
	struct xfs_inode	*ip,
	struct xfs_dinode	*dip)
{
	xfs_extnum_t		naextents = xfs_dfork_attr_extents(dip);
	int			error = 0;

	 
	xfs_ifork_init_attr(ip, dip->di_aformat, naextents);

	switch (ip->i_af.if_format) {
	case XFS_DINODE_FMT_LOCAL:
		error = xfs_iformat_local(ip, dip, XFS_ATTR_FORK,
				xfs_dfork_attr_shortform_size(dip));
		if (!error)
			error = xfs_ifork_verify_local_attr(ip);
		break;
	case XFS_DINODE_FMT_EXTENTS:
		error = xfs_iformat_extents(ip, dip, XFS_ATTR_FORK);
		break;
	case XFS_DINODE_FMT_BTREE:
		error = xfs_iformat_btree(ip, dip, XFS_ATTR_FORK);
		break;
	default:
		xfs_inode_verifier_error(ip, error, __func__, dip,
				sizeof(*dip), __this_address);
		error = -EFSCORRUPTED;
		break;
	}

	if (error)
		xfs_ifork_zap_attr(ip);
	return error;
}

 
void
xfs_iroot_realloc(
	xfs_inode_t		*ip,
	int			rec_diff,
	int			whichfork)
{
	struct xfs_mount	*mp = ip->i_mount;
	int			cur_max;
	struct xfs_ifork	*ifp;
	struct xfs_btree_block	*new_broot;
	int			new_max;
	size_t			new_size;
	char			*np;
	char			*op;

	 
	if (rec_diff == 0) {
		return;
	}

	ifp = xfs_ifork_ptr(ip, whichfork);
	if (rec_diff > 0) {
		 
		if (ifp->if_broot_bytes == 0) {
			new_size = XFS_BMAP_BROOT_SPACE_CALC(mp, rec_diff);
			ifp->if_broot = kmem_alloc(new_size, KM_NOFS);
			ifp->if_broot_bytes = (int)new_size;
			return;
		}

		 
		cur_max = xfs_bmbt_maxrecs(mp, ifp->if_broot_bytes, 0);
		new_max = cur_max + rec_diff;
		new_size = XFS_BMAP_BROOT_SPACE_CALC(mp, new_max);
		ifp->if_broot = krealloc(ifp->if_broot, new_size,
					 GFP_NOFS | __GFP_NOFAIL);
		op = (char *)XFS_BMAP_BROOT_PTR_ADDR(mp, ifp->if_broot, 1,
						     ifp->if_broot_bytes);
		np = (char *)XFS_BMAP_BROOT_PTR_ADDR(mp, ifp->if_broot, 1,
						     (int)new_size);
		ifp->if_broot_bytes = (int)new_size;
		ASSERT(XFS_BMAP_BMDR_SPACE(ifp->if_broot) <=
			xfs_inode_fork_size(ip, whichfork));
		memmove(np, op, cur_max * (uint)sizeof(xfs_fsblock_t));
		return;
	}

	 
	ASSERT((ifp->if_broot != NULL) && (ifp->if_broot_bytes > 0));
	cur_max = xfs_bmbt_maxrecs(mp, ifp->if_broot_bytes, 0);
	new_max = cur_max + rec_diff;
	ASSERT(new_max >= 0);
	if (new_max > 0)
		new_size = XFS_BMAP_BROOT_SPACE_CALC(mp, new_max);
	else
		new_size = 0;
	if (new_size > 0) {
		new_broot = kmem_alloc(new_size, KM_NOFS);
		 
		memcpy(new_broot, ifp->if_broot,
			XFS_BMBT_BLOCK_LEN(ip->i_mount));
	} else {
		new_broot = NULL;
	}

	 
	if (new_max > 0) {
		 
		op = (char *)XFS_BMBT_REC_ADDR(mp, ifp->if_broot, 1);
		np = (char *)XFS_BMBT_REC_ADDR(mp, new_broot, 1);
		memcpy(np, op, new_max * (uint)sizeof(xfs_bmbt_rec_t));

		 
		op = (char *)XFS_BMAP_BROOT_PTR_ADDR(mp, ifp->if_broot, 1,
						     ifp->if_broot_bytes);
		np = (char *)XFS_BMAP_BROOT_PTR_ADDR(mp, new_broot, 1,
						     (int)new_size);
		memcpy(np, op, new_max * (uint)sizeof(xfs_fsblock_t));
	}
	kmem_free(ifp->if_broot);
	ifp->if_broot = new_broot;
	ifp->if_broot_bytes = (int)new_size;
	if (ifp->if_broot)
		ASSERT(XFS_BMAP_BMDR_SPACE(ifp->if_broot) <=
			xfs_inode_fork_size(ip, whichfork));
	return;
}


 
void
xfs_idata_realloc(
	struct xfs_inode	*ip,
	int64_t			byte_diff,
	int			whichfork)
{
	struct xfs_ifork	*ifp = xfs_ifork_ptr(ip, whichfork);
	int64_t			new_size = ifp->if_bytes + byte_diff;

	ASSERT(new_size >= 0);
	ASSERT(new_size <= xfs_inode_fork_size(ip, whichfork));

	if (byte_diff == 0)
		return;

	if (new_size == 0) {
		kmem_free(ifp->if_u1.if_data);
		ifp->if_u1.if_data = NULL;
		ifp->if_bytes = 0;
		return;
	}

	ifp->if_u1.if_data = krealloc(ifp->if_u1.if_data, new_size,
				      GFP_NOFS | __GFP_NOFAIL);
	ifp->if_bytes = new_size;
}

void
xfs_idestroy_fork(
	struct xfs_ifork	*ifp)
{
	if (ifp->if_broot != NULL) {
		kmem_free(ifp->if_broot);
		ifp->if_broot = NULL;
	}

	switch (ifp->if_format) {
	case XFS_DINODE_FMT_LOCAL:
		kmem_free(ifp->if_u1.if_data);
		ifp->if_u1.if_data = NULL;
		break;
	case XFS_DINODE_FMT_EXTENTS:
	case XFS_DINODE_FMT_BTREE:
		if (ifp->if_height)
			xfs_iext_destroy(ifp);
		break;
	}
}

 
int
xfs_iextents_copy(
	struct xfs_inode	*ip,
	struct xfs_bmbt_rec	*dp,
	int			whichfork)
{
	int			state = xfs_bmap_fork_to_state(whichfork);
	struct xfs_ifork	*ifp = xfs_ifork_ptr(ip, whichfork);
	struct xfs_iext_cursor	icur;
	struct xfs_bmbt_irec	rec;
	int64_t			copied = 0;

	ASSERT(xfs_isilocked(ip, XFS_ILOCK_EXCL | XFS_ILOCK_SHARED));
	ASSERT(ifp->if_bytes > 0);

	for_each_xfs_iext(ifp, &icur, &rec) {
		if (isnullstartblock(rec.br_startblock))
			continue;
		ASSERT(xfs_bmap_validate_extent(ip, whichfork, &rec) == NULL);
		xfs_bmbt_disk_set_all(dp, &rec);
		trace_xfs_write_extent(ip, &icur, state, _RET_IP_);
		copied += sizeof(struct xfs_bmbt_rec);
		dp++;
	}

	ASSERT(copied > 0);
	ASSERT(copied <= ifp->if_bytes);
	return copied;
}

 
void
xfs_iflush_fork(
	struct xfs_inode	*ip,
	struct xfs_dinode	*dip,
	struct xfs_inode_log_item *iip,
	int			whichfork)
{
	char			*cp;
	struct xfs_ifork	*ifp;
	xfs_mount_t		*mp;
	static const short	brootflag[2] =
		{ XFS_ILOG_DBROOT, XFS_ILOG_ABROOT };
	static const short	dataflag[2] =
		{ XFS_ILOG_DDATA, XFS_ILOG_ADATA };
	static const short	extflag[2] =
		{ XFS_ILOG_DEXT, XFS_ILOG_AEXT };

	if (!iip)
		return;
	ifp = xfs_ifork_ptr(ip, whichfork);
	 
	if (!ifp) {
		ASSERT(whichfork == XFS_ATTR_FORK);
		return;
	}
	cp = XFS_DFORK_PTR(dip, whichfork);
	mp = ip->i_mount;
	switch (ifp->if_format) {
	case XFS_DINODE_FMT_LOCAL:
		if ((iip->ili_fields & dataflag[whichfork]) &&
		    (ifp->if_bytes > 0)) {
			ASSERT(ifp->if_u1.if_data != NULL);
			ASSERT(ifp->if_bytes <= xfs_inode_fork_size(ip, whichfork));
			memcpy(cp, ifp->if_u1.if_data, ifp->if_bytes);
		}
		break;

	case XFS_DINODE_FMT_EXTENTS:
		if ((iip->ili_fields & extflag[whichfork]) &&
		    (ifp->if_bytes > 0)) {
			ASSERT(ifp->if_nextents > 0);
			(void)xfs_iextents_copy(ip, (xfs_bmbt_rec_t *)cp,
				whichfork);
		}
		break;

	case XFS_DINODE_FMT_BTREE:
		if ((iip->ili_fields & brootflag[whichfork]) &&
		    (ifp->if_broot_bytes > 0)) {
			ASSERT(ifp->if_broot != NULL);
			ASSERT(XFS_BMAP_BMDR_SPACE(ifp->if_broot) <=
			        xfs_inode_fork_size(ip, whichfork));
			xfs_bmbt_to_bmdr(mp, ifp->if_broot, ifp->if_broot_bytes,
				(xfs_bmdr_block_t *)cp,
				XFS_DFORK_SIZE(dip, mp, whichfork));
		}
		break;

	case XFS_DINODE_FMT_DEV:
		if (iip->ili_fields & XFS_ILOG_DEV) {
			ASSERT(whichfork == XFS_DATA_FORK);
			xfs_dinode_put_rdev(dip,
					linux_to_xfs_dev_t(VFS_I(ip)->i_rdev));
		}
		break;

	default:
		ASSERT(0);
		break;
	}
}

 
struct xfs_ifork *
xfs_iext_state_to_fork(
	struct xfs_inode	*ip,
	int			state)
{
	if (state & BMAP_COWFORK)
		return ip->i_cowfp;
	else if (state & BMAP_ATTRFORK)
		return &ip->i_af;
	return &ip->i_df;
}

 
void
xfs_ifork_init_cow(
	struct xfs_inode	*ip)
{
	if (ip->i_cowfp)
		return;

	ip->i_cowfp = kmem_cache_zalloc(xfs_ifork_cache,
				       GFP_NOFS | __GFP_NOFAIL);
	ip->i_cowfp->if_format = XFS_DINODE_FMT_EXTENTS;
}

 
int
xfs_ifork_verify_local_data(
	struct xfs_inode	*ip)
{
	xfs_failaddr_t		fa = NULL;

	switch (VFS_I(ip)->i_mode & S_IFMT) {
	case S_IFDIR:
		fa = xfs_dir2_sf_verify(ip);
		break;
	case S_IFLNK:
		fa = xfs_symlink_shortform_verify(ip);
		break;
	default:
		break;
	}

	if (fa) {
		xfs_inode_verifier_error(ip, -EFSCORRUPTED, "data fork",
				ip->i_df.if_u1.if_data, ip->i_df.if_bytes, fa);
		return -EFSCORRUPTED;
	}

	return 0;
}

 
int
xfs_ifork_verify_local_attr(
	struct xfs_inode	*ip)
{
	struct xfs_ifork	*ifp = &ip->i_af;
	xfs_failaddr_t		fa;

	if (!xfs_inode_has_attr_fork(ip))
		fa = __this_address;
	else
		fa = xfs_attr_shortform_verify(ip);

	if (fa) {
		xfs_inode_verifier_error(ip, -EFSCORRUPTED, "attr fork",
				ifp->if_u1.if_data, ifp->if_bytes, fa);
		return -EFSCORRUPTED;
	}

	return 0;
}

int
xfs_iext_count_may_overflow(
	struct xfs_inode	*ip,
	int			whichfork,
	int			nr_to_add)
{
	struct xfs_ifork	*ifp = xfs_ifork_ptr(ip, whichfork);
	uint64_t		max_exts;
	uint64_t		nr_exts;

	if (whichfork == XFS_COW_FORK)
		return 0;

	max_exts = xfs_iext_max_nextents(xfs_inode_has_large_extent_counts(ip),
				whichfork);

	if (XFS_TEST_ERROR(false, ip->i_mount, XFS_ERRTAG_REDUCE_MAX_IEXTENTS))
		max_exts = 10;

	nr_exts = ifp->if_nextents + nr_to_add;
	if (nr_exts < ifp->if_nextents || nr_exts > max_exts)
		return -EFBIG;

	return 0;
}

 
int
xfs_iext_count_upgrade(
	struct xfs_trans	*tp,
	struct xfs_inode	*ip,
	uint			nr_to_add)
{
	ASSERT(nr_to_add <= XFS_MAX_EXTCNT_UPGRADE_NR);

	if (!xfs_has_large_extent_counts(ip->i_mount) ||
	    xfs_inode_has_large_extent_counts(ip) ||
	    XFS_TEST_ERROR(false, ip->i_mount, XFS_ERRTAG_REDUCE_MAX_IEXTENTS))
		return -EFBIG;

	ip->i_diflags2 |= XFS_DIFLAG2_NREXT64;
	xfs_trans_log_inode(tp, ip, XFS_ILOG_CORE);

	return 0;
}
