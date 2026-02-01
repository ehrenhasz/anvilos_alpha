
 
#include "xfs.h"
#include "xfs_fs.h"
#include "xfs_shared.h"
#include "xfs_format.h"
#include "xfs_log_format.h"
#include "xfs_trans_resv.h"
#include "xfs_mount.h"
#include "xfs_inode.h"
#include "xfs_quota.h"
#include "xfs_trans.h"
#include "xfs_buf_item.h"
#include "xfs_trans_priv.h"
#include "xfs_qm.h"
#include "xfs_log.h"
#include "xfs_log_priv.h"
#include "xfs_log_recover.h"

STATIC void
xlog_recover_dquot_ra_pass2(
	struct xlog			*log,
	struct xlog_recover_item	*item)
{
	struct xfs_mount	*mp = log->l_mp;
	struct xfs_disk_dquot	*recddq;
	struct xfs_dq_logformat	*dq_f;
	uint			type;

	if (mp->m_qflags == 0)
		return;

	recddq = item->ri_buf[1].i_addr;
	if (recddq == NULL)
		return;
	if (item->ri_buf[1].i_len < sizeof(struct xfs_disk_dquot))
		return;

	type = recddq->d_type & XFS_DQTYPE_REC_MASK;
	ASSERT(type);
	if (log->l_quotaoffs_flag & type)
		return;

	dq_f = item->ri_buf[0].i_addr;
	ASSERT(dq_f);
	ASSERT(dq_f->qlf_len == 1);

	xlog_buf_readahead(log, dq_f->qlf_blkno,
			XFS_FSB_TO_BB(mp, dq_f->qlf_len),
			&xfs_dquot_buf_ra_ops);
}

 
STATIC int
xlog_recover_dquot_commit_pass2(
	struct xlog			*log,
	struct list_head		*buffer_list,
	struct xlog_recover_item	*item,
	xfs_lsn_t			current_lsn)
{
	struct xfs_mount		*mp = log->l_mp;
	struct xfs_buf			*bp;
	struct xfs_disk_dquot		*ddq, *recddq;
	struct xfs_dq_logformat		*dq_f;
	xfs_failaddr_t			fa;
	int				error;
	uint				type;

	 
	if (mp->m_qflags == 0)
		return 0;

	recddq = item->ri_buf[1].i_addr;
	if (recddq == NULL) {
		xfs_alert(log->l_mp, "NULL dquot in %s.", __func__);
		return -EFSCORRUPTED;
	}
	if (item->ri_buf[1].i_len < sizeof(struct xfs_disk_dquot)) {
		xfs_alert(log->l_mp, "dquot too small (%d) in %s.",
			item->ri_buf[1].i_len, __func__);
		return -EFSCORRUPTED;
	}

	 
	type = recddq->d_type & XFS_DQTYPE_REC_MASK;
	ASSERT(type);
	if (log->l_quotaoffs_flag & type)
		return 0;

	 
	dq_f = item->ri_buf[0].i_addr;
	ASSERT(dq_f);
	fa = xfs_dquot_verify(mp, recddq, dq_f->qlf_id);
	if (fa) {
		xfs_alert(mp, "corrupt dquot ID 0x%x in log at %pS",
				dq_f->qlf_id, fa);
		return -EFSCORRUPTED;
	}
	ASSERT(dq_f->qlf_len == 1);

	 
	error = xfs_trans_read_buf(mp, NULL, mp->m_ddev_targp, dq_f->qlf_blkno,
				   XFS_FSB_TO_BB(mp, dq_f->qlf_len), 0, &bp,
				   &xfs_dquot_buf_ops);
	if (error)
		return error;

	ASSERT(bp);
	ddq = xfs_buf_offset(bp, dq_f->qlf_boffset);

	 
	if (xfs_has_crc(mp)) {
		struct xfs_dqblk *dqb = (struct xfs_dqblk *)ddq;
		xfs_lsn_t	lsn = be64_to_cpu(dqb->dd_lsn);

		if (lsn && lsn != -1 && XFS_LSN_CMP(lsn, current_lsn) >= 0) {
			goto out_release;
		}
	}

	memcpy(ddq, recddq, item->ri_buf[1].i_len);
	if (xfs_has_crc(mp)) {
		xfs_update_cksum((char *)ddq, sizeof(struct xfs_dqblk),
				 XFS_DQUOT_CRC_OFF);
	}

	ASSERT(dq_f->qlf_size == 2);
	ASSERT(bp->b_mount == mp);
	bp->b_flags |= _XBF_LOGRECOVERY;
	xfs_buf_delwri_queue(bp, buffer_list);

out_release:
	xfs_buf_relse(bp);
	return 0;
}

const struct xlog_recover_item_ops xlog_dquot_item_ops = {
	.item_type		= XFS_LI_DQUOT,
	.ra_pass2		= xlog_recover_dquot_ra_pass2,
	.commit_pass2		= xlog_recover_dquot_commit_pass2,
};

 
STATIC int
xlog_recover_quotaoff_commit_pass1(
	struct xlog			*log,
	struct xlog_recover_item	*item)
{
	struct xfs_qoff_logformat	*qoff_f = item->ri_buf[0].i_addr;
	ASSERT(qoff_f);

	 
	if (qoff_f->qf_flags & XFS_UQUOTA_ACCT)
		log->l_quotaoffs_flag |= XFS_DQTYPE_USER;
	if (qoff_f->qf_flags & XFS_PQUOTA_ACCT)
		log->l_quotaoffs_flag |= XFS_DQTYPE_PROJ;
	if (qoff_f->qf_flags & XFS_GQUOTA_ACCT)
		log->l_quotaoffs_flag |= XFS_DQTYPE_GROUP;

	return 0;
}

const struct xlog_recover_item_ops xlog_quotaoff_item_ops = {
	.item_type		= XFS_LI_QUOTAOFF,
	.commit_pass1		= xlog_recover_quotaoff_commit_pass1,
	 
};
