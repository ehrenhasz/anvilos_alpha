
 
#include "xfs.h"
#include "xfs_fs.h"
#include "xfs_shared.h"
#include "xfs_format.h"
#include "xfs_log_format.h"
#include "xfs_trans_resv.h"
#include "xfs_bit.h"
#include "xfs_sb.h"
#include "xfs_mount.h"
#include "xfs_inode.h"
#include "xfs_iwalk.h"
#include "xfs_quota.h"
#include "xfs_bmap.h"
#include "xfs_bmap_util.h"
#include "xfs_trans.h"
#include "xfs_trans_space.h"
#include "xfs_qm.h"
#include "xfs_trace.h"
#include "xfs_icache.h"
#include "xfs_error.h"
#include "xfs_ag.h"
#include "xfs_ialloc.h"
#include "xfs_log_priv.h"

 
STATIC int	xfs_qm_init_quotainos(struct xfs_mount *mp);
STATIC int	xfs_qm_init_quotainfo(struct xfs_mount *mp);

STATIC void	xfs_qm_destroy_quotainos(struct xfs_quotainfo *qi);
STATIC void	xfs_qm_dqfree_one(struct xfs_dquot *dqp);
 
#define XFS_DQ_LOOKUP_BATCH	32

STATIC int
xfs_qm_dquot_walk(
	struct xfs_mount	*mp,
	xfs_dqtype_t		type,
	int			(*execute)(struct xfs_dquot *dqp, void *data),
	void			*data)
{
	struct xfs_quotainfo	*qi = mp->m_quotainfo;
	struct radix_tree_root	*tree = xfs_dquot_tree(qi, type);
	uint32_t		next_index;
	int			last_error = 0;
	int			skipped;
	int			nr_found;

restart:
	skipped = 0;
	next_index = 0;
	nr_found = 0;

	while (1) {
		struct xfs_dquot *batch[XFS_DQ_LOOKUP_BATCH];
		int		error;
		int		i;

		mutex_lock(&qi->qi_tree_lock);
		nr_found = radix_tree_gang_lookup(tree, (void **)batch,
					next_index, XFS_DQ_LOOKUP_BATCH);
		if (!nr_found) {
			mutex_unlock(&qi->qi_tree_lock);
			break;
		}

		for (i = 0; i < nr_found; i++) {
			struct xfs_dquot *dqp = batch[i];

			next_index = dqp->q_id + 1;

			error = execute(batch[i], data);
			if (error == -EAGAIN) {
				skipped++;
				continue;
			}
			if (error && last_error != -EFSCORRUPTED)
				last_error = error;
		}

		mutex_unlock(&qi->qi_tree_lock);

		 
		if (last_error == -EFSCORRUPTED) {
			skipped = 0;
			break;
		}
		 
		if (!next_index)
			break;
	}

	if (skipped) {
		delay(1);
		goto restart;
	}

	return last_error;
}


 
STATIC int
xfs_qm_dqpurge(
	struct xfs_dquot	*dqp,
	void			*data)
{
	struct xfs_quotainfo	*qi = dqp->q_mount->m_quotainfo;
	int			error = -EAGAIN;

	xfs_dqlock(dqp);
	if ((dqp->q_flags & XFS_DQFLAG_FREEING) || dqp->q_nrefs != 0)
		goto out_unlock;

	dqp->q_flags |= XFS_DQFLAG_FREEING;

	xfs_dqflock(dqp);

	 
	if (XFS_DQ_IS_DIRTY(dqp)) {
		struct xfs_buf	*bp = NULL;

		 
		error = xfs_qm_dqflush(dqp, &bp);
		if (!error) {
			error = xfs_bwrite(bp);
			xfs_buf_relse(bp);
		} else if (error == -EAGAIN) {
			dqp->q_flags &= ~XFS_DQFLAG_FREEING;
			goto out_unlock;
		}
		xfs_dqflock(dqp);
	}

	ASSERT(atomic_read(&dqp->q_pincount) == 0);
	ASSERT(xlog_is_shutdown(dqp->q_logitem.qli_item.li_log) ||
		!test_bit(XFS_LI_IN_AIL, &dqp->q_logitem.qli_item.li_flags));

	xfs_dqfunlock(dqp);
	xfs_dqunlock(dqp);

	radix_tree_delete(xfs_dquot_tree(qi, xfs_dquot_type(dqp)), dqp->q_id);
	qi->qi_dquots--;

	 
	ASSERT(!list_empty(&dqp->q_lru));
	list_lru_del(&qi->qi_lru, &dqp->q_lru);
	XFS_STATS_DEC(dqp->q_mount, xs_qm_dquot_unused);

	xfs_qm_dqdestroy(dqp);
	return 0;

out_unlock:
	xfs_dqunlock(dqp);
	return error;
}

 
static void
xfs_qm_dqpurge_all(
	struct xfs_mount	*mp)
{
	xfs_qm_dquot_walk(mp, XFS_DQTYPE_USER, xfs_qm_dqpurge, NULL);
	xfs_qm_dquot_walk(mp, XFS_DQTYPE_GROUP, xfs_qm_dqpurge, NULL);
	xfs_qm_dquot_walk(mp, XFS_DQTYPE_PROJ, xfs_qm_dqpurge, NULL);
}

 
void
xfs_qm_unmount(
	struct xfs_mount	*mp)
{
	if (mp->m_quotainfo) {
		xfs_qm_dqpurge_all(mp);
		xfs_qm_destroy_quotainfo(mp);
	}
}

 
void
xfs_qm_unmount_quotas(
	xfs_mount_t	*mp)
{
	 
	ASSERT(mp->m_rootip);
	xfs_qm_dqdetach(mp->m_rootip);
	if (mp->m_rbmip)
		xfs_qm_dqdetach(mp->m_rbmip);
	if (mp->m_rsumip)
		xfs_qm_dqdetach(mp->m_rsumip);

	 
	if (mp->m_quotainfo) {
		if (mp->m_quotainfo->qi_uquotaip) {
			xfs_irele(mp->m_quotainfo->qi_uquotaip);
			mp->m_quotainfo->qi_uquotaip = NULL;
		}
		if (mp->m_quotainfo->qi_gquotaip) {
			xfs_irele(mp->m_quotainfo->qi_gquotaip);
			mp->m_quotainfo->qi_gquotaip = NULL;
		}
		if (mp->m_quotainfo->qi_pquotaip) {
			xfs_irele(mp->m_quotainfo->qi_pquotaip);
			mp->m_quotainfo->qi_pquotaip = NULL;
		}
	}
}

STATIC int
xfs_qm_dqattach_one(
	struct xfs_inode	*ip,
	xfs_dqtype_t		type,
	bool			doalloc,
	struct xfs_dquot	**IO_idqpp)
{
	struct xfs_dquot	*dqp;
	int			error;

	ASSERT(xfs_isilocked(ip, XFS_ILOCK_EXCL));
	error = 0;

	 
	dqp = *IO_idqpp;
	if (dqp) {
		trace_xfs_dqattach_found(dqp);
		return 0;
	}

	 
	error = xfs_qm_dqget_inode(ip, type, doalloc, &dqp);
	if (error)
		return error;

	trace_xfs_dqattach_get(dqp);

	 
	*IO_idqpp = dqp;
	xfs_dqunlock(dqp);
	return 0;
}

static bool
xfs_qm_need_dqattach(
	struct xfs_inode	*ip)
{
	struct xfs_mount	*mp = ip->i_mount;

	if (!XFS_IS_QUOTA_ON(mp))
		return false;
	if (!XFS_NOT_DQATTACHED(mp, ip))
		return false;
	if (xfs_is_quota_inode(&mp->m_sb, ip->i_ino))
		return false;
	return true;
}

 
int
xfs_qm_dqattach_locked(
	xfs_inode_t	*ip,
	bool		doalloc)
{
	xfs_mount_t	*mp = ip->i_mount;
	int		error = 0;

	if (!xfs_qm_need_dqattach(ip))
		return 0;

	ASSERT(xfs_isilocked(ip, XFS_ILOCK_EXCL));

	if (XFS_IS_UQUOTA_ON(mp) && !ip->i_udquot) {
		error = xfs_qm_dqattach_one(ip, XFS_DQTYPE_USER,
				doalloc, &ip->i_udquot);
		if (error)
			goto done;
		ASSERT(ip->i_udquot);
	}

	if (XFS_IS_GQUOTA_ON(mp) && !ip->i_gdquot) {
		error = xfs_qm_dqattach_one(ip, XFS_DQTYPE_GROUP,
				doalloc, &ip->i_gdquot);
		if (error)
			goto done;
		ASSERT(ip->i_gdquot);
	}

	if (XFS_IS_PQUOTA_ON(mp) && !ip->i_pdquot) {
		error = xfs_qm_dqattach_one(ip, XFS_DQTYPE_PROJ,
				doalloc, &ip->i_pdquot);
		if (error)
			goto done;
		ASSERT(ip->i_pdquot);
	}

done:
	 
	ASSERT(xfs_isilocked(ip, XFS_ILOCK_EXCL));
	return error;
}

int
xfs_qm_dqattach(
	struct xfs_inode	*ip)
{
	int			error;

	if (!xfs_qm_need_dqattach(ip))
		return 0;

	xfs_ilock(ip, XFS_ILOCK_EXCL);
	error = xfs_qm_dqattach_locked(ip, false);
	xfs_iunlock(ip, XFS_ILOCK_EXCL);

	return error;
}

 
void
xfs_qm_dqdetach(
	xfs_inode_t	*ip)
{
	if (!(ip->i_udquot || ip->i_gdquot || ip->i_pdquot))
		return;

	trace_xfs_dquot_dqdetach(ip);

	ASSERT(!xfs_is_quota_inode(&ip->i_mount->m_sb, ip->i_ino));
	if (ip->i_udquot) {
		xfs_qm_dqrele(ip->i_udquot);
		ip->i_udquot = NULL;
	}
	if (ip->i_gdquot) {
		xfs_qm_dqrele(ip->i_gdquot);
		ip->i_gdquot = NULL;
	}
	if (ip->i_pdquot) {
		xfs_qm_dqrele(ip->i_pdquot);
		ip->i_pdquot = NULL;
	}
}

struct xfs_qm_isolate {
	struct list_head	buffers;
	struct list_head	dispose;
};

static enum lru_status
xfs_qm_dquot_isolate(
	struct list_head	*item,
	struct list_lru_one	*lru,
	spinlock_t		*lru_lock,
	void			*arg)
		__releases(lru_lock) __acquires(lru_lock)
{
	struct xfs_dquot	*dqp = container_of(item,
						struct xfs_dquot, q_lru);
	struct xfs_qm_isolate	*isol = arg;

	if (!xfs_dqlock_nowait(dqp))
		goto out_miss_busy;

	 
	if (dqp->q_flags & XFS_DQFLAG_FREEING)
		goto out_miss_unlock;

	 
	if (dqp->q_nrefs) {
		xfs_dqunlock(dqp);
		XFS_STATS_INC(dqp->q_mount, xs_qm_dqwants);

		trace_xfs_dqreclaim_want(dqp);
		list_lru_isolate(lru, &dqp->q_lru);
		XFS_STATS_DEC(dqp->q_mount, xs_qm_dquot_unused);
		return LRU_REMOVED;
	}

	 
	if (!xfs_dqflock_nowait(dqp))
		goto out_miss_unlock;

	if (XFS_DQ_IS_DIRTY(dqp)) {
		struct xfs_buf	*bp = NULL;
		int		error;

		trace_xfs_dqreclaim_dirty(dqp);

		 
		spin_unlock(lru_lock);

		error = xfs_qm_dqflush(dqp, &bp);
		if (error)
			goto out_unlock_dirty;

		xfs_buf_delwri_queue(bp, &isol->buffers);
		xfs_buf_relse(bp);
		goto out_unlock_dirty;
	}
	xfs_dqfunlock(dqp);

	 
	dqp->q_flags |= XFS_DQFLAG_FREEING;
	xfs_dqunlock(dqp);

	ASSERT(dqp->q_nrefs == 0);
	list_lru_isolate_move(lru, &dqp->q_lru, &isol->dispose);
	XFS_STATS_DEC(dqp->q_mount, xs_qm_dquot_unused);
	trace_xfs_dqreclaim_done(dqp);
	XFS_STATS_INC(dqp->q_mount, xs_qm_dqreclaims);
	return LRU_REMOVED;

out_miss_unlock:
	xfs_dqunlock(dqp);
out_miss_busy:
	trace_xfs_dqreclaim_busy(dqp);
	XFS_STATS_INC(dqp->q_mount, xs_qm_dqreclaim_misses);
	return LRU_SKIP;

out_unlock_dirty:
	trace_xfs_dqreclaim_busy(dqp);
	XFS_STATS_INC(dqp->q_mount, xs_qm_dqreclaim_misses);
	xfs_dqunlock(dqp);
	spin_lock(lru_lock);
	return LRU_RETRY;
}

static unsigned long
xfs_qm_shrink_scan(
	struct shrinker		*shrink,
	struct shrink_control	*sc)
{
	struct xfs_quotainfo	*qi = container_of(shrink,
					struct xfs_quotainfo, qi_shrinker);
	struct xfs_qm_isolate	isol;
	unsigned long		freed;
	int			error;

	if ((sc->gfp_mask & (__GFP_FS|__GFP_DIRECT_RECLAIM)) != (__GFP_FS|__GFP_DIRECT_RECLAIM))
		return 0;

	INIT_LIST_HEAD(&isol.buffers);
	INIT_LIST_HEAD(&isol.dispose);

	freed = list_lru_shrink_walk(&qi->qi_lru, sc,
				     xfs_qm_dquot_isolate, &isol);

	error = xfs_buf_delwri_submit(&isol.buffers);
	if (error)
		xfs_warn(NULL, "%s: dquot reclaim failed", __func__);

	while (!list_empty(&isol.dispose)) {
		struct xfs_dquot	*dqp;

		dqp = list_first_entry(&isol.dispose, struct xfs_dquot, q_lru);
		list_del_init(&dqp->q_lru);
		xfs_qm_dqfree_one(dqp);
	}

	return freed;
}

static unsigned long
xfs_qm_shrink_count(
	struct shrinker		*shrink,
	struct shrink_control	*sc)
{
	struct xfs_quotainfo	*qi = container_of(shrink,
					struct xfs_quotainfo, qi_shrinker);

	return list_lru_shrink_count(&qi->qi_lru, sc);
}

STATIC void
xfs_qm_set_defquota(
	struct xfs_mount	*mp,
	xfs_dqtype_t		type,
	struct xfs_quotainfo	*qinf)
{
	struct xfs_dquot	*dqp;
	struct xfs_def_quota	*defq;
	int			error;

	error = xfs_qm_dqget_uncached(mp, 0, type, &dqp);
	if (error)
		return;

	defq = xfs_get_defquota(qinf, xfs_dquot_type(dqp));

	 
	defq->blk.hard = dqp->q_blk.hardlimit;
	defq->blk.soft = dqp->q_blk.softlimit;
	defq->ino.hard = dqp->q_ino.hardlimit;
	defq->ino.soft = dqp->q_ino.softlimit;
	defq->rtb.hard = dqp->q_rtb.hardlimit;
	defq->rtb.soft = dqp->q_rtb.softlimit;
	xfs_qm_dqdestroy(dqp);
}

 
static void
xfs_qm_init_timelimits(
	struct xfs_mount	*mp,
	xfs_dqtype_t		type)
{
	struct xfs_quotainfo	*qinf = mp->m_quotainfo;
	struct xfs_def_quota	*defq;
	struct xfs_dquot	*dqp;
	int			error;

	defq = xfs_get_defquota(qinf, type);

	defq->blk.time = XFS_QM_BTIMELIMIT;
	defq->ino.time = XFS_QM_ITIMELIMIT;
	defq->rtb.time = XFS_QM_RTBTIMELIMIT;

	 
	error = xfs_qm_dqget_uncached(mp, 0, type, &dqp);
	if (error)
		return;

	 
	if (dqp->q_blk.timer)
		defq->blk.time = dqp->q_blk.timer;
	if (dqp->q_ino.timer)
		defq->ino.time = dqp->q_ino.timer;
	if (dqp->q_rtb.timer)
		defq->rtb.time = dqp->q_rtb.timer;

	xfs_qm_dqdestroy(dqp);
}

 
STATIC int
xfs_qm_init_quotainfo(
	struct xfs_mount	*mp)
{
	struct xfs_quotainfo	*qinf;
	int			error;

	ASSERT(XFS_IS_QUOTA_ON(mp));

	qinf = mp->m_quotainfo = kmem_zalloc(sizeof(struct xfs_quotainfo), 0);

	error = list_lru_init(&qinf->qi_lru);
	if (error)
		goto out_free_qinf;

	 
	error = xfs_qm_init_quotainos(mp);
	if (error)
		goto out_free_lru;

	INIT_RADIX_TREE(&qinf->qi_uquota_tree, GFP_NOFS);
	INIT_RADIX_TREE(&qinf->qi_gquota_tree, GFP_NOFS);
	INIT_RADIX_TREE(&qinf->qi_pquota_tree, GFP_NOFS);
	mutex_init(&qinf->qi_tree_lock);

	 
	mutex_init(&qinf->qi_quotaofflock);

	 
	qinf->qi_dqchunklen = XFS_FSB_TO_BB(mp, XFS_DQUOT_CLUSTER_SIZE_FSB);
	qinf->qi_dqperchunk = xfs_calc_dquots_per_chunk(qinf->qi_dqchunklen);
	if (xfs_has_bigtime(mp)) {
		qinf->qi_expiry_min =
			xfs_dq_bigtime_to_unix(XFS_DQ_BIGTIME_EXPIRY_MIN);
		qinf->qi_expiry_max =
			xfs_dq_bigtime_to_unix(XFS_DQ_BIGTIME_EXPIRY_MAX);
	} else {
		qinf->qi_expiry_min = XFS_DQ_LEGACY_EXPIRY_MIN;
		qinf->qi_expiry_max = XFS_DQ_LEGACY_EXPIRY_MAX;
	}
	trace_xfs_quota_expiry_range(mp, qinf->qi_expiry_min,
			qinf->qi_expiry_max);

	mp->m_qflags |= (mp->m_sb.sb_qflags & XFS_ALL_QUOTA_CHKD);

	xfs_qm_init_timelimits(mp, XFS_DQTYPE_USER);
	xfs_qm_init_timelimits(mp, XFS_DQTYPE_GROUP);
	xfs_qm_init_timelimits(mp, XFS_DQTYPE_PROJ);

	if (XFS_IS_UQUOTA_ON(mp))
		xfs_qm_set_defquota(mp, XFS_DQTYPE_USER, qinf);
	if (XFS_IS_GQUOTA_ON(mp))
		xfs_qm_set_defquota(mp, XFS_DQTYPE_GROUP, qinf);
	if (XFS_IS_PQUOTA_ON(mp))
		xfs_qm_set_defquota(mp, XFS_DQTYPE_PROJ, qinf);

	qinf->qi_shrinker.count_objects = xfs_qm_shrink_count;
	qinf->qi_shrinker.scan_objects = xfs_qm_shrink_scan;
	qinf->qi_shrinker.seeks = DEFAULT_SEEKS;
	qinf->qi_shrinker.flags = SHRINKER_NUMA_AWARE;

	error = register_shrinker(&qinf->qi_shrinker, "xfs-qm:%s",
				  mp->m_super->s_id);
	if (error)
		goto out_free_inos;

	return 0;

out_free_inos:
	mutex_destroy(&qinf->qi_quotaofflock);
	mutex_destroy(&qinf->qi_tree_lock);
	xfs_qm_destroy_quotainos(qinf);
out_free_lru:
	list_lru_destroy(&qinf->qi_lru);
out_free_qinf:
	kmem_free(qinf);
	mp->m_quotainfo = NULL;
	return error;
}

 
void
xfs_qm_destroy_quotainfo(
	struct xfs_mount	*mp)
{
	struct xfs_quotainfo	*qi;

	qi = mp->m_quotainfo;
	ASSERT(qi != NULL);

	unregister_shrinker(&qi->qi_shrinker);
	list_lru_destroy(&qi->qi_lru);
	xfs_qm_destroy_quotainos(qi);
	mutex_destroy(&qi->qi_tree_lock);
	mutex_destroy(&qi->qi_quotaofflock);
	kmem_free(qi);
	mp->m_quotainfo = NULL;
}

 
STATIC int
xfs_qm_qino_alloc(
	struct xfs_mount	*mp,
	struct xfs_inode	**ipp,
	unsigned int		flags)
{
	struct xfs_trans	*tp;
	int			error;
	bool			need_alloc = true;

	*ipp = NULL;
	 
	if (!xfs_has_pquotino(mp) &&
			(flags & (XFS_QMOPT_PQUOTA|XFS_QMOPT_GQUOTA))) {
		xfs_ino_t ino = NULLFSINO;

		if ((flags & XFS_QMOPT_PQUOTA) &&
			     (mp->m_sb.sb_gquotino != NULLFSINO)) {
			ino = mp->m_sb.sb_gquotino;
			if (XFS_IS_CORRUPT(mp,
					   mp->m_sb.sb_pquotino != NULLFSINO))
				return -EFSCORRUPTED;
		} else if ((flags & XFS_QMOPT_GQUOTA) &&
			     (mp->m_sb.sb_pquotino != NULLFSINO)) {
			ino = mp->m_sb.sb_pquotino;
			if (XFS_IS_CORRUPT(mp,
					   mp->m_sb.sb_gquotino != NULLFSINO))
				return -EFSCORRUPTED;
		}
		if (ino != NULLFSINO) {
			error = xfs_iget(mp, NULL, ino, 0, 0, ipp);
			if (error)
				return error;
			mp->m_sb.sb_gquotino = NULLFSINO;
			mp->m_sb.sb_pquotino = NULLFSINO;
			need_alloc = false;
		}
	}

	error = xfs_trans_alloc(mp, &M_RES(mp)->tr_create,
			need_alloc ? XFS_QM_QINOCREATE_SPACE_RES(mp) : 0,
			0, 0, &tp);
	if (error)
		return error;

	if (need_alloc) {
		xfs_ino_t	ino;

		error = xfs_dialloc(&tp, 0, S_IFREG, &ino);
		if (!error)
			error = xfs_init_new_inode(&nop_mnt_idmap, tp, NULL, ino,
					S_IFREG, 1, 0, 0, false, ipp);
		if (error) {
			xfs_trans_cancel(tp);
			return error;
		}
	}

	 
	spin_lock(&mp->m_sb_lock);
	if (flags & XFS_QMOPT_SBVERSION) {
		ASSERT(!xfs_has_quota(mp));

		xfs_add_quota(mp);
		mp->m_sb.sb_uquotino = NULLFSINO;
		mp->m_sb.sb_gquotino = NULLFSINO;
		mp->m_sb.sb_pquotino = NULLFSINO;

		 
		mp->m_sb.sb_qflags = mp->m_qflags & XFS_ALL_QUOTA_ACCT;
	}
	if (flags & XFS_QMOPT_UQUOTA)
		mp->m_sb.sb_uquotino = (*ipp)->i_ino;
	else if (flags & XFS_QMOPT_GQUOTA)
		mp->m_sb.sb_gquotino = (*ipp)->i_ino;
	else
		mp->m_sb.sb_pquotino = (*ipp)->i_ino;
	spin_unlock(&mp->m_sb_lock);
	xfs_log_sb(tp);

	error = xfs_trans_commit(tp);
	if (error) {
		ASSERT(xfs_is_shutdown(mp));
		xfs_alert(mp, "%s failed (error %d)!", __func__, error);
	}
	if (need_alloc)
		xfs_finish_inode_setup(*ipp);
	return error;
}


STATIC void
xfs_qm_reset_dqcounts(
	struct xfs_mount	*mp,
	struct xfs_buf		*bp,
	xfs_dqid_t		id,
	xfs_dqtype_t		type)
{
	struct xfs_dqblk	*dqb;
	int			j;

	trace_xfs_reset_dqcounts(bp, _RET_IP_);

	 
#ifdef DEBUG
	j = (int)XFS_FSB_TO_B(mp, XFS_DQUOT_CLUSTER_SIZE_FSB) /
		sizeof(struct xfs_dqblk);
	ASSERT(mp->m_quotainfo->qi_dqperchunk == j);
#endif
	dqb = bp->b_addr;
	for (j = 0; j < mp->m_quotainfo->qi_dqperchunk; j++) {
		struct xfs_disk_dquot	*ddq;

		ddq = (struct xfs_disk_dquot *)&dqb[j];

		 
		if (xfs_dqblk_verify(mp, &dqb[j], id + j) ||
		    (dqb[j].dd_diskdq.d_type & XFS_DQTYPE_REC_MASK) != type)
			xfs_dqblk_repair(mp, &dqb[j], id + j, type);

		 
		ddq->d_type = type;
		ddq->d_bcount = 0;
		ddq->d_icount = 0;
		ddq->d_rtbcount = 0;

		 
		if (ddq->d_id != 0) {
			ddq->d_btimer = 0;
			ddq->d_itimer = 0;
			ddq->d_rtbtimer = 0;
			ddq->d_bwarns = 0;
			ddq->d_iwarns = 0;
			ddq->d_rtbwarns = 0;
			if (xfs_has_bigtime(mp))
				ddq->d_type |= XFS_DQTYPE_BIGTIME;
		}

		if (xfs_has_crc(mp)) {
			xfs_update_cksum((char *)&dqb[j],
					 sizeof(struct xfs_dqblk),
					 XFS_DQUOT_CRC_OFF);
		}
	}
}

STATIC int
xfs_qm_reset_dqcounts_all(
	struct xfs_mount	*mp,
	xfs_dqid_t		firstid,
	xfs_fsblock_t		bno,
	xfs_filblks_t		blkcnt,
	xfs_dqtype_t		type,
	struct list_head	*buffer_list)
{
	struct xfs_buf		*bp;
	int			error = 0;

	ASSERT(blkcnt > 0);

	 
	while (blkcnt--) {
		error = xfs_trans_read_buf(mp, NULL, mp->m_ddev_targp,
			      XFS_FSB_TO_DADDR(mp, bno),
			      mp->m_quotainfo->qi_dqchunklen, 0, &bp,
			      &xfs_dquot_buf_ops);

		 
		if (error == -EFSCORRUPTED) {
			error = xfs_trans_read_buf(mp, NULL, mp->m_ddev_targp,
				      XFS_FSB_TO_DADDR(mp, bno),
				      mp->m_quotainfo->qi_dqchunklen, 0, &bp,
				      NULL);
		}

		if (error)
			break;

		 
		bp->b_ops = &xfs_dquot_buf_ops;
		xfs_qm_reset_dqcounts(mp, bp, firstid, type);
		xfs_buf_delwri_queue(bp, buffer_list);
		xfs_buf_relse(bp);

		 
		bno++;
		firstid += mp->m_quotainfo->qi_dqperchunk;
	}

	return error;
}

 
STATIC int
xfs_qm_reset_dqcounts_buf(
	struct xfs_mount	*mp,
	struct xfs_inode	*qip,
	xfs_dqtype_t		type,
	struct list_head	*buffer_list)
{
	struct xfs_bmbt_irec	*map;
	int			i, nmaps;	 
	int			error;		 
	xfs_fileoff_t		lblkno;
	xfs_filblks_t		maxlblkcnt;
	xfs_dqid_t		firstid;
	xfs_fsblock_t		rablkno;
	xfs_filblks_t		rablkcnt;

	error = 0;
	 
	if (qip->i_nblocks == 0)
		return 0;

	map = kmem_alloc(XFS_DQITER_MAP_SIZE * sizeof(*map), 0);

	lblkno = 0;
	maxlblkcnt = XFS_B_TO_FSB(mp, mp->m_super->s_maxbytes);
	do {
		uint		lock_mode;

		nmaps = XFS_DQITER_MAP_SIZE;
		 
		lock_mode = xfs_ilock_data_map_shared(qip);
		error = xfs_bmapi_read(qip, lblkno, maxlblkcnt - lblkno,
				       map, &nmaps, 0);
		xfs_iunlock(qip, lock_mode);
		if (error)
			break;

		ASSERT(nmaps <= XFS_DQITER_MAP_SIZE);
		for (i = 0; i < nmaps; i++) {
			ASSERT(map[i].br_startblock != DELAYSTARTBLOCK);
			ASSERT(map[i].br_blockcount);


			lblkno += map[i].br_blockcount;

			if (map[i].br_startblock == HOLESTARTBLOCK)
				continue;

			firstid = (xfs_dqid_t) map[i].br_startoff *
				mp->m_quotainfo->qi_dqperchunk;
			 
			if ((i+1 < nmaps) &&
			    (map[i+1].br_startblock != HOLESTARTBLOCK)) {
				rablkcnt =  map[i+1].br_blockcount;
				rablkno = map[i+1].br_startblock;
				while (rablkcnt--) {
					xfs_buf_readahead(mp->m_ddev_targp,
					       XFS_FSB_TO_DADDR(mp, rablkno),
					       mp->m_quotainfo->qi_dqchunklen,
					       &xfs_dquot_buf_ops);
					rablkno++;
				}
			}
			 
			error = xfs_qm_reset_dqcounts_all(mp, firstid,
						   map[i].br_startblock,
						   map[i].br_blockcount,
						   type, buffer_list);
			if (error)
				goto out;
		}
	} while (nmaps > 0);

out:
	kmem_free(map);
	return error;
}

 
STATIC int
xfs_qm_quotacheck_dqadjust(
	struct xfs_inode	*ip,
	xfs_dqtype_t		type,
	xfs_qcnt_t		nblks,
	xfs_qcnt_t		rtblks)
{
	struct xfs_mount	*mp = ip->i_mount;
	struct xfs_dquot	*dqp;
	xfs_dqid_t		id;
	int			error;

	id = xfs_qm_id_for_quotatype(ip, type);
	error = xfs_qm_dqget(mp, id, type, true, &dqp);
	if (error) {
		 
		ASSERT(error != -ESRCH);
		ASSERT(error != -ENOENT);
		return error;
	}

	trace_xfs_dqadjust(dqp);

	 
	dqp->q_ino.count++;
	dqp->q_ino.reserved++;
	if (nblks) {
		dqp->q_blk.count += nblks;
		dqp->q_blk.reserved += nblks;
	}
	if (rtblks) {
		dqp->q_rtb.count += rtblks;
		dqp->q_rtb.reserved += rtblks;
	}

	 
	if (dqp->q_id) {
		xfs_qm_adjust_dqlimits(dqp);
		xfs_qm_adjust_dqtimers(dqp);
	}

	dqp->q_flags |= XFS_DQFLAG_DIRTY;
	xfs_qm_dqput(dqp);
	return 0;
}

 
 
STATIC int
xfs_qm_dqusage_adjust(
	struct xfs_mount	*mp,
	struct xfs_trans	*tp,
	xfs_ino_t		ino,
	void			*data)
{
	struct xfs_inode	*ip;
	xfs_qcnt_t		nblks;
	xfs_filblks_t		rtblks = 0;	 
	int			error;

	ASSERT(XFS_IS_QUOTA_ON(mp));

	 
	if (xfs_is_quota_inode(&mp->m_sb, ino))
		return 0;

	 
	error = xfs_iget(mp, tp, ino, XFS_IGET_DONTCACHE, 0, &ip);
	if (error == -EINVAL || error == -ENOENT)
		return 0;
	if (error)
		return error;

	 
	if (xfs_inode_unlinked_incomplete(ip)) {
		error = xfs_inode_reload_unlinked(ip);
		if (error) {
			xfs_force_shutdown(mp, SHUTDOWN_CORRUPT_INCORE);
			goto error0;
		}
	}

	ASSERT(ip->i_delayed_blks == 0);

	if (XFS_IS_REALTIME_INODE(ip)) {
		struct xfs_ifork	*ifp = xfs_ifork_ptr(ip, XFS_DATA_FORK);

		error = xfs_iread_extents(tp, ip, XFS_DATA_FORK);
		if (error)
			goto error0;

		xfs_bmap_count_leaves(ifp, &rtblks);
	}

	nblks = (xfs_qcnt_t)ip->i_nblocks - rtblks;
	xfs_iflags_clear(ip, XFS_IQUOTAUNCHECKED);

	 
	if (XFS_IS_UQUOTA_ON(mp)) {
		error = xfs_qm_quotacheck_dqadjust(ip, XFS_DQTYPE_USER, nblks,
				rtblks);
		if (error)
			goto error0;
	}

	if (XFS_IS_GQUOTA_ON(mp)) {
		error = xfs_qm_quotacheck_dqadjust(ip, XFS_DQTYPE_GROUP, nblks,
				rtblks);
		if (error)
			goto error0;
	}

	if (XFS_IS_PQUOTA_ON(mp)) {
		error = xfs_qm_quotacheck_dqadjust(ip, XFS_DQTYPE_PROJ, nblks,
				rtblks);
		if (error)
			goto error0;
	}

error0:
	xfs_irele(ip);
	return error;
}

STATIC int
xfs_qm_flush_one(
	struct xfs_dquot	*dqp,
	void			*data)
{
	struct xfs_mount	*mp = dqp->q_mount;
	struct list_head	*buffer_list = data;
	struct xfs_buf		*bp = NULL;
	int			error = 0;

	xfs_dqlock(dqp);
	if (dqp->q_flags & XFS_DQFLAG_FREEING)
		goto out_unlock;
	if (!XFS_DQ_IS_DIRTY(dqp))
		goto out_unlock;

	 
	if (!xfs_dqflock_nowait(dqp)) {
		 
		error = xfs_buf_incore(mp->m_ddev_targp, dqp->q_blkno,
				mp->m_quotainfo->qi_dqchunklen, 0, &bp);
		if (error)
			goto out_unlock;

		if (!(bp->b_flags & _XBF_DELWRI_Q)) {
			error = -EAGAIN;
			xfs_buf_relse(bp);
			goto out_unlock;
		}
		xfs_buf_unlock(bp);

		xfs_buf_delwri_pushbuf(bp, buffer_list);
		xfs_buf_rele(bp);

		error = -EAGAIN;
		goto out_unlock;
	}

	error = xfs_qm_dqflush(dqp, &bp);
	if (error)
		goto out_unlock;

	xfs_buf_delwri_queue(bp, buffer_list);
	xfs_buf_relse(bp);
out_unlock:
	xfs_dqunlock(dqp);
	return error;
}

 
STATIC int
xfs_qm_quotacheck(
	xfs_mount_t	*mp)
{
	int			error, error2;
	uint			flags;
	LIST_HEAD		(buffer_list);
	struct xfs_inode	*uip = mp->m_quotainfo->qi_uquotaip;
	struct xfs_inode	*gip = mp->m_quotainfo->qi_gquotaip;
	struct xfs_inode	*pip = mp->m_quotainfo->qi_pquotaip;

	flags = 0;

	ASSERT(uip || gip || pip);
	ASSERT(XFS_IS_QUOTA_ON(mp));

	xfs_notice(mp, "Quotacheck needed: Please wait.");

	 
	if (uip) {
		error = xfs_qm_reset_dqcounts_buf(mp, uip, XFS_DQTYPE_USER,
					 &buffer_list);
		if (error)
			goto error_return;
		flags |= XFS_UQUOTA_CHKD;
	}

	if (gip) {
		error = xfs_qm_reset_dqcounts_buf(mp, gip, XFS_DQTYPE_GROUP,
					 &buffer_list);
		if (error)
			goto error_return;
		flags |= XFS_GQUOTA_CHKD;
	}

	if (pip) {
		error = xfs_qm_reset_dqcounts_buf(mp, pip, XFS_DQTYPE_PROJ,
					 &buffer_list);
		if (error)
			goto error_return;
		flags |= XFS_PQUOTA_CHKD;
	}

	xfs_set_quotacheck_running(mp);
	error = xfs_iwalk_threaded(mp, 0, 0, xfs_qm_dqusage_adjust, 0, true,
			NULL);
	xfs_clear_quotacheck_running(mp);

	 
	if (error)
		goto error_purge;

	 
	if (XFS_IS_UQUOTA_ON(mp)) {
		error = xfs_qm_dquot_walk(mp, XFS_DQTYPE_USER, xfs_qm_flush_one,
					  &buffer_list);
	}
	if (XFS_IS_GQUOTA_ON(mp)) {
		error2 = xfs_qm_dquot_walk(mp, XFS_DQTYPE_GROUP, xfs_qm_flush_one,
					   &buffer_list);
		if (!error)
			error = error2;
	}
	if (XFS_IS_PQUOTA_ON(mp)) {
		error2 = xfs_qm_dquot_walk(mp, XFS_DQTYPE_PROJ, xfs_qm_flush_one,
					   &buffer_list);
		if (!error)
			error = error2;
	}

	error2 = xfs_buf_delwri_submit(&buffer_list);
	if (!error)
		error = error2;

	 
	if (error)
		goto error_purge;

	 
	mp->m_qflags &= ~XFS_ALL_QUOTA_CHKD;
	mp->m_qflags |= flags;

error_return:
	xfs_buf_delwri_cancel(&buffer_list);

	if (error) {
		xfs_warn(mp,
	"Quotacheck: Unsuccessful (Error %d): Disabling quotas.",
			error);
		 
		ASSERT(mp->m_quotainfo != NULL);
		xfs_qm_destroy_quotainfo(mp);
		if (xfs_mount_reset_sbqflags(mp)) {
			xfs_warn(mp,
				"Quotacheck: Failed to reset quota flags.");
		}
	} else
		xfs_notice(mp, "Quotacheck: Done.");
	return error;

error_purge:
	 
	xfs_inodegc_flush(mp);
	xfs_qm_dqpurge_all(mp);
	goto error_return;

}

 
void
xfs_qm_mount_quotas(
	struct xfs_mount	*mp)
{
	int			error = 0;
	uint			sbf;

	 
	if (mp->m_sb.sb_rextents) {
		xfs_notice(mp, "Cannot turn on quotas for realtime filesystem");
		mp->m_qflags = 0;
		goto write_changes;
	}

	ASSERT(XFS_IS_QUOTA_ON(mp));

	 
	error = xfs_qm_init_quotainfo(mp);
	if (error) {
		 
		ASSERT(mp->m_quotainfo == NULL);
		mp->m_qflags = 0;
		goto write_changes;
	}
	 
	if (XFS_QM_NEED_QUOTACHECK(mp)) {
		error = xfs_qm_quotacheck(mp);
		if (error) {
			 
			return;
		}
	}
	 
	if (!XFS_IS_UQUOTA_ON(mp))
		mp->m_qflags &= ~XFS_UQUOTA_CHKD;
	if (!XFS_IS_GQUOTA_ON(mp))
		mp->m_qflags &= ~XFS_GQUOTA_CHKD;
	if (!XFS_IS_PQUOTA_ON(mp))
		mp->m_qflags &= ~XFS_PQUOTA_CHKD;

 write_changes:
	 
	spin_lock(&mp->m_sb_lock);
	sbf = mp->m_sb.sb_qflags;
	mp->m_sb.sb_qflags = mp->m_qflags & XFS_MOUNT_QUOTA_ALL;
	spin_unlock(&mp->m_sb_lock);

	if (sbf != (mp->m_qflags & XFS_MOUNT_QUOTA_ALL)) {
		if (xfs_sync_sb(mp, false)) {
			 
			ASSERT(!(XFS_IS_QUOTA_ON(mp)));
			xfs_alert(mp, "%s: Superblock update failed!",
				__func__);
		}
	}

	if (error) {
		xfs_warn(mp, "Failed to initialize disk quotas.");
		return;
	}
}

 
STATIC int
xfs_qm_init_quotainos(
	xfs_mount_t	*mp)
{
	struct xfs_inode	*uip = NULL;
	struct xfs_inode	*gip = NULL;
	struct xfs_inode	*pip = NULL;
	int			error;
	uint			flags = 0;

	ASSERT(mp->m_quotainfo);

	 
	if (xfs_has_quota(mp)) {
		if (XFS_IS_UQUOTA_ON(mp) &&
		    mp->m_sb.sb_uquotino != NULLFSINO) {
			ASSERT(mp->m_sb.sb_uquotino > 0);
			error = xfs_iget(mp, NULL, mp->m_sb.sb_uquotino,
					     0, 0, &uip);
			if (error)
				return error;
		}
		if (XFS_IS_GQUOTA_ON(mp) &&
		    mp->m_sb.sb_gquotino != NULLFSINO) {
			ASSERT(mp->m_sb.sb_gquotino > 0);
			error = xfs_iget(mp, NULL, mp->m_sb.sb_gquotino,
					     0, 0, &gip);
			if (error)
				goto error_rele;
		}
		if (XFS_IS_PQUOTA_ON(mp) &&
		    mp->m_sb.sb_pquotino != NULLFSINO) {
			ASSERT(mp->m_sb.sb_pquotino > 0);
			error = xfs_iget(mp, NULL, mp->m_sb.sb_pquotino,
					     0, 0, &pip);
			if (error)
				goto error_rele;
		}
	} else {
		flags |= XFS_QMOPT_SBVERSION;
	}

	 
	if (XFS_IS_UQUOTA_ON(mp) && uip == NULL) {
		error = xfs_qm_qino_alloc(mp, &uip,
					      flags | XFS_QMOPT_UQUOTA);
		if (error)
			goto error_rele;

		flags &= ~XFS_QMOPT_SBVERSION;
	}
	if (XFS_IS_GQUOTA_ON(mp) && gip == NULL) {
		error = xfs_qm_qino_alloc(mp, &gip,
					  flags | XFS_QMOPT_GQUOTA);
		if (error)
			goto error_rele;

		flags &= ~XFS_QMOPT_SBVERSION;
	}
	if (XFS_IS_PQUOTA_ON(mp) && pip == NULL) {
		error = xfs_qm_qino_alloc(mp, &pip,
					  flags | XFS_QMOPT_PQUOTA);
		if (error)
			goto error_rele;
	}

	mp->m_quotainfo->qi_uquotaip = uip;
	mp->m_quotainfo->qi_gquotaip = gip;
	mp->m_quotainfo->qi_pquotaip = pip;

	return 0;

error_rele:
	if (uip)
		xfs_irele(uip);
	if (gip)
		xfs_irele(gip);
	if (pip)
		xfs_irele(pip);
	return error;
}

STATIC void
xfs_qm_destroy_quotainos(
	struct xfs_quotainfo	*qi)
{
	if (qi->qi_uquotaip) {
		xfs_irele(qi->qi_uquotaip);
		qi->qi_uquotaip = NULL;  
	}
	if (qi->qi_gquotaip) {
		xfs_irele(qi->qi_gquotaip);
		qi->qi_gquotaip = NULL;
	}
	if (qi->qi_pquotaip) {
		xfs_irele(qi->qi_pquotaip);
		qi->qi_pquotaip = NULL;
	}
}

STATIC void
xfs_qm_dqfree_one(
	struct xfs_dquot	*dqp)
{
	struct xfs_mount	*mp = dqp->q_mount;
	struct xfs_quotainfo	*qi = mp->m_quotainfo;

	mutex_lock(&qi->qi_tree_lock);
	radix_tree_delete(xfs_dquot_tree(qi, xfs_dquot_type(dqp)), dqp->q_id);

	qi->qi_dquots--;
	mutex_unlock(&qi->qi_tree_lock);

	xfs_qm_dqdestroy(dqp);
}

 


 
int
xfs_qm_vop_dqalloc(
	struct xfs_inode	*ip,
	kuid_t			uid,
	kgid_t			gid,
	prid_t			prid,
	uint			flags,
	struct xfs_dquot	**O_udqpp,
	struct xfs_dquot	**O_gdqpp,
	struct xfs_dquot	**O_pdqpp)
{
	struct xfs_mount	*mp = ip->i_mount;
	struct inode		*inode = VFS_I(ip);
	struct user_namespace	*user_ns = inode->i_sb->s_user_ns;
	struct xfs_dquot	*uq = NULL;
	struct xfs_dquot	*gq = NULL;
	struct xfs_dquot	*pq = NULL;
	int			error;
	uint			lockflags;

	if (!XFS_IS_QUOTA_ON(mp))
		return 0;

	lockflags = XFS_ILOCK_EXCL;
	xfs_ilock(ip, lockflags);

	if ((flags & XFS_QMOPT_INHERIT) && XFS_INHERIT_GID(ip))
		gid = inode->i_gid;

	 
	if (XFS_NOT_DQATTACHED(mp, ip)) {
		error = xfs_qm_dqattach_locked(ip, true);
		if (error) {
			xfs_iunlock(ip, lockflags);
			return error;
		}
	}

	if ((flags & XFS_QMOPT_UQUOTA) && XFS_IS_UQUOTA_ON(mp)) {
		ASSERT(O_udqpp);
		if (!uid_eq(inode->i_uid, uid)) {
			 
			xfs_iunlock(ip, lockflags);
			error = xfs_qm_dqget(mp, from_kuid(user_ns, uid),
					XFS_DQTYPE_USER, true, &uq);
			if (error) {
				ASSERT(error != -ENOENT);
				return error;
			}
			 
			xfs_dqunlock(uq);
			lockflags = XFS_ILOCK_SHARED;
			xfs_ilock(ip, lockflags);
		} else {
			 
			ASSERT(ip->i_udquot);
			uq = xfs_qm_dqhold(ip->i_udquot);
		}
	}
	if ((flags & XFS_QMOPT_GQUOTA) && XFS_IS_GQUOTA_ON(mp)) {
		ASSERT(O_gdqpp);
		if (!gid_eq(inode->i_gid, gid)) {
			xfs_iunlock(ip, lockflags);
			error = xfs_qm_dqget(mp, from_kgid(user_ns, gid),
					XFS_DQTYPE_GROUP, true, &gq);
			if (error) {
				ASSERT(error != -ENOENT);
				goto error_rele;
			}
			xfs_dqunlock(gq);
			lockflags = XFS_ILOCK_SHARED;
			xfs_ilock(ip, lockflags);
		} else {
			ASSERT(ip->i_gdquot);
			gq = xfs_qm_dqhold(ip->i_gdquot);
		}
	}
	if ((flags & XFS_QMOPT_PQUOTA) && XFS_IS_PQUOTA_ON(mp)) {
		ASSERT(O_pdqpp);
		if (ip->i_projid != prid) {
			xfs_iunlock(ip, lockflags);
			error = xfs_qm_dqget(mp, prid,
					XFS_DQTYPE_PROJ, true, &pq);
			if (error) {
				ASSERT(error != -ENOENT);
				goto error_rele;
			}
			xfs_dqunlock(pq);
			lockflags = XFS_ILOCK_SHARED;
			xfs_ilock(ip, lockflags);
		} else {
			ASSERT(ip->i_pdquot);
			pq = xfs_qm_dqhold(ip->i_pdquot);
		}
	}
	trace_xfs_dquot_dqalloc(ip);

	xfs_iunlock(ip, lockflags);
	if (O_udqpp)
		*O_udqpp = uq;
	else
		xfs_qm_dqrele(uq);
	if (O_gdqpp)
		*O_gdqpp = gq;
	else
		xfs_qm_dqrele(gq);
	if (O_pdqpp)
		*O_pdqpp = pq;
	else
		xfs_qm_dqrele(pq);
	return 0;

error_rele:
	xfs_qm_dqrele(gq);
	xfs_qm_dqrele(uq);
	return error;
}

 
struct xfs_dquot *
xfs_qm_vop_chown(
	struct xfs_trans	*tp,
	struct xfs_inode	*ip,
	struct xfs_dquot	**IO_olddq,
	struct xfs_dquot	*newdq)
{
	struct xfs_dquot	*prevdq;
	uint		bfield = XFS_IS_REALTIME_INODE(ip) ?
				 XFS_TRANS_DQ_RTBCOUNT : XFS_TRANS_DQ_BCOUNT;


	ASSERT(xfs_isilocked(ip, XFS_ILOCK_EXCL));
	ASSERT(XFS_IS_QUOTA_ON(ip->i_mount));

	 
	prevdq = *IO_olddq;
	ASSERT(prevdq);
	ASSERT(prevdq != newdq);

	xfs_trans_mod_dquot(tp, prevdq, bfield, -(ip->i_nblocks));
	xfs_trans_mod_dquot(tp, prevdq, XFS_TRANS_DQ_ICOUNT, -1);

	 
	xfs_trans_mod_dquot(tp, newdq, bfield, ip->i_nblocks);
	xfs_trans_mod_dquot(tp, newdq, XFS_TRANS_DQ_ICOUNT, 1);

	 
	xfs_trans_mod_dquot(tp, newdq, XFS_TRANS_DQ_RES_BLKS,
			-ip->i_delayed_blks);

	 
	tp->t_flags |= XFS_TRANS_DIRTY;
	xfs_dqlock(prevdq);
	ASSERT(prevdq->q_blk.reserved >= ip->i_delayed_blks);
	prevdq->q_blk.reserved -= ip->i_delayed_blks;
	xfs_dqunlock(prevdq);

	 
	*IO_olddq = xfs_qm_dqhold(newdq);

	return prevdq;
}

int
xfs_qm_vop_rename_dqattach(
	struct xfs_inode	**i_tab)
{
	struct xfs_mount	*mp = i_tab[0]->i_mount;
	int			i;

	if (!XFS_IS_QUOTA_ON(mp))
		return 0;

	for (i = 0; (i < 4 && i_tab[i]); i++) {
		struct xfs_inode	*ip = i_tab[i];
		int			error;

		 
		if (i == 0 || ip != i_tab[i-1]) {
			if (XFS_NOT_DQATTACHED(mp, ip)) {
				error = xfs_qm_dqattach(ip);
				if (error)
					return error;
			}
		}
	}
	return 0;
}

void
xfs_qm_vop_create_dqattach(
	struct xfs_trans	*tp,
	struct xfs_inode	*ip,
	struct xfs_dquot	*udqp,
	struct xfs_dquot	*gdqp,
	struct xfs_dquot	*pdqp)
{
	struct xfs_mount	*mp = tp->t_mountp;

	if (!XFS_IS_QUOTA_ON(mp))
		return;

	ASSERT(xfs_isilocked(ip, XFS_ILOCK_EXCL));

	if (udqp && XFS_IS_UQUOTA_ON(mp)) {
		ASSERT(ip->i_udquot == NULL);
		ASSERT(i_uid_read(VFS_I(ip)) == udqp->q_id);

		ip->i_udquot = xfs_qm_dqhold(udqp);
		xfs_trans_mod_dquot(tp, udqp, XFS_TRANS_DQ_ICOUNT, 1);
	}
	if (gdqp && XFS_IS_GQUOTA_ON(mp)) {
		ASSERT(ip->i_gdquot == NULL);
		ASSERT(i_gid_read(VFS_I(ip)) == gdqp->q_id);

		ip->i_gdquot = xfs_qm_dqhold(gdqp);
		xfs_trans_mod_dquot(tp, gdqp, XFS_TRANS_DQ_ICOUNT, 1);
	}
	if (pdqp && XFS_IS_PQUOTA_ON(mp)) {
		ASSERT(ip->i_pdquot == NULL);
		ASSERT(ip->i_projid == pdqp->q_id);

		ip->i_pdquot = xfs_qm_dqhold(pdqp);
		xfs_trans_mod_dquot(tp, pdqp, XFS_TRANS_DQ_ICOUNT, 1);
	}
}

 
bool
xfs_inode_near_dquot_enforcement(
	struct xfs_inode	*ip,
	xfs_dqtype_t		type)
{
	struct xfs_dquot	*dqp;
	int64_t			freesp;

	 
	dqp = xfs_inode_dquot(ip, type);
	if (!dqp || !xfs_dquot_is_enforced(dqp))
		return false;

	if (xfs_dquot_res_over_limits(&dqp->q_ino) ||
	    xfs_dquot_res_over_limits(&dqp->q_rtb))
		return true;

	 
	if (!dqp->q_prealloc_hi_wmark)
		return false;

	if (dqp->q_blk.reserved < dqp->q_prealloc_lo_wmark)
		return false;

	if (dqp->q_blk.reserved >= dqp->q_prealloc_hi_wmark)
		return true;

	freesp = dqp->q_prealloc_hi_wmark - dqp->q_blk.reserved;
	if (freesp < dqp->q_low_space[XFS_QLOWSP_5_PCNT])
		return true;

	return false;
}
