
 


#include "xfs.h"
#include "xfs_fs.h"
#include "xfs_shared.h"
#include "xfs_format.h"
#include "xfs_log_format.h"
#include "xfs_trans_resv.h"
#include "xfs_sb.h"
#include "xfs_mount.h"
#include "xfs_inode.h"
#include "xfs_trans.h"
#include "xfs_quota.h"
#include "xfs_qm.h"
#include "xfs_icache.h"

int
xfs_qm_scall_quotaoff(
	xfs_mount_t		*mp,
	uint			flags)
{
	 
	if ((mp->m_qflags & flags) == 0)
		return -EEXIST;

	 
	if (flags & XFS_ALL_QUOTA_ACCT)
		xfs_info(mp, "disabling of quota accounting not supported.");

	mutex_lock(&mp->m_quotainfo->qi_quotaofflock);
	mp->m_qflags &= ~(flags & XFS_ALL_QUOTA_ENFD);
	spin_lock(&mp->m_sb_lock);
	mp->m_sb.sb_qflags = mp->m_qflags;
	spin_unlock(&mp->m_sb_lock);
	mutex_unlock(&mp->m_quotainfo->qi_quotaofflock);

	 
	return xfs_sync_sb(mp, false);
}

STATIC int
xfs_qm_scall_trunc_qfile(
	struct xfs_mount	*mp,
	xfs_ino_t		ino)
{
	struct xfs_inode	*ip;
	struct xfs_trans	*tp;
	int			error;

	if (ino == NULLFSINO)
		return 0;

	error = xfs_iget(mp, NULL, ino, 0, 0, &ip);
	if (error)
		return error;

	xfs_ilock(ip, XFS_IOLOCK_EXCL);

	error = xfs_trans_alloc(mp, &M_RES(mp)->tr_itruncate, 0, 0, 0, &tp);
	if (error) {
		xfs_iunlock(ip, XFS_IOLOCK_EXCL);
		goto out_put;
	}

	xfs_ilock(ip, XFS_ILOCK_EXCL);
	xfs_trans_ijoin(tp, ip, 0);

	ip->i_disk_size = 0;
	xfs_trans_log_inode(tp, ip, XFS_ILOG_CORE);

	error = xfs_itruncate_extents(&tp, ip, XFS_DATA_FORK, 0);
	if (error) {
		xfs_trans_cancel(tp);
		goto out_unlock;
	}

	ASSERT(ip->i_df.if_nextents == 0);

	xfs_trans_ichgtime(tp, ip, XFS_ICHGTIME_MOD | XFS_ICHGTIME_CHG);
	error = xfs_trans_commit(tp);

out_unlock:
	xfs_iunlock(ip, XFS_ILOCK_EXCL | XFS_IOLOCK_EXCL);
out_put:
	xfs_irele(ip);
	return error;
}

int
xfs_qm_scall_trunc_qfiles(
	xfs_mount_t	*mp,
	uint		flags)
{
	int		error = -EINVAL;

	if (!xfs_has_quota(mp) || flags == 0 ||
	    (flags & ~XFS_QMOPT_QUOTALL)) {
		xfs_debug(mp, "%s: flags=%x m_qflags=%x",
			__func__, flags, mp->m_qflags);
		return -EINVAL;
	}

	if (flags & XFS_QMOPT_UQUOTA) {
		error = xfs_qm_scall_trunc_qfile(mp, mp->m_sb.sb_uquotino);
		if (error)
			return error;
	}
	if (flags & XFS_QMOPT_GQUOTA) {
		error = xfs_qm_scall_trunc_qfile(mp, mp->m_sb.sb_gquotino);
		if (error)
			return error;
	}
	if (flags & XFS_QMOPT_PQUOTA)
		error = xfs_qm_scall_trunc_qfile(mp, mp->m_sb.sb_pquotino);

	return error;
}

 
int
xfs_qm_scall_quotaon(
	xfs_mount_t	*mp,
	uint		flags)
{
	int		error;
	uint		qf;

	 
	flags &= XFS_ALL_QUOTA_ENFD;

	if (flags == 0) {
		xfs_debug(mp, "%s: zero flags, m_qflags=%x",
			__func__, mp->m_qflags);
		return -EINVAL;
	}

	 
	if (((mp->m_sb.sb_qflags & XFS_UQUOTA_ACCT) == 0 &&
	     (flags & XFS_UQUOTA_ENFD)) ||
	    ((mp->m_sb.sb_qflags & XFS_GQUOTA_ACCT) == 0 &&
	     (flags & XFS_GQUOTA_ENFD)) ||
	    ((mp->m_sb.sb_qflags & XFS_PQUOTA_ACCT) == 0 &&
	     (flags & XFS_PQUOTA_ENFD))) {
		xfs_debug(mp,
			"%s: Can't enforce without acct, flags=%x sbflags=%x",
			__func__, flags, mp->m_sb.sb_qflags);
		return -EINVAL;
	}
	 
	if ((mp->m_qflags & flags) == flags)
		return -EEXIST;

	 
	spin_lock(&mp->m_sb_lock);
	qf = mp->m_sb.sb_qflags;
	mp->m_sb.sb_qflags = qf | flags;
	spin_unlock(&mp->m_sb_lock);

	 
	if ((qf & flags) == flags)
		return -EEXIST;

	error = xfs_sync_sb(mp, false);
	if (error)
		return error;
	 
	if  (((mp->m_sb.sb_qflags & XFS_UQUOTA_ACCT) !=
	     (mp->m_qflags & XFS_UQUOTA_ACCT)) ||
	     ((mp->m_sb.sb_qflags & XFS_PQUOTA_ACCT) !=
	     (mp->m_qflags & XFS_PQUOTA_ACCT)) ||
	     ((mp->m_sb.sb_qflags & XFS_GQUOTA_ACCT) !=
	     (mp->m_qflags & XFS_GQUOTA_ACCT)))
		return 0;

	if (!XFS_IS_QUOTA_ON(mp))
		return -ESRCH;

	 
	mutex_lock(&mp->m_quotainfo->qi_quotaofflock);
	mp->m_qflags |= (flags & XFS_ALL_QUOTA_ENFD);
	mutex_unlock(&mp->m_quotainfo->qi_quotaofflock);

	return 0;
}

#define XFS_QC_MASK (QC_LIMIT_MASK | QC_TIMER_MASK)

 
static inline bool
xfs_setqlim_limits(
	struct xfs_mount	*mp,
	struct xfs_dquot_res	*res,
	struct xfs_quota_limits	*qlim,
	xfs_qcnt_t		hard,
	xfs_qcnt_t		soft,
	const char		*tag)
{
	 
	if (hard != 0 && hard < soft) {
		xfs_debug(mp, "%shard %lld < %ssoft %lld", tag, hard, tag,
				soft);
		return false;
	}

	res->hardlimit = hard;
	res->softlimit = soft;
	if (qlim) {
		qlim->hard = hard;
		qlim->soft = soft;
	}

	return true;
}

static inline void
xfs_setqlim_timer(
	struct xfs_mount	*mp,
	struct xfs_dquot_res	*res,
	struct xfs_quota_limits	*qlim,
	s64			timer)
{
	if (qlim) {
		 
		res->timer = xfs_dquot_set_grace_period(timer);
		qlim->time = res->timer;
	} else {
		 
		res->timer = xfs_dquot_set_timeout(mp, timer);
	}
}

 
int
xfs_qm_scall_setqlim(
	struct xfs_mount	*mp,
	xfs_dqid_t		id,
	xfs_dqtype_t		type,
	struct qc_dqblk		*newlim)
{
	struct xfs_quotainfo	*q = mp->m_quotainfo;
	struct xfs_dquot	*dqp;
	struct xfs_trans	*tp;
	struct xfs_def_quota	*defq;
	struct xfs_dquot_res	*res;
	struct xfs_quota_limits	*qlim;
	int			error;
	xfs_qcnt_t		hard, soft;

	if (newlim->d_fieldmask & ~XFS_QC_MASK)
		return -EINVAL;
	if ((newlim->d_fieldmask & XFS_QC_MASK) == 0)
		return 0;

	 
	error = xfs_qm_dqget(mp, id, type, true, &dqp);
	if (error) {
		ASSERT(error != -ENOENT);
		return error;
	}

	defq = xfs_get_defquota(q, xfs_dquot_type(dqp));
	xfs_dqunlock(dqp);

	error = xfs_trans_alloc(mp, &M_RES(mp)->tr_qm_setqlim, 0, 0, 0, &tp);
	if (error)
		goto out_rele;

	xfs_dqlock(dqp);
	xfs_trans_dqjoin(tp, dqp);

	 

	 
	hard = (newlim->d_fieldmask & QC_SPC_HARD) ?
		(xfs_qcnt_t) XFS_B_TO_FSB(mp, newlim->d_spc_hardlimit) :
			dqp->q_blk.hardlimit;
	soft = (newlim->d_fieldmask & QC_SPC_SOFT) ?
		(xfs_qcnt_t) XFS_B_TO_FSB(mp, newlim->d_spc_softlimit) :
			dqp->q_blk.softlimit;
	res = &dqp->q_blk;
	qlim = id == 0 ? &defq->blk : NULL;

	if (xfs_setqlim_limits(mp, res, qlim, hard, soft, "blk"))
		xfs_dquot_set_prealloc_limits(dqp);
	if (newlim->d_fieldmask & QC_SPC_TIMER)
		xfs_setqlim_timer(mp, res, qlim, newlim->d_spc_timer);

	 
	hard = (newlim->d_fieldmask & QC_RT_SPC_HARD) ?
		(xfs_qcnt_t) XFS_B_TO_FSB(mp, newlim->d_rt_spc_hardlimit) :
			dqp->q_rtb.hardlimit;
	soft = (newlim->d_fieldmask & QC_RT_SPC_SOFT) ?
		(xfs_qcnt_t) XFS_B_TO_FSB(mp, newlim->d_rt_spc_softlimit) :
			dqp->q_rtb.softlimit;
	res = &dqp->q_rtb;
	qlim = id == 0 ? &defq->rtb : NULL;

	xfs_setqlim_limits(mp, res, qlim, hard, soft, "rtb");
	if (newlim->d_fieldmask & QC_RT_SPC_TIMER)
		xfs_setqlim_timer(mp, res, qlim, newlim->d_rt_spc_timer);

	 
	hard = (newlim->d_fieldmask & QC_INO_HARD) ?
		(xfs_qcnt_t) newlim->d_ino_hardlimit :
			dqp->q_ino.hardlimit;
	soft = (newlim->d_fieldmask & QC_INO_SOFT) ?
		(xfs_qcnt_t) newlim->d_ino_softlimit :
			dqp->q_ino.softlimit;
	res = &dqp->q_ino;
	qlim = id == 0 ? &defq->ino : NULL;

	xfs_setqlim_limits(mp, res, qlim, hard, soft, "ino");
	if (newlim->d_fieldmask & QC_INO_TIMER)
		xfs_setqlim_timer(mp, res, qlim, newlim->d_ino_timer);

	if (id != 0) {
		 
		xfs_qm_adjust_dqtimers(dqp);
	}
	dqp->q_flags |= XFS_DQFLAG_DIRTY;
	xfs_trans_log_dquot(tp, dqp);

	error = xfs_trans_commit(tp);

out_rele:
	xfs_qm_dqrele(dqp);
	return error;
}

 
static void
xfs_qm_scall_getquota_fill_qc(
	struct xfs_mount	*mp,
	xfs_dqtype_t		type,
	const struct xfs_dquot	*dqp,
	struct qc_dqblk		*dst)
{
	memset(dst, 0, sizeof(*dst));
	dst->d_spc_hardlimit = XFS_FSB_TO_B(mp, dqp->q_blk.hardlimit);
	dst->d_spc_softlimit = XFS_FSB_TO_B(mp, dqp->q_blk.softlimit);
	dst->d_ino_hardlimit = dqp->q_ino.hardlimit;
	dst->d_ino_softlimit = dqp->q_ino.softlimit;
	dst->d_space = XFS_FSB_TO_B(mp, dqp->q_blk.reserved);
	dst->d_ino_count = dqp->q_ino.reserved;
	dst->d_spc_timer = dqp->q_blk.timer;
	dst->d_ino_timer = dqp->q_ino.timer;
	dst->d_ino_warns = 0;
	dst->d_spc_warns = 0;
	dst->d_rt_spc_hardlimit = XFS_FSB_TO_B(mp, dqp->q_rtb.hardlimit);
	dst->d_rt_spc_softlimit = XFS_FSB_TO_B(mp, dqp->q_rtb.softlimit);
	dst->d_rt_space = XFS_FSB_TO_B(mp, dqp->q_rtb.reserved);
	dst->d_rt_spc_timer = dqp->q_rtb.timer;
	dst->d_rt_spc_warns = 0;

	 
	if (!xfs_dquot_is_enforced(dqp)) {
		dst->d_spc_timer = 0;
		dst->d_ino_timer = 0;
		dst->d_rt_spc_timer = 0;
	}

#ifdef DEBUG
	if (xfs_dquot_is_enforced(dqp) && dqp->q_id != 0) {
		if ((dst->d_space > dst->d_spc_softlimit) &&
		    (dst->d_spc_softlimit > 0)) {
			ASSERT(dst->d_spc_timer != 0);
		}
		if ((dst->d_ino_count > dqp->q_ino.softlimit) &&
		    (dqp->q_ino.softlimit > 0)) {
			ASSERT(dst->d_ino_timer != 0);
		}
	}
#endif
}

 
int
xfs_qm_scall_getquota(
	struct xfs_mount	*mp,
	xfs_dqid_t		id,
	xfs_dqtype_t		type,
	struct qc_dqblk		*dst)
{
	struct xfs_dquot	*dqp;
	int			error;

	 
	if (id == 0)
		xfs_inodegc_push(mp);

	 
	error = xfs_qm_dqget(mp, id, type, false, &dqp);
	if (error)
		return error;

	 
	if (XFS_IS_DQUOT_UNINITIALIZED(dqp)) {
		error = -ENOENT;
		goto out_put;
	}

	xfs_qm_scall_getquota_fill_qc(mp, type, dqp, dst);

out_put:
	xfs_qm_dqput(dqp);
	return error;
}

 
int
xfs_qm_scall_getquota_next(
	struct xfs_mount	*mp,
	xfs_dqid_t		*id,
	xfs_dqtype_t		type,
	struct qc_dqblk		*dst)
{
	struct xfs_dquot	*dqp;
	int			error;

	 
	if (*id == 0)
		xfs_inodegc_push(mp);

	error = xfs_qm_dqget_next(mp, *id, type, &dqp);
	if (error)
		return error;

	 
	*id = dqp->q_id;

	xfs_qm_scall_getquota_fill_qc(mp, type, dqp, dst);

	xfs_qm_dqput(dqp);
	return error;
}
