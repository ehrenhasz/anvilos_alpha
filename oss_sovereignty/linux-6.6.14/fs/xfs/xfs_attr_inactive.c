
 
#include "xfs.h"
#include "xfs_fs.h"
#include "xfs_shared.h"
#include "xfs_format.h"
#include "xfs_log_format.h"
#include "xfs_trans_resv.h"
#include "xfs_bit.h"
#include "xfs_mount.h"
#include "xfs_da_format.h"
#include "xfs_da_btree.h"
#include "xfs_inode.h"
#include "xfs_attr.h"
#include "xfs_attr_remote.h"
#include "xfs_trans.h"
#include "xfs_bmap.h"
#include "xfs_attr_leaf.h"
#include "xfs_quota.h"
#include "xfs_dir2.h"
#include "xfs_error.h"

 
STATIC int
xfs_attr3_rmt_stale(
	struct xfs_inode	*dp,
	xfs_dablk_t		blkno,
	int			blkcnt)
{
	struct xfs_bmbt_irec	map;
	int			nmap;
	int			error;

	 
	while (blkcnt > 0) {
		 
		nmap = 1;
		error = xfs_bmapi_read(dp, (xfs_fileoff_t)blkno, blkcnt,
				       &map, &nmap, XFS_BMAPI_ATTRFORK);
		if (error)
			return error;
		if (XFS_IS_CORRUPT(dp->i_mount, nmap != 1))
			return -EFSCORRUPTED;

		 
		error = xfs_attr_rmtval_stale(dp, &map, 0);
		if (error)
			return error;

		blkno += map.br_blockcount;
		blkcnt -= map.br_blockcount;
	}

	return 0;
}

 
STATIC int
xfs_attr3_leaf_inactive(
	struct xfs_trans		**trans,
	struct xfs_inode		*dp,
	struct xfs_buf			*bp)
{
	struct xfs_attr3_icleaf_hdr	ichdr;
	struct xfs_mount		*mp = bp->b_mount;
	struct xfs_attr_leafblock	*leaf = bp->b_addr;
	struct xfs_attr_leaf_entry	*entry;
	struct xfs_attr_leaf_name_remote *name_rmt;
	int				error = 0;
	int				i;

	xfs_attr3_leaf_hdr_from_disk(mp->m_attr_geo, &ichdr, leaf);

	 
	entry = xfs_attr3_leaf_entryp(leaf);
	for (i = 0; i < ichdr.count; entry++, i++) {
		int		blkcnt;

		if (!entry->nameidx || (entry->flags & XFS_ATTR_LOCAL))
			continue;

		name_rmt = xfs_attr3_leaf_name_remote(leaf, i);
		if (!name_rmt->valueblk)
			continue;

		blkcnt = xfs_attr3_rmt_blocks(dp->i_mount,
				be32_to_cpu(name_rmt->valuelen));
		error = xfs_attr3_rmt_stale(dp,
				be32_to_cpu(name_rmt->valueblk), blkcnt);
		if (error)
			goto err;
	}

	xfs_trans_brelse(*trans, bp);
err:
	return error;
}

 
STATIC int
xfs_attr3_node_inactive(
	struct xfs_trans	**trans,
	struct xfs_inode	*dp,
	struct xfs_buf		*bp,
	int			level)
{
	struct xfs_mount	*mp = dp->i_mount;
	struct xfs_da_blkinfo	*info;
	xfs_dablk_t		child_fsb;
	xfs_daddr_t		parent_blkno, child_blkno;
	struct xfs_buf		*child_bp;
	struct xfs_da3_icnode_hdr ichdr;
	int			error, i;

	 
	if (level > XFS_DA_NODE_MAXDEPTH) {
		xfs_buf_mark_corrupt(bp);
		xfs_trans_brelse(*trans, bp);	 
		return -EFSCORRUPTED;
	}

	xfs_da3_node_hdr_from_disk(dp->i_mount, &ichdr, bp->b_addr);
	parent_blkno = xfs_buf_daddr(bp);
	if (!ichdr.count) {
		xfs_trans_brelse(*trans, bp);
		return 0;
	}
	child_fsb = be32_to_cpu(ichdr.btree[0].before);
	xfs_trans_brelse(*trans, bp);	 
	bp = NULL;

	 
	for (i = 0; i < ichdr.count; i++) {
		 
		error = xfs_da3_node_read(*trans, dp, child_fsb, &child_bp,
					  XFS_ATTR_FORK);
		if (error)
			return error;

		 
		child_blkno = xfs_buf_daddr(child_bp);

		 
		info = child_bp->b_addr;
		switch (info->magic) {
		case cpu_to_be16(XFS_DA_NODE_MAGIC):
		case cpu_to_be16(XFS_DA3_NODE_MAGIC):
			error = xfs_attr3_node_inactive(trans, dp, child_bp,
							level + 1);
			break;
		case cpu_to_be16(XFS_ATTR_LEAF_MAGIC):
		case cpu_to_be16(XFS_ATTR3_LEAF_MAGIC):
			error = xfs_attr3_leaf_inactive(trans, dp, child_bp);
			break;
		default:
			xfs_buf_mark_corrupt(child_bp);
			xfs_trans_brelse(*trans, child_bp);
			error = -EFSCORRUPTED;
			break;
		}
		if (error)
			return error;

		 
		error = xfs_trans_get_buf(*trans, mp->m_ddev_targp,
				child_blkno,
				XFS_FSB_TO_BB(mp, mp->m_attr_geo->fsbcount), 0,
				&child_bp);
		if (error)
			return error;
		xfs_trans_binval(*trans, child_bp);
		child_bp = NULL;

		 
		if (i + 1 < ichdr.count) {
			struct xfs_da3_icnode_hdr phdr;

			error = xfs_da3_node_read_mapped(*trans, dp,
					parent_blkno, &bp, XFS_ATTR_FORK);
			if (error)
				return error;
			xfs_da3_node_hdr_from_disk(dp->i_mount, &phdr,
						  bp->b_addr);
			child_fsb = be32_to_cpu(phdr.btree[i + 1].before);
			xfs_trans_brelse(*trans, bp);
			bp = NULL;
		}
		 
		error = xfs_trans_roll_inode(trans, dp);
		if (error)
			return  error;
	}

	return 0;
}

 
static int
xfs_attr3_root_inactive(
	struct xfs_trans	**trans,
	struct xfs_inode	*dp)
{
	struct xfs_mount	*mp = dp->i_mount;
	struct xfs_da_blkinfo	*info;
	struct xfs_buf		*bp;
	xfs_daddr_t		blkno;
	int			error;

	 
	error = xfs_da3_node_read(*trans, dp, 0, &bp, XFS_ATTR_FORK);
	if (error)
		return error;
	blkno = xfs_buf_daddr(bp);

	 
	info = bp->b_addr;
	switch (info->magic) {
	case cpu_to_be16(XFS_DA_NODE_MAGIC):
	case cpu_to_be16(XFS_DA3_NODE_MAGIC):
		error = xfs_attr3_node_inactive(trans, dp, bp, 1);
		break;
	case cpu_to_be16(XFS_ATTR_LEAF_MAGIC):
	case cpu_to_be16(XFS_ATTR3_LEAF_MAGIC):
		error = xfs_attr3_leaf_inactive(trans, dp, bp);
		break;
	default:
		error = -EFSCORRUPTED;
		xfs_buf_mark_corrupt(bp);
		xfs_trans_brelse(*trans, bp);
		break;
	}
	if (error)
		return error;

	 
	error = xfs_trans_get_buf(*trans, mp->m_ddev_targp, blkno,
			XFS_FSB_TO_BB(mp, mp->m_attr_geo->fsbcount), 0, &bp);
	if (error)
		return error;
	error = bp->b_error;
	if (error) {
		xfs_trans_brelse(*trans, bp);
		return error;
	}
	xfs_trans_binval(*trans, bp);	 
	 
	error = xfs_trans_roll_inode(trans, dp);

	return error;
}

 
int
xfs_attr_inactive(
	struct xfs_inode	*dp)
{
	struct xfs_trans	*trans;
	struct xfs_mount	*mp;
	int			lock_mode = XFS_ILOCK_SHARED;
	int			error = 0;

	mp = dp->i_mount;

	xfs_ilock(dp, lock_mode);
	if (!xfs_inode_has_attr_fork(dp))
		goto out_destroy_fork;
	xfs_iunlock(dp, lock_mode);

	lock_mode = 0;

	error = xfs_trans_alloc(mp, &M_RES(mp)->tr_attrinval, 0, 0, 0, &trans);
	if (error)
		goto out_destroy_fork;

	lock_mode = XFS_ILOCK_EXCL;
	xfs_ilock(dp, lock_mode);

	if (!xfs_inode_has_attr_fork(dp))
		goto out_cancel;

	 
	xfs_trans_ijoin(trans, dp, 0);

	 
	if (dp->i_af.if_nextents > 0) {
		error = xfs_attr3_root_inactive(&trans, dp);
		if (error)
			goto out_cancel;

		error = xfs_itruncate_extents(&trans, dp, XFS_ATTR_FORK, 0);
		if (error)
			goto out_cancel;
	}

	 
	xfs_attr_fork_remove(dp, trans);

	error = xfs_trans_commit(trans);
	xfs_iunlock(dp, lock_mode);
	return error;

out_cancel:
	xfs_trans_cancel(trans);
out_destroy_fork:
	 
	xfs_ifork_zap_attr(dp);
	if (lock_mode)
		xfs_iunlock(dp, lock_mode);
	return error;
}
