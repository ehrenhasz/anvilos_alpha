
 
#include "xfs.h"
#include "xfs_fs.h"
#include "xfs_shared.h"
#include "xfs_format.h"
#include "xfs_log_format.h"
#include "xfs_trans_resv.h"
#include "xfs_mount.h"
#include "xfs_da_format.h"
#include "xfs_da_btree.h"
#include "xfs_inode.h"
#include "xfs_bmap_btree.h"
#include "xfs_quota.h"
#include "xfs_trans.h"
#include "xfs_qm.h"
#include "xfs_trans_space.h"

#define _ALLOC	true
#define _FREE	false

 
STATIC uint
xfs_buf_log_overhead(void)
{
	return round_up(sizeof(struct xlog_op_header) +
			sizeof(struct xfs_buf_log_format), 128);
}

 
STATIC uint
xfs_calc_buf_res(
	uint		nbufs,
	uint		size)
{
	return nbufs * (size + xfs_buf_log_overhead());
}

 
uint
xfs_allocfree_block_count(
	struct xfs_mount *mp,
	uint		num_ops)
{
	uint		blocks;

	blocks = num_ops * 2 * (2 * mp->m_alloc_maxlevels - 1);
	if (xfs_has_rmapbt(mp))
		blocks += num_ops * (2 * mp->m_rmap_maxlevels - 1);

	return blocks;
}

 
static unsigned int
xfs_refcountbt_block_count(
	struct xfs_mount	*mp,
	unsigned int		num_ops)
{
	return num_ops * (2 * mp->m_refc_maxlevels - 1);
}

 
STATIC uint
xfs_calc_inode_res(
	struct xfs_mount	*mp,
	uint			ninodes)
{
	return ninodes *
		(4 * sizeof(struct xlog_op_header) +
		 sizeof(struct xfs_inode_log_format) +
		 mp->m_sb.sb_inodesize +
		 2 * XFS_BMBT_BLOCK_LEN(mp));
}

 
STATIC uint
xfs_calc_inobt_res(
	struct xfs_mount	*mp)
{
	return xfs_calc_buf_res(M_IGEO(mp)->inobt_maxlevels,
			XFS_FSB_TO_B(mp, 1)) +
				xfs_calc_buf_res(xfs_allocfree_block_count(mp, 1),
			XFS_FSB_TO_B(mp, 1));
}

 
STATIC uint
xfs_calc_finobt_res(
	struct xfs_mount	*mp)
{
	if (!xfs_has_finobt(mp))
		return 0;

	return xfs_calc_inobt_res(mp);
}

 
STATIC uint
xfs_calc_inode_chunk_res(
	struct xfs_mount	*mp,
	bool			alloc)
{
	uint			res, size = 0;

	res = xfs_calc_buf_res(xfs_allocfree_block_count(mp, 1),
			       XFS_FSB_TO_B(mp, 1));
	if (alloc) {
		 
		if (xfs_has_v3inodes(mp))
			return res;
		size = XFS_FSB_TO_B(mp, 1);
	}

	res += xfs_calc_buf_res(M_IGEO(mp)->ialloc_blks, size);
	return res;
}

 
static unsigned int
xfs_rtalloc_block_count(
	struct xfs_mount	*mp,
	unsigned int		num_ops)
{
	unsigned int		blksz = XFS_FSB_TO_B(mp, 1);
	unsigned int		rtbmp_bytes;

	rtbmp_bytes = (XFS_MAX_BMBT_EXTLEN / mp->m_sb.sb_rextsize) / NBBY;
	return (howmany(rtbmp_bytes, blksz) + 1) * num_ops;
}

 

 
static unsigned int
xfs_calc_refcountbt_reservation(
	struct xfs_mount	*mp,
	unsigned int		nr_ops)
{
	unsigned int		blksz = XFS_FSB_TO_B(mp, 1);

	if (!xfs_has_reflink(mp))
		return 0;

	return xfs_calc_buf_res(nr_ops, mp->m_sb.sb_sectsize) +
	       xfs_calc_buf_res(xfs_refcountbt_block_count(mp, nr_ops), blksz);
}

 
STATIC uint
xfs_calc_write_reservation(
	struct xfs_mount	*mp,
	bool			for_minlogsize)
{
	unsigned int		t1, t2, t3, t4;
	unsigned int		blksz = XFS_FSB_TO_B(mp, 1);

	t1 = xfs_calc_inode_res(mp, 1) +
	     xfs_calc_buf_res(XFS_BM_MAXLEVELS(mp, XFS_DATA_FORK), blksz) +
	     xfs_calc_buf_res(3, mp->m_sb.sb_sectsize) +
	     xfs_calc_buf_res(xfs_allocfree_block_count(mp, 2), blksz);

	if (xfs_has_realtime(mp)) {
		t2 = xfs_calc_inode_res(mp, 1) +
		     xfs_calc_buf_res(XFS_BM_MAXLEVELS(mp, XFS_DATA_FORK),
				     blksz) +
		     xfs_calc_buf_res(3, mp->m_sb.sb_sectsize) +
		     xfs_calc_buf_res(xfs_rtalloc_block_count(mp, 1), blksz) +
		     xfs_calc_buf_res(xfs_allocfree_block_count(mp, 1), blksz);
	} else {
		t2 = 0;
	}

	t3 = xfs_calc_buf_res(5, mp->m_sb.sb_sectsize) +
	     xfs_calc_buf_res(xfs_allocfree_block_count(mp, 2), blksz);

	 
	if (for_minlogsize) {
		unsigned int	adj = 0;

		if (xfs_has_reflink(mp))
			adj = xfs_calc_buf_res(
					xfs_refcountbt_block_count(mp, 2),
					blksz);
		t1 += adj;
		t3 += adj;
		return XFS_DQUOT_LOGRES(mp) + max3(t1, t2, t3);
	}

	t4 = xfs_calc_refcountbt_reservation(mp, 1);
	return XFS_DQUOT_LOGRES(mp) + max(t4, max3(t1, t2, t3));
}

unsigned int
xfs_calc_write_reservation_minlogsize(
	struct xfs_mount	*mp)
{
	return xfs_calc_write_reservation(mp, true);
}

 
STATIC uint
xfs_calc_itruncate_reservation(
	struct xfs_mount	*mp,
	bool			for_minlogsize)
{
	unsigned int		t1, t2, t3, t4;
	unsigned int		blksz = XFS_FSB_TO_B(mp, 1);

	t1 = xfs_calc_inode_res(mp, 1) +
	     xfs_calc_buf_res(XFS_BM_MAXLEVELS(mp, XFS_DATA_FORK) + 1, blksz);

	t2 = xfs_calc_buf_res(9, mp->m_sb.sb_sectsize) +
	     xfs_calc_buf_res(xfs_allocfree_block_count(mp, 4), blksz);

	if (xfs_has_realtime(mp)) {
		t3 = xfs_calc_buf_res(5, mp->m_sb.sb_sectsize) +
		     xfs_calc_buf_res(xfs_rtalloc_block_count(mp, 2), blksz) +
		     xfs_calc_buf_res(xfs_allocfree_block_count(mp, 2), blksz);
	} else {
		t3 = 0;
	}

	 
	if (for_minlogsize) {
		if (xfs_has_reflink(mp))
			t2 += xfs_calc_buf_res(
					xfs_refcountbt_block_count(mp, 4),
					blksz);

		return XFS_DQUOT_LOGRES(mp) + max3(t1, t2, t3);
	}

	t4 = xfs_calc_refcountbt_reservation(mp, 2);
	return XFS_DQUOT_LOGRES(mp) + max(t4, max3(t1, t2, t3));
}

unsigned int
xfs_calc_itruncate_reservation_minlogsize(
	struct xfs_mount	*mp)
{
	return xfs_calc_itruncate_reservation(mp, true);
}

 
STATIC uint
xfs_calc_rename_reservation(
	struct xfs_mount	*mp)
{
	return XFS_DQUOT_LOGRES(mp) +
		max((xfs_calc_inode_res(mp, 5) +
		     xfs_calc_buf_res(2 * XFS_DIROP_LOG_COUNT(mp),
				      XFS_FSB_TO_B(mp, 1))),
		    (xfs_calc_buf_res(7, mp->m_sb.sb_sectsize) +
		     xfs_calc_buf_res(xfs_allocfree_block_count(mp, 3),
				      XFS_FSB_TO_B(mp, 1))));
}

 
STATIC uint
xfs_calc_iunlink_remove_reservation(
	struct xfs_mount        *mp)
{
	return xfs_calc_buf_res(1, mp->m_sb.sb_sectsize) +
	       2 * M_IGEO(mp)->inode_cluster_size;
}

 
STATIC uint
xfs_calc_link_reservation(
	struct xfs_mount	*mp)
{
	return XFS_DQUOT_LOGRES(mp) +
		xfs_calc_iunlink_remove_reservation(mp) +
		max((xfs_calc_inode_res(mp, 2) +
		     xfs_calc_buf_res(XFS_DIROP_LOG_COUNT(mp),
				      XFS_FSB_TO_B(mp, 1))),
		    (xfs_calc_buf_res(3, mp->m_sb.sb_sectsize) +
		     xfs_calc_buf_res(xfs_allocfree_block_count(mp, 1),
				      XFS_FSB_TO_B(mp, 1))));
}

 
STATIC uint
xfs_calc_iunlink_add_reservation(xfs_mount_t *mp)
{
	return xfs_calc_buf_res(1, mp->m_sb.sb_sectsize) +
			M_IGEO(mp)->inode_cluster_size;
}

 
STATIC uint
xfs_calc_remove_reservation(
	struct xfs_mount	*mp)
{
	return XFS_DQUOT_LOGRES(mp) +
		xfs_calc_iunlink_add_reservation(mp) +
		max((xfs_calc_inode_res(mp, 2) +
		     xfs_calc_buf_res(XFS_DIROP_LOG_COUNT(mp),
				      XFS_FSB_TO_B(mp, 1))),
		    (xfs_calc_buf_res(4, mp->m_sb.sb_sectsize) +
		     xfs_calc_buf_res(xfs_allocfree_block_count(mp, 2),
				      XFS_FSB_TO_B(mp, 1))));
}

 

 
STATIC uint
xfs_calc_create_resv_modify(
	struct xfs_mount	*mp)
{
	return xfs_calc_inode_res(mp, 2) +
		xfs_calc_buf_res(1, mp->m_sb.sb_sectsize) +
		(uint)XFS_FSB_TO_B(mp, 1) +
		xfs_calc_buf_res(XFS_DIROP_LOG_COUNT(mp), XFS_FSB_TO_B(mp, 1)) +
		xfs_calc_finobt_res(mp);
}

 
STATIC uint
xfs_calc_icreate_resv_alloc(
	struct xfs_mount	*mp)
{
	return xfs_calc_buf_res(2, mp->m_sb.sb_sectsize) +
		mp->m_sb.sb_sectsize +
		xfs_calc_inode_chunk_res(mp, _ALLOC) +
		xfs_calc_inobt_res(mp) +
		xfs_calc_finobt_res(mp);
}

STATIC uint
xfs_calc_icreate_reservation(xfs_mount_t *mp)
{
	return XFS_DQUOT_LOGRES(mp) +
		max(xfs_calc_icreate_resv_alloc(mp),
		    xfs_calc_create_resv_modify(mp));
}

STATIC uint
xfs_calc_create_tmpfile_reservation(
	struct xfs_mount        *mp)
{
	uint	res = XFS_DQUOT_LOGRES(mp);

	res += xfs_calc_icreate_resv_alloc(mp);
	return res + xfs_calc_iunlink_add_reservation(mp);
}

 
STATIC uint
xfs_calc_mkdir_reservation(
	struct xfs_mount	*mp)
{
	return xfs_calc_icreate_reservation(mp);
}


 
STATIC uint
xfs_calc_symlink_reservation(
	struct xfs_mount	*mp)
{
	return xfs_calc_icreate_reservation(mp) +
	       xfs_calc_buf_res(1, XFS_SYMLINK_MAXLEN);
}

 
STATIC uint
xfs_calc_ifree_reservation(
	struct xfs_mount	*mp)
{
	return XFS_DQUOT_LOGRES(mp) +
		xfs_calc_inode_res(mp, 1) +
		xfs_calc_buf_res(3, mp->m_sb.sb_sectsize) +
		xfs_calc_iunlink_remove_reservation(mp) +
		xfs_calc_inode_chunk_res(mp, _FREE) +
		xfs_calc_inobt_res(mp) +
		xfs_calc_finobt_res(mp);
}

 
STATIC uint
xfs_calc_ichange_reservation(
	struct xfs_mount	*mp)
{
	return XFS_DQUOT_LOGRES(mp) +
		xfs_calc_inode_res(mp, 1) +
		xfs_calc_buf_res(1, mp->m_sb.sb_sectsize);

}

 
STATIC uint
xfs_calc_growdata_reservation(
	struct xfs_mount	*mp)
{
	return xfs_calc_buf_res(3, mp->m_sb.sb_sectsize) +
		xfs_calc_buf_res(xfs_allocfree_block_count(mp, 1),
				 XFS_FSB_TO_B(mp, 1));
}

 
STATIC uint
xfs_calc_growrtalloc_reservation(
	struct xfs_mount	*mp)
{
	return xfs_calc_buf_res(2, mp->m_sb.sb_sectsize) +
		xfs_calc_buf_res(XFS_BM_MAXLEVELS(mp, XFS_DATA_FORK),
				 XFS_FSB_TO_B(mp, 1)) +
		xfs_calc_inode_res(mp, 1) +
		xfs_calc_buf_res(xfs_allocfree_block_count(mp, 1),
				 XFS_FSB_TO_B(mp, 1));
}

 
STATIC uint
xfs_calc_growrtzero_reservation(
	struct xfs_mount	*mp)
{
	return xfs_calc_buf_res(1, mp->m_sb.sb_blocksize);
}

 
STATIC uint
xfs_calc_growrtfree_reservation(
	struct xfs_mount	*mp)
{
	return xfs_calc_buf_res(1, mp->m_sb.sb_sectsize) +
		xfs_calc_inode_res(mp, 2) +
		xfs_calc_buf_res(1, mp->m_sb.sb_blocksize) +
		xfs_calc_buf_res(1, mp->m_rsumsize);
}

 
STATIC uint
xfs_calc_swrite_reservation(
	struct xfs_mount	*mp)
{
	return xfs_calc_inode_res(mp, 1);
}

 
STATIC uint
xfs_calc_writeid_reservation(
	struct xfs_mount	*mp)
{
	return xfs_calc_inode_res(mp, 1);
}

 
STATIC uint
xfs_calc_addafork_reservation(
	struct xfs_mount	*mp)
{
	return XFS_DQUOT_LOGRES(mp) +
		xfs_calc_inode_res(mp, 1) +
		xfs_calc_buf_res(2, mp->m_sb.sb_sectsize) +
		xfs_calc_buf_res(1, mp->m_dir_geo->blksize) +
		xfs_calc_buf_res(XFS_DAENTER_BMAP1B(mp, XFS_DATA_FORK) + 1,
				 XFS_FSB_TO_B(mp, 1)) +
		xfs_calc_buf_res(xfs_allocfree_block_count(mp, 1),
				 XFS_FSB_TO_B(mp, 1));
}

 
STATIC uint
xfs_calc_attrinval_reservation(
	struct xfs_mount	*mp)
{
	return max((xfs_calc_inode_res(mp, 1) +
		    xfs_calc_buf_res(XFS_BM_MAXLEVELS(mp, XFS_ATTR_FORK),
				     XFS_FSB_TO_B(mp, 1))),
		   (xfs_calc_buf_res(9, mp->m_sb.sb_sectsize) +
		    xfs_calc_buf_res(xfs_allocfree_block_count(mp, 4),
				     XFS_FSB_TO_B(mp, 1))));
}

 
STATIC uint
xfs_calc_attrsetm_reservation(
	struct xfs_mount	*mp)
{
	return XFS_DQUOT_LOGRES(mp) +
		xfs_calc_inode_res(mp, 1) +
		xfs_calc_buf_res(1, mp->m_sb.sb_sectsize) +
		xfs_calc_buf_res(XFS_DA_NODE_MAXDEPTH, XFS_FSB_TO_B(mp, 1));
}

 
STATIC uint
xfs_calc_attrsetrt_reservation(
	struct xfs_mount	*mp)
{
	return xfs_calc_buf_res(1, mp->m_sb.sb_sectsize) +
		xfs_calc_buf_res(XFS_BM_MAXLEVELS(mp, XFS_ATTR_FORK),
				 XFS_FSB_TO_B(mp, 1));
}

 
STATIC uint
xfs_calc_attrrm_reservation(
	struct xfs_mount	*mp)
{
	return XFS_DQUOT_LOGRES(mp) +
		max((xfs_calc_inode_res(mp, 1) +
		     xfs_calc_buf_res(XFS_DA_NODE_MAXDEPTH,
				      XFS_FSB_TO_B(mp, 1)) +
		     (uint)XFS_FSB_TO_B(mp,
					XFS_BM_MAXLEVELS(mp, XFS_ATTR_FORK)) +
		     xfs_calc_buf_res(XFS_BM_MAXLEVELS(mp, XFS_DATA_FORK), 0)),
		    (xfs_calc_buf_res(5, mp->m_sb.sb_sectsize) +
		     xfs_calc_buf_res(xfs_allocfree_block_count(mp, 2),
				      XFS_FSB_TO_B(mp, 1))));
}

 
STATIC uint
xfs_calc_clear_agi_bucket_reservation(
	struct xfs_mount	*mp)
{
	return xfs_calc_buf_res(1, mp->m_sb.sb_sectsize);
}

 
STATIC uint
xfs_calc_qm_setqlim_reservation(void)
{
	return xfs_calc_buf_res(1, sizeof(struct xfs_disk_dquot));
}

 
STATIC uint
xfs_calc_qm_dqalloc_reservation(
	struct xfs_mount	*mp,
	bool			for_minlogsize)
{
	return xfs_calc_write_reservation(mp, for_minlogsize) +
		xfs_calc_buf_res(1,
			XFS_FSB_TO_B(mp, XFS_DQUOT_CLUSTER_SIZE_FSB) - 1);
}

unsigned int
xfs_calc_qm_dqalloc_reservation_minlogsize(
	struct xfs_mount	*mp)
{
	return xfs_calc_qm_dqalloc_reservation(mp, true);
}

 
STATIC uint
xfs_calc_sb_reservation(
	struct xfs_mount	*mp)
{
	return xfs_calc_buf_res(1, mp->m_sb.sb_sectsize);
}

void
xfs_trans_resv_calc(
	struct xfs_mount	*mp,
	struct xfs_trans_resv	*resp)
{
	int			logcount_adj = 0;

	 
	resp->tr_write.tr_logres = xfs_calc_write_reservation(mp, false);
	resp->tr_write.tr_logcount = XFS_WRITE_LOG_COUNT;
	resp->tr_write.tr_logflags |= XFS_TRANS_PERM_LOG_RES;

	resp->tr_itruncate.tr_logres = xfs_calc_itruncate_reservation(mp, false);
	resp->tr_itruncate.tr_logcount = XFS_ITRUNCATE_LOG_COUNT;
	resp->tr_itruncate.tr_logflags |= XFS_TRANS_PERM_LOG_RES;

	resp->tr_rename.tr_logres = xfs_calc_rename_reservation(mp);
	resp->tr_rename.tr_logcount = XFS_RENAME_LOG_COUNT;
	resp->tr_rename.tr_logflags |= XFS_TRANS_PERM_LOG_RES;

	resp->tr_link.tr_logres = xfs_calc_link_reservation(mp);
	resp->tr_link.tr_logcount = XFS_LINK_LOG_COUNT;
	resp->tr_link.tr_logflags |= XFS_TRANS_PERM_LOG_RES;

	resp->tr_remove.tr_logres = xfs_calc_remove_reservation(mp);
	resp->tr_remove.tr_logcount = XFS_REMOVE_LOG_COUNT;
	resp->tr_remove.tr_logflags |= XFS_TRANS_PERM_LOG_RES;

	resp->tr_symlink.tr_logres = xfs_calc_symlink_reservation(mp);
	resp->tr_symlink.tr_logcount = XFS_SYMLINK_LOG_COUNT;
	resp->tr_symlink.tr_logflags |= XFS_TRANS_PERM_LOG_RES;

	resp->tr_create.tr_logres = xfs_calc_icreate_reservation(mp);
	resp->tr_create.tr_logcount = XFS_CREATE_LOG_COUNT;
	resp->tr_create.tr_logflags |= XFS_TRANS_PERM_LOG_RES;

	resp->tr_create_tmpfile.tr_logres =
			xfs_calc_create_tmpfile_reservation(mp);
	resp->tr_create_tmpfile.tr_logcount = XFS_CREATE_TMPFILE_LOG_COUNT;
	resp->tr_create_tmpfile.tr_logflags |= XFS_TRANS_PERM_LOG_RES;

	resp->tr_mkdir.tr_logres = xfs_calc_mkdir_reservation(mp);
	resp->tr_mkdir.tr_logcount = XFS_MKDIR_LOG_COUNT;
	resp->tr_mkdir.tr_logflags |= XFS_TRANS_PERM_LOG_RES;

	resp->tr_ifree.tr_logres = xfs_calc_ifree_reservation(mp);
	resp->tr_ifree.tr_logcount = XFS_INACTIVE_LOG_COUNT;
	resp->tr_ifree.tr_logflags |= XFS_TRANS_PERM_LOG_RES;

	resp->tr_addafork.tr_logres = xfs_calc_addafork_reservation(mp);
	resp->tr_addafork.tr_logcount = XFS_ADDAFORK_LOG_COUNT;
	resp->tr_addafork.tr_logflags |= XFS_TRANS_PERM_LOG_RES;

	resp->tr_attrinval.tr_logres = xfs_calc_attrinval_reservation(mp);
	resp->tr_attrinval.tr_logcount = XFS_ATTRINVAL_LOG_COUNT;
	resp->tr_attrinval.tr_logflags |= XFS_TRANS_PERM_LOG_RES;

	resp->tr_attrsetm.tr_logres = xfs_calc_attrsetm_reservation(mp);
	resp->tr_attrsetm.tr_logcount = XFS_ATTRSET_LOG_COUNT;
	resp->tr_attrsetm.tr_logflags |= XFS_TRANS_PERM_LOG_RES;

	resp->tr_attrrm.tr_logres = xfs_calc_attrrm_reservation(mp);
	resp->tr_attrrm.tr_logcount = XFS_ATTRRM_LOG_COUNT;
	resp->tr_attrrm.tr_logflags |= XFS_TRANS_PERM_LOG_RES;

	resp->tr_growrtalloc.tr_logres = xfs_calc_growrtalloc_reservation(mp);
	resp->tr_growrtalloc.tr_logcount = XFS_DEFAULT_PERM_LOG_COUNT;
	resp->tr_growrtalloc.tr_logflags |= XFS_TRANS_PERM_LOG_RES;

	resp->tr_qm_dqalloc.tr_logres = xfs_calc_qm_dqalloc_reservation(mp,
			false);
	resp->tr_qm_dqalloc.tr_logcount = XFS_WRITE_LOG_COUNT;
	resp->tr_qm_dqalloc.tr_logflags |= XFS_TRANS_PERM_LOG_RES;

	 
	resp->tr_qm_setqlim.tr_logres = xfs_calc_qm_setqlim_reservation();
	resp->tr_qm_setqlim.tr_logcount = XFS_DEFAULT_LOG_COUNT;

	resp->tr_sb.tr_logres = xfs_calc_sb_reservation(mp);
	resp->tr_sb.tr_logcount = XFS_DEFAULT_LOG_COUNT;

	 
	resp->tr_growdata.tr_logres = xfs_calc_growdata_reservation(mp);
	resp->tr_growdata.tr_logcount = XFS_DEFAULT_PERM_LOG_COUNT;
	resp->tr_growdata.tr_logflags |= XFS_TRANS_PERM_LOG_RES;

	 
	resp->tr_ichange.tr_logres = xfs_calc_ichange_reservation(mp);
	resp->tr_fsyncts.tr_logres = xfs_calc_swrite_reservation(mp);
	resp->tr_writeid.tr_logres = xfs_calc_writeid_reservation(mp);
	resp->tr_attrsetrt.tr_logres = xfs_calc_attrsetrt_reservation(mp);
	resp->tr_clearagi.tr_logres = xfs_calc_clear_agi_bucket_reservation(mp);
	resp->tr_growrtzero.tr_logres = xfs_calc_growrtzero_reservation(mp);
	resp->tr_growrtfree.tr_logres = xfs_calc_growrtfree_reservation(mp);

	 
	if (xfs_has_reflink(mp) || xfs_has_rmapbt(mp))
		logcount_adj++;
	if (xfs_has_reflink(mp))
		logcount_adj++;
	if (xfs_has_rmapbt(mp))
		logcount_adj++;

	resp->tr_itruncate.tr_logcount += logcount_adj;
	resp->tr_write.tr_logcount += logcount_adj;
	resp->tr_qm_dqalloc.tr_logcount += logcount_adj;
}
