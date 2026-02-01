
 
#include "xfs.h"
#include "xfs_fs.h"
#include "xfs_shared.h"
#include "xfs_format.h"
#include "xfs_log_format.h"
#include "xfs_trans_resv.h"
#include "xfs_mount.h"
#include "xfs_da_format.h"
#include "xfs_trans_space.h"
#include "xfs_da_btree.h"
#include "xfs_bmap_btree.h"
#include "xfs_trace.h"

 
STATIC int
xfs_log_calc_max_attrsetm_res(
	struct xfs_mount	*mp)
{
	int			size;
	int			nblks;

	size = xfs_attr_leaf_entsize_local_max(mp->m_attr_geo->blksize) -
	       MAXNAMELEN - 1;
	nblks = XFS_DAENTER_SPACE_RES(mp, XFS_ATTR_FORK);
	nblks += XFS_B_TO_FSB(mp, size);
	nblks += XFS_NEXTENTADD_SPACE_RES(mp, size, XFS_ATTR_FORK);

	return  M_RES(mp)->tr_attrsetm.tr_logres +
		M_RES(mp)->tr_attrsetrt.tr_logres * nblks;
}

 
static void
xfs_log_calc_trans_resv_for_minlogblocks(
	struct xfs_mount	*mp,
	struct xfs_trans_resv	*resv)
{
	unsigned int		rmap_maxlevels = mp->m_rmap_maxlevels;

	 
	if (xfs_has_rmapbt(mp) && xfs_has_reflink(mp))
		mp->m_rmap_maxlevels = XFS_OLD_REFLINK_RMAP_MAXLEVELS;

	xfs_trans_resv_calc(mp, resv);

	if (xfs_has_reflink(mp)) {
		 
		resv->tr_write.tr_logcount = XFS_WRITE_LOG_COUNT_REFLINK;
		resv->tr_itruncate.tr_logcount =
				XFS_ITRUNCATE_LOG_COUNT_REFLINK;
		resv->tr_qm_dqalloc.tr_logcount = XFS_WRITE_LOG_COUNT_REFLINK;
	} else if (xfs_has_rmapbt(mp)) {
		 
		resv->tr_write.tr_logcount = XFS_WRITE_LOG_COUNT;
		resv->tr_itruncate.tr_logcount = XFS_ITRUNCATE_LOG_COUNT;
		resv->tr_qm_dqalloc.tr_logcount = XFS_WRITE_LOG_COUNT;
	}

	 
	resv->tr_write.tr_logres =
			xfs_calc_write_reservation_minlogsize(mp);
	resv->tr_itruncate.tr_logres =
			xfs_calc_itruncate_reservation_minlogsize(mp);
	resv->tr_qm_dqalloc.tr_logres =
			xfs_calc_qm_dqalloc_reservation_minlogsize(mp);

	 
	mp->m_rmap_maxlevels = rmap_maxlevels;
}

 
void
xfs_log_get_max_trans_res(
	struct xfs_mount	*mp,
	struct xfs_trans_res	*max_resp)
{
	struct xfs_trans_resv	resv = {};
	struct xfs_trans_res	*resp;
	struct xfs_trans_res	*end_resp;
	unsigned int		i;
	int			log_space = 0;
	int			attr_space;

	attr_space = xfs_log_calc_max_attrsetm_res(mp);

	xfs_log_calc_trans_resv_for_minlogblocks(mp, &resv);

	resp = (struct xfs_trans_res *)&resv;
	end_resp = (struct xfs_trans_res *)(&resv + 1);
	for (i = 0; resp < end_resp; i++, resp++) {
		int		tmp = resp->tr_logcount > 1 ?
				      resp->tr_logres * resp->tr_logcount :
				      resp->tr_logres;

		trace_xfs_trans_resv_calc_minlogsize(mp, i, resp);
		if (log_space < tmp) {
			log_space = tmp;
			*max_resp = *resp;		 
		}
	}

	if (attr_space > log_space) {
		*max_resp = resv.tr_attrsetm;	 
		max_resp->tr_logres = attr_space;
	}
	trace_xfs_log_get_max_trans_res(mp, max_resp);
}

 
int
xfs_log_calc_minimum_size(
	struct xfs_mount	*mp)
{
	struct xfs_trans_res	tres = {0};
	int			max_logres;
	int			min_logblks = 0;
	int			lsunit = 0;

	xfs_log_get_max_trans_res(mp, &tres);

	max_logres = xfs_log_calc_unit_res(mp, tres.tr_logres);
	if (tres.tr_logcount > 1)
		max_logres *= tres.tr_logcount;

	if (xfs_has_logv2(mp) && mp->m_sb.sb_logsunit > 1)
		lsunit = BTOBB(mp->m_sb.sb_logsunit);

	 
	if (lsunit) {
		min_logblks = roundup_64(BTOBB(max_logres), lsunit) +
			      2 * lsunit;
	} else
		min_logblks = BTOBB(max_logres) + 2 * BBSIZE;
	min_logblks *= XFS_MIN_LOG_FACTOR;

	return XFS_BB_TO_FSB(mp, min_logblks);
}
