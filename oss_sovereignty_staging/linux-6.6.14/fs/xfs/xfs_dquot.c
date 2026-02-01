
 
#include "xfs.h"
#include "xfs_fs.h"
#include "xfs_format.h"
#include "xfs_log_format.h"
#include "xfs_shared.h"
#include "xfs_trans_resv.h"
#include "xfs_bit.h"
#include "xfs_mount.h"
#include "xfs_defer.h"
#include "xfs_inode.h"
#include "xfs_bmap.h"
#include "xfs_quota.h"
#include "xfs_trans.h"
#include "xfs_buf_item.h"
#include "xfs_trans_space.h"
#include "xfs_trans_priv.h"
#include "xfs_qm.h"
#include "xfs_trace.h"
#include "xfs_log.h"
#include "xfs_bmap_btree.h"
#include "xfs_error.h"

 

struct kmem_cache		*xfs_dqtrx_cache;
static struct kmem_cache	*xfs_dquot_cache;

static struct lock_class_key xfs_dquot_group_class;
static struct lock_class_key xfs_dquot_project_class;

 
void
xfs_qm_dqdestroy(
	struct xfs_dquot	*dqp)
{
	ASSERT(list_empty(&dqp->q_lru));

	kmem_free(dqp->q_logitem.qli_item.li_lv_shadow);
	mutex_destroy(&dqp->q_qlock);

	XFS_STATS_DEC(dqp->q_mount, xs_qm_dquot);
	kmem_cache_free(xfs_dquot_cache, dqp);
}

 
void
xfs_qm_adjust_dqlimits(
	struct xfs_dquot	*dq)
{
	struct xfs_mount	*mp = dq->q_mount;
	struct xfs_quotainfo	*q = mp->m_quotainfo;
	struct xfs_def_quota	*defq;
	int			prealloc = 0;

	ASSERT(dq->q_id);
	defq = xfs_get_defquota(q, xfs_dquot_type(dq));

	if (!dq->q_blk.softlimit) {
		dq->q_blk.softlimit = defq->blk.soft;
		prealloc = 1;
	}
	if (!dq->q_blk.hardlimit) {
		dq->q_blk.hardlimit = defq->blk.hard;
		prealloc = 1;
	}
	if (!dq->q_ino.softlimit)
		dq->q_ino.softlimit = defq->ino.soft;
	if (!dq->q_ino.hardlimit)
		dq->q_ino.hardlimit = defq->ino.hard;
	if (!dq->q_rtb.softlimit)
		dq->q_rtb.softlimit = defq->rtb.soft;
	if (!dq->q_rtb.hardlimit)
		dq->q_rtb.hardlimit = defq->rtb.hard;

	if (prealloc)
		xfs_dquot_set_prealloc_limits(dq);
}

 
time64_t
xfs_dquot_set_timeout(
	struct xfs_mount	*mp,
	time64_t		timeout)
{
	struct xfs_quotainfo	*qi = mp->m_quotainfo;

	return clamp_t(time64_t, timeout, qi->qi_expiry_min,
					  qi->qi_expiry_max);
}

 
time64_t
xfs_dquot_set_grace_period(
	time64_t		grace)
{
	return clamp_t(time64_t, grace, XFS_DQ_GRACE_MIN, XFS_DQ_GRACE_MAX);
}

 
static inline void
xfs_qm_adjust_res_timer(
	struct xfs_mount	*mp,
	struct xfs_dquot_res	*res,
	struct xfs_quota_limits	*qlim)
{
	ASSERT(res->hardlimit == 0 || res->softlimit <= res->hardlimit);

	if ((res->softlimit && res->count > res->softlimit) ||
	    (res->hardlimit && res->count > res->hardlimit)) {
		if (res->timer == 0)
			res->timer = xfs_dquot_set_timeout(mp,
					ktime_get_real_seconds() + qlim->time);
	} else {
		res->timer = 0;
	}
}

 
void
xfs_qm_adjust_dqtimers(
	struct xfs_dquot	*dq)
{
	struct xfs_mount	*mp = dq->q_mount;
	struct xfs_quotainfo	*qi = mp->m_quotainfo;
	struct xfs_def_quota	*defq;

	ASSERT(dq->q_id);
	defq = xfs_get_defquota(qi, xfs_dquot_type(dq));

	xfs_qm_adjust_res_timer(dq->q_mount, &dq->q_blk, &defq->blk);
	xfs_qm_adjust_res_timer(dq->q_mount, &dq->q_ino, &defq->ino);
	xfs_qm_adjust_res_timer(dq->q_mount, &dq->q_rtb, &defq->rtb);
}

 
STATIC void
xfs_qm_init_dquot_blk(
	struct xfs_trans	*tp,
	struct xfs_mount	*mp,
	xfs_dqid_t		id,
	xfs_dqtype_t		type,
	struct xfs_buf		*bp)
{
	struct xfs_quotainfo	*q = mp->m_quotainfo;
	struct xfs_dqblk	*d;
	xfs_dqid_t		curid;
	unsigned int		qflag;
	unsigned int		blftype;
	int			i;

	ASSERT(tp);
	ASSERT(xfs_buf_islocked(bp));

	switch (type) {
	case XFS_DQTYPE_USER:
		qflag = XFS_UQUOTA_CHKD;
		blftype = XFS_BLF_UDQUOT_BUF;
		break;
	case XFS_DQTYPE_PROJ:
		qflag = XFS_PQUOTA_CHKD;
		blftype = XFS_BLF_PDQUOT_BUF;
		break;
	case XFS_DQTYPE_GROUP:
		qflag = XFS_GQUOTA_CHKD;
		blftype = XFS_BLF_GDQUOT_BUF;
		break;
	default:
		ASSERT(0);
		return;
	}

	d = bp->b_addr;

	 
	curid = id - (id % q->qi_dqperchunk);
	memset(d, 0, BBTOB(q->qi_dqchunklen));
	for (i = 0; i < q->qi_dqperchunk; i++, d++, curid++) {
		d->dd_diskdq.d_magic = cpu_to_be16(XFS_DQUOT_MAGIC);
		d->dd_diskdq.d_version = XFS_DQUOT_VERSION;
		d->dd_diskdq.d_id = cpu_to_be32(curid);
		d->dd_diskdq.d_type = type;
		if (curid > 0 && xfs_has_bigtime(mp))
			d->dd_diskdq.d_type |= XFS_DQTYPE_BIGTIME;
		if (xfs_has_crc(mp)) {
			uuid_copy(&d->dd_uuid, &mp->m_sb.sb_meta_uuid);
			xfs_update_cksum((char *)d, sizeof(struct xfs_dqblk),
					 XFS_DQUOT_CRC_OFF);
		}
	}

	xfs_trans_dquot_buf(tp, bp, blftype);

	 
	if (!(mp->m_qflags & qflag))
		xfs_trans_ordered_buf(tp, bp);
	else
		xfs_trans_log_buf(tp, bp, 0, BBTOB(q->qi_dqchunklen) - 1);
}

 
void
xfs_dquot_set_prealloc_limits(struct xfs_dquot *dqp)
{
	uint64_t space;

	dqp->q_prealloc_hi_wmark = dqp->q_blk.hardlimit;
	dqp->q_prealloc_lo_wmark = dqp->q_blk.softlimit;
	if (!dqp->q_prealloc_lo_wmark) {
		dqp->q_prealloc_lo_wmark = dqp->q_prealloc_hi_wmark;
		do_div(dqp->q_prealloc_lo_wmark, 100);
		dqp->q_prealloc_lo_wmark *= 95;
	}

	space = dqp->q_prealloc_hi_wmark;

	do_div(space, 100);
	dqp->q_low_space[XFS_QLOWSP_1_PCNT] = space;
	dqp->q_low_space[XFS_QLOWSP_3_PCNT] = space * 3;
	dqp->q_low_space[XFS_QLOWSP_5_PCNT] = space * 5;
}

 
STATIC int
xfs_dquot_disk_alloc(
	struct xfs_dquot	*dqp,
	struct xfs_buf		**bpp)
{
	struct xfs_bmbt_irec	map;
	struct xfs_trans	*tp;
	struct xfs_mount	*mp = dqp->q_mount;
	struct xfs_buf		*bp;
	xfs_dqtype_t		qtype = xfs_dquot_type(dqp);
	struct xfs_inode	*quotip = xfs_quota_inode(mp, qtype);
	int			nmaps = 1;
	int			error;

	trace_xfs_dqalloc(dqp);

	error = xfs_trans_alloc(mp, &M_RES(mp)->tr_qm_dqalloc,
			XFS_QM_DQALLOC_SPACE_RES(mp), 0, 0, &tp);
	if (error)
		return error;

	xfs_ilock(quotip, XFS_ILOCK_EXCL);
	xfs_trans_ijoin(tp, quotip, 0);

	if (!xfs_this_quota_on(dqp->q_mount, qtype)) {
		 
		error = -ESRCH;
		goto err_cancel;
	}

	error = xfs_iext_count_may_overflow(quotip, XFS_DATA_FORK,
			XFS_IEXT_ADD_NOSPLIT_CNT);
	if (error == -EFBIG)
		error = xfs_iext_count_upgrade(tp, quotip,
				XFS_IEXT_ADD_NOSPLIT_CNT);
	if (error)
		goto err_cancel;

	 
	error = xfs_bmapi_write(tp, quotip, dqp->q_fileoffset,
			XFS_DQUOT_CLUSTER_SIZE_FSB, XFS_BMAPI_METADATA, 0, &map,
			&nmaps);
	if (error)
		goto err_cancel;

	ASSERT(map.br_blockcount == XFS_DQUOT_CLUSTER_SIZE_FSB);
	ASSERT(nmaps == 1);
	ASSERT((map.br_startblock != DELAYSTARTBLOCK) &&
	       (map.br_startblock != HOLESTARTBLOCK));

	 
	dqp->q_blkno = XFS_FSB_TO_DADDR(mp, map.br_startblock);

	 
	error = xfs_trans_get_buf(tp, mp->m_ddev_targp, dqp->q_blkno,
			mp->m_quotainfo->qi_dqchunklen, 0, &bp);
	if (error)
		goto err_cancel;
	bp->b_ops = &xfs_dquot_buf_ops;

	 
	xfs_qm_init_dquot_blk(tp, mp, dqp->q_id, qtype, bp);
	xfs_buf_set_ref(bp, XFS_DQUOT_REF);

	 
	xfs_trans_bhold(tp, bp);
	error = xfs_trans_commit(tp);
	xfs_iunlock(quotip, XFS_ILOCK_EXCL);
	if (error) {
		xfs_buf_relse(bp);
		return error;
	}

	*bpp = bp;
	return 0;

err_cancel:
	xfs_trans_cancel(tp);
	xfs_iunlock(quotip, XFS_ILOCK_EXCL);
	return error;
}

 
STATIC int
xfs_dquot_disk_read(
	struct xfs_mount	*mp,
	struct xfs_dquot	*dqp,
	struct xfs_buf		**bpp)
{
	struct xfs_bmbt_irec	map;
	struct xfs_buf		*bp;
	xfs_dqtype_t		qtype = xfs_dquot_type(dqp);
	struct xfs_inode	*quotip = xfs_quota_inode(mp, qtype);
	uint			lock_mode;
	int			nmaps = 1;
	int			error;

	lock_mode = xfs_ilock_data_map_shared(quotip);
	if (!xfs_this_quota_on(mp, qtype)) {
		 
		xfs_iunlock(quotip, lock_mode);
		return -ESRCH;
	}

	 
	error = xfs_bmapi_read(quotip, dqp->q_fileoffset,
			XFS_DQUOT_CLUSTER_SIZE_FSB, &map, &nmaps, 0);
	xfs_iunlock(quotip, lock_mode);
	if (error)
		return error;

	ASSERT(nmaps == 1);
	ASSERT(map.br_blockcount >= 1);
	ASSERT(map.br_startblock != DELAYSTARTBLOCK);
	if (map.br_startblock == HOLESTARTBLOCK)
		return -ENOENT;

	trace_xfs_dqtobp_read(dqp);

	 
	dqp->q_blkno = XFS_FSB_TO_DADDR(mp, map.br_startblock);

	error = xfs_trans_read_buf(mp, NULL, mp->m_ddev_targp, dqp->q_blkno,
			mp->m_quotainfo->qi_dqchunklen, 0, &bp,
			&xfs_dquot_buf_ops);
	if (error) {
		ASSERT(bp == NULL);
		return error;
	}

	ASSERT(xfs_buf_islocked(bp));
	xfs_buf_set_ref(bp, XFS_DQUOT_REF);
	*bpp = bp;

	return 0;
}

 
STATIC struct xfs_dquot *
xfs_dquot_alloc(
	struct xfs_mount	*mp,
	xfs_dqid_t		id,
	xfs_dqtype_t		type)
{
	struct xfs_dquot	*dqp;

	dqp = kmem_cache_zalloc(xfs_dquot_cache, GFP_KERNEL | __GFP_NOFAIL);

	dqp->q_type = type;
	dqp->q_id = id;
	dqp->q_mount = mp;
	INIT_LIST_HEAD(&dqp->q_lru);
	mutex_init(&dqp->q_qlock);
	init_waitqueue_head(&dqp->q_pinwait);
	dqp->q_fileoffset = (xfs_fileoff_t)id / mp->m_quotainfo->qi_dqperchunk;
	 
	dqp->q_bufoffset = (id % mp->m_quotainfo->qi_dqperchunk) *
			sizeof(struct xfs_dqblk);

	 
	init_completion(&dqp->q_flush);
	complete(&dqp->q_flush);

	 
	switch (type) {
	case XFS_DQTYPE_USER:
		 
		break;
	case XFS_DQTYPE_GROUP:
		lockdep_set_class(&dqp->q_qlock, &xfs_dquot_group_class);
		break;
	case XFS_DQTYPE_PROJ:
		lockdep_set_class(&dqp->q_qlock, &xfs_dquot_project_class);
		break;
	default:
		ASSERT(0);
		break;
	}

	xfs_qm_dquot_logitem_init(dqp);

	XFS_STATS_INC(mp, xs_qm_dquot);
	return dqp;
}

 
static bool
xfs_dquot_check_type(
	struct xfs_dquot	*dqp,
	struct xfs_disk_dquot	*ddqp)
{
	uint8_t			ddqp_type;
	uint8_t			dqp_type;

	ddqp_type = ddqp->d_type & XFS_DQTYPE_REC_MASK;
	dqp_type = xfs_dquot_type(dqp);

	if (be32_to_cpu(ddqp->d_id) != dqp->q_id)
		return false;

	 
	if (xfs_has_crc(dqp->q_mount) ||
	    dqp_type == XFS_DQTYPE_USER || dqp->q_id != 0)
		return ddqp_type == dqp_type;

	 
	return ddqp_type == XFS_DQTYPE_GROUP || ddqp_type == XFS_DQTYPE_PROJ;
}

 
STATIC int
xfs_dquot_from_disk(
	struct xfs_dquot	*dqp,
	struct xfs_buf		*bp)
{
	struct xfs_disk_dquot	*ddqp = bp->b_addr + dqp->q_bufoffset;

	 
	if (!xfs_dquot_check_type(dqp, ddqp)) {
		xfs_alert_tag(bp->b_mount, XFS_PTAG_VERIFIER_ERROR,
			  "Metadata corruption detected at %pS, quota %u",
			  __this_address, dqp->q_id);
		xfs_alert(bp->b_mount, "Unmount and run xfs_repair");
		return -EFSCORRUPTED;
	}

	 
	dqp->q_type = ddqp->d_type;
	dqp->q_blk.hardlimit = be64_to_cpu(ddqp->d_blk_hardlimit);
	dqp->q_blk.softlimit = be64_to_cpu(ddqp->d_blk_softlimit);
	dqp->q_ino.hardlimit = be64_to_cpu(ddqp->d_ino_hardlimit);
	dqp->q_ino.softlimit = be64_to_cpu(ddqp->d_ino_softlimit);
	dqp->q_rtb.hardlimit = be64_to_cpu(ddqp->d_rtb_hardlimit);
	dqp->q_rtb.softlimit = be64_to_cpu(ddqp->d_rtb_softlimit);

	dqp->q_blk.count = be64_to_cpu(ddqp->d_bcount);
	dqp->q_ino.count = be64_to_cpu(ddqp->d_icount);
	dqp->q_rtb.count = be64_to_cpu(ddqp->d_rtbcount);

	dqp->q_blk.timer = xfs_dquot_from_disk_ts(ddqp, ddqp->d_btimer);
	dqp->q_ino.timer = xfs_dquot_from_disk_ts(ddqp, ddqp->d_itimer);
	dqp->q_rtb.timer = xfs_dquot_from_disk_ts(ddqp, ddqp->d_rtbtimer);

	 
	dqp->q_blk.reserved = dqp->q_blk.count;
	dqp->q_ino.reserved = dqp->q_ino.count;
	dqp->q_rtb.reserved = dqp->q_rtb.count;

	 
	xfs_dquot_set_prealloc_limits(dqp);
	return 0;
}

 
void
xfs_dquot_to_disk(
	struct xfs_disk_dquot	*ddqp,
	struct xfs_dquot	*dqp)
{
	ddqp->d_magic = cpu_to_be16(XFS_DQUOT_MAGIC);
	ddqp->d_version = XFS_DQUOT_VERSION;
	ddqp->d_type = dqp->q_type;
	ddqp->d_id = cpu_to_be32(dqp->q_id);
	ddqp->d_pad0 = 0;
	ddqp->d_pad = 0;

	ddqp->d_blk_hardlimit = cpu_to_be64(dqp->q_blk.hardlimit);
	ddqp->d_blk_softlimit = cpu_to_be64(dqp->q_blk.softlimit);
	ddqp->d_ino_hardlimit = cpu_to_be64(dqp->q_ino.hardlimit);
	ddqp->d_ino_softlimit = cpu_to_be64(dqp->q_ino.softlimit);
	ddqp->d_rtb_hardlimit = cpu_to_be64(dqp->q_rtb.hardlimit);
	ddqp->d_rtb_softlimit = cpu_to_be64(dqp->q_rtb.softlimit);

	ddqp->d_bcount = cpu_to_be64(dqp->q_blk.count);
	ddqp->d_icount = cpu_to_be64(dqp->q_ino.count);
	ddqp->d_rtbcount = cpu_to_be64(dqp->q_rtb.count);

	ddqp->d_bwarns = 0;
	ddqp->d_iwarns = 0;
	ddqp->d_rtbwarns = 0;

	ddqp->d_btimer = xfs_dquot_to_disk_ts(dqp, dqp->q_blk.timer);
	ddqp->d_itimer = xfs_dquot_to_disk_ts(dqp, dqp->q_ino.timer);
	ddqp->d_rtbtimer = xfs_dquot_to_disk_ts(dqp, dqp->q_rtb.timer);
}

 
static int
xfs_qm_dqread(
	struct xfs_mount	*mp,
	xfs_dqid_t		id,
	xfs_dqtype_t		type,
	bool			can_alloc,
	struct xfs_dquot	**dqpp)
{
	struct xfs_dquot	*dqp;
	struct xfs_buf		*bp;
	int			error;

	dqp = xfs_dquot_alloc(mp, id, type);
	trace_xfs_dqread(dqp);

	 
	error = xfs_dquot_disk_read(mp, dqp, &bp);
	if (error == -ENOENT && can_alloc)
		error = xfs_dquot_disk_alloc(dqp, &bp);
	if (error)
		goto err;

	 
	ASSERT(xfs_buf_islocked(bp));
	error = xfs_dquot_from_disk(dqp, bp);
	xfs_buf_relse(bp);
	if (error)
		goto err;

	*dqpp = dqp;
	return error;

err:
	trace_xfs_dqread_fail(dqp);
	xfs_qm_dqdestroy(dqp);
	*dqpp = NULL;
	return error;
}

 
static int
xfs_dq_get_next_id(
	struct xfs_mount	*mp,
	xfs_dqtype_t		type,
	xfs_dqid_t		*id)
{
	struct xfs_inode	*quotip = xfs_quota_inode(mp, type);
	xfs_dqid_t		next_id = *id + 1;  
	uint			lock_flags;
	struct xfs_bmbt_irec	got;
	struct xfs_iext_cursor	cur;
	xfs_fsblock_t		start;
	int			error = 0;

	 
	if (next_id < *id)
		return -ENOENT;

	 
	if (next_id % mp->m_quotainfo->qi_dqperchunk) {
		*id = next_id;
		return 0;
	}

	 
	start = (xfs_fsblock_t)next_id / mp->m_quotainfo->qi_dqperchunk;

	lock_flags = xfs_ilock_data_map_shared(quotip);
	error = xfs_iread_extents(NULL, quotip, XFS_DATA_FORK);
	if (error)
		return error;

	if (xfs_iext_lookup_extent(quotip, &quotip->i_df, start, &cur, &got)) {
		 
		if (got.br_startoff < start)
			got.br_startoff = start;
		*id = got.br_startoff * mp->m_quotainfo->qi_dqperchunk;
	} else {
		error = -ENOENT;
	}

	xfs_iunlock(quotip, lock_flags);

	return error;
}

 
static struct xfs_dquot *
xfs_qm_dqget_cache_lookup(
	struct xfs_mount	*mp,
	struct xfs_quotainfo	*qi,
	struct radix_tree_root	*tree,
	xfs_dqid_t		id)
{
	struct xfs_dquot	*dqp;

restart:
	mutex_lock(&qi->qi_tree_lock);
	dqp = radix_tree_lookup(tree, id);
	if (!dqp) {
		mutex_unlock(&qi->qi_tree_lock);
		XFS_STATS_INC(mp, xs_qm_dqcachemisses);
		return NULL;
	}

	xfs_dqlock(dqp);
	if (dqp->q_flags & XFS_DQFLAG_FREEING) {
		xfs_dqunlock(dqp);
		mutex_unlock(&qi->qi_tree_lock);
		trace_xfs_dqget_freeing(dqp);
		delay(1);
		goto restart;
	}

	dqp->q_nrefs++;
	mutex_unlock(&qi->qi_tree_lock);

	trace_xfs_dqget_hit(dqp);
	XFS_STATS_INC(mp, xs_qm_dqcachehits);
	return dqp;
}

 
static int
xfs_qm_dqget_cache_insert(
	struct xfs_mount	*mp,
	struct xfs_quotainfo	*qi,
	struct radix_tree_root	*tree,
	xfs_dqid_t		id,
	struct xfs_dquot	*dqp)
{
	int			error;

	mutex_lock(&qi->qi_tree_lock);
	error = radix_tree_insert(tree, id, dqp);
	if (unlikely(error)) {
		 
		mutex_unlock(&qi->qi_tree_lock);
		trace_xfs_dqget_dup(dqp);
		return error;
	}

	 
	xfs_dqlock(dqp);
	dqp->q_nrefs = 1;

	qi->qi_dquots++;
	mutex_unlock(&qi->qi_tree_lock);

	return 0;
}

 
static int
xfs_qm_dqget_checks(
	struct xfs_mount	*mp,
	xfs_dqtype_t		type)
{
	switch (type) {
	case XFS_DQTYPE_USER:
		if (!XFS_IS_UQUOTA_ON(mp))
			return -ESRCH;
		return 0;
	case XFS_DQTYPE_GROUP:
		if (!XFS_IS_GQUOTA_ON(mp))
			return -ESRCH;
		return 0;
	case XFS_DQTYPE_PROJ:
		if (!XFS_IS_PQUOTA_ON(mp))
			return -ESRCH;
		return 0;
	default:
		WARN_ON_ONCE(0);
		return -EINVAL;
	}
}

 
int
xfs_qm_dqget(
	struct xfs_mount	*mp,
	xfs_dqid_t		id,
	xfs_dqtype_t		type,
	bool			can_alloc,
	struct xfs_dquot	**O_dqpp)
{
	struct xfs_quotainfo	*qi = mp->m_quotainfo;
	struct radix_tree_root	*tree = xfs_dquot_tree(qi, type);
	struct xfs_dquot	*dqp;
	int			error;

	error = xfs_qm_dqget_checks(mp, type);
	if (error)
		return error;

restart:
	dqp = xfs_qm_dqget_cache_lookup(mp, qi, tree, id);
	if (dqp) {
		*O_dqpp = dqp;
		return 0;
	}

	error = xfs_qm_dqread(mp, id, type, can_alloc, &dqp);
	if (error)
		return error;

	error = xfs_qm_dqget_cache_insert(mp, qi, tree, id, dqp);
	if (error) {
		 
		xfs_qm_dqdestroy(dqp);
		XFS_STATS_INC(mp, xs_qm_dquot_dups);
		goto restart;
	}

	trace_xfs_dqget_miss(dqp);
	*O_dqpp = dqp;
	return 0;
}

 
int
xfs_qm_dqget_uncached(
	struct xfs_mount	*mp,
	xfs_dqid_t		id,
	xfs_dqtype_t		type,
	struct xfs_dquot	**dqpp)
{
	int			error;

	error = xfs_qm_dqget_checks(mp, type);
	if (error)
		return error;

	return xfs_qm_dqread(mp, id, type, 0, dqpp);
}

 
xfs_dqid_t
xfs_qm_id_for_quotatype(
	struct xfs_inode	*ip,
	xfs_dqtype_t		type)
{
	switch (type) {
	case XFS_DQTYPE_USER:
		return i_uid_read(VFS_I(ip));
	case XFS_DQTYPE_GROUP:
		return i_gid_read(VFS_I(ip));
	case XFS_DQTYPE_PROJ:
		return ip->i_projid;
	}
	ASSERT(0);
	return 0;
}

 
int
xfs_qm_dqget_inode(
	struct xfs_inode	*ip,
	xfs_dqtype_t		type,
	bool			can_alloc,
	struct xfs_dquot	**O_dqpp)
{
	struct xfs_mount	*mp = ip->i_mount;
	struct xfs_quotainfo	*qi = mp->m_quotainfo;
	struct radix_tree_root	*tree = xfs_dquot_tree(qi, type);
	struct xfs_dquot	*dqp;
	xfs_dqid_t		id;
	int			error;

	error = xfs_qm_dqget_checks(mp, type);
	if (error)
		return error;

	ASSERT(xfs_isilocked(ip, XFS_ILOCK_EXCL));
	ASSERT(xfs_inode_dquot(ip, type) == NULL);

	id = xfs_qm_id_for_quotatype(ip, type);

restart:
	dqp = xfs_qm_dqget_cache_lookup(mp, qi, tree, id);
	if (dqp) {
		*O_dqpp = dqp;
		return 0;
	}

	 
	xfs_iunlock(ip, XFS_ILOCK_EXCL);
	error = xfs_qm_dqread(mp, id, type, can_alloc, &dqp);
	xfs_ilock(ip, XFS_ILOCK_EXCL);
	if (error)
		return error;

	 
	if (xfs_this_quota_on(mp, type)) {
		struct xfs_dquot	*dqp1;

		dqp1 = xfs_inode_dquot(ip, type);
		if (dqp1) {
			xfs_qm_dqdestroy(dqp);
			dqp = dqp1;
			xfs_dqlock(dqp);
			goto dqret;
		}
	} else {
		 
		xfs_qm_dqdestroy(dqp);
		return -ESRCH;
	}

	error = xfs_qm_dqget_cache_insert(mp, qi, tree, id, dqp);
	if (error) {
		 
		xfs_qm_dqdestroy(dqp);
		XFS_STATS_INC(mp, xs_qm_dquot_dups);
		goto restart;
	}

dqret:
	ASSERT(xfs_isilocked(ip, XFS_ILOCK_EXCL));
	trace_xfs_dqget_miss(dqp);
	*O_dqpp = dqp;
	return 0;
}

 
int
xfs_qm_dqget_next(
	struct xfs_mount	*mp,
	xfs_dqid_t		id,
	xfs_dqtype_t		type,
	struct xfs_dquot	**dqpp)
{
	struct xfs_dquot	*dqp;
	int			error = 0;

	*dqpp = NULL;
	for (; !error; error = xfs_dq_get_next_id(mp, type, &id)) {
		error = xfs_qm_dqget(mp, id, type, false, &dqp);
		if (error == -ENOENT)
			continue;
		else if (error != 0)
			break;

		if (!XFS_IS_DQUOT_UNINITIALIZED(dqp)) {
			*dqpp = dqp;
			return 0;
		}

		xfs_qm_dqput(dqp);
	}

	return error;
}

 
void
xfs_qm_dqput(
	struct xfs_dquot	*dqp)
{
	ASSERT(dqp->q_nrefs > 0);
	ASSERT(XFS_DQ_IS_LOCKED(dqp));

	trace_xfs_dqput(dqp);

	if (--dqp->q_nrefs == 0) {
		struct xfs_quotainfo	*qi = dqp->q_mount->m_quotainfo;
		trace_xfs_dqput_free(dqp);

		if (list_lru_add(&qi->qi_lru, &dqp->q_lru))
			XFS_STATS_INC(dqp->q_mount, xs_qm_dquot_unused);
	}
	xfs_dqunlock(dqp);
}

 
void
xfs_qm_dqrele(
	struct xfs_dquot	*dqp)
{
	if (!dqp)
		return;

	trace_xfs_dqrele(dqp);

	xfs_dqlock(dqp);
	 
	xfs_qm_dqput(dqp);
}

 
static void
xfs_qm_dqflush_done(
	struct xfs_log_item	*lip)
{
	struct xfs_dq_logitem	*qip = (struct xfs_dq_logitem *)lip;
	struct xfs_dquot	*dqp = qip->qli_dquot;
	struct xfs_ail		*ailp = lip->li_ailp;
	xfs_lsn_t		tail_lsn;

	 
	if (test_bit(XFS_LI_IN_AIL, &lip->li_flags) &&
	    ((lip->li_lsn == qip->qli_flush_lsn) ||
	     test_bit(XFS_LI_FAILED, &lip->li_flags))) {

		spin_lock(&ailp->ail_lock);
		xfs_clear_li_failed(lip);
		if (lip->li_lsn == qip->qli_flush_lsn) {
			 
			tail_lsn = xfs_ail_delete_one(ailp, lip);
			xfs_ail_update_finish(ailp, tail_lsn);
		} else {
			spin_unlock(&ailp->ail_lock);
		}
	}

	 
	xfs_dqfunlock(dqp);
}

void
xfs_buf_dquot_iodone(
	struct xfs_buf		*bp)
{
	struct xfs_log_item	*lip, *n;

	list_for_each_entry_safe(lip, n, &bp->b_li_list, li_bio_list) {
		list_del_init(&lip->li_bio_list);
		xfs_qm_dqflush_done(lip);
	}
}

void
xfs_buf_dquot_io_fail(
	struct xfs_buf		*bp)
{
	struct xfs_log_item	*lip;

	spin_lock(&bp->b_mount->m_ail->ail_lock);
	list_for_each_entry(lip, &bp->b_li_list, li_bio_list)
		xfs_set_li_failed(lip, bp);
	spin_unlock(&bp->b_mount->m_ail->ail_lock);
}

 
static xfs_failaddr_t
xfs_qm_dqflush_check(
	struct xfs_dquot	*dqp)
{
	xfs_dqtype_t		type = xfs_dquot_type(dqp);

	if (type != XFS_DQTYPE_USER &&
	    type != XFS_DQTYPE_GROUP &&
	    type != XFS_DQTYPE_PROJ)
		return __this_address;

	if (dqp->q_id == 0)
		return NULL;

	if (dqp->q_blk.softlimit && dqp->q_blk.count > dqp->q_blk.softlimit &&
	    !dqp->q_blk.timer)
		return __this_address;

	if (dqp->q_ino.softlimit && dqp->q_ino.count > dqp->q_ino.softlimit &&
	    !dqp->q_ino.timer)
		return __this_address;

	if (dqp->q_rtb.softlimit && dqp->q_rtb.count > dqp->q_rtb.softlimit &&
	    !dqp->q_rtb.timer)
		return __this_address;

	 
	if (dqp->q_type & XFS_DQTYPE_BIGTIME) {
		if (!xfs_has_bigtime(dqp->q_mount))
			return __this_address;
		if (dqp->q_id == 0)
			return __this_address;
	}

	return NULL;
}

 
int
xfs_qm_dqflush(
	struct xfs_dquot	*dqp,
	struct xfs_buf		**bpp)
{
	struct xfs_mount	*mp = dqp->q_mount;
	struct xfs_log_item	*lip = &dqp->q_logitem.qli_item;
	struct xfs_buf		*bp;
	struct xfs_dqblk	*dqblk;
	xfs_failaddr_t		fa;
	int			error;

	ASSERT(XFS_DQ_IS_LOCKED(dqp));
	ASSERT(!completion_done(&dqp->q_flush));

	trace_xfs_dqflush(dqp);

	*bpp = NULL;

	xfs_qm_dqunpin_wait(dqp);

	 
	error = xfs_trans_read_buf(mp, NULL, mp->m_ddev_targp, dqp->q_blkno,
				   mp->m_quotainfo->qi_dqchunklen, XBF_TRYLOCK,
				   &bp, &xfs_dquot_buf_ops);
	if (error == -EAGAIN)
		goto out_unlock;
	if (error)
		goto out_abort;

	fa = xfs_qm_dqflush_check(dqp);
	if (fa) {
		xfs_alert(mp, "corrupt dquot ID 0x%x in memory at %pS",
				dqp->q_id, fa);
		xfs_buf_relse(bp);
		error = -EFSCORRUPTED;
		goto out_abort;
	}

	 
	dqblk = bp->b_addr + dqp->q_bufoffset;
	xfs_dquot_to_disk(&dqblk->dd_diskdq, dqp);

	 
	dqp->q_flags &= ~XFS_DQFLAG_DIRTY;

	xfs_trans_ail_copy_lsn(mp->m_ail, &dqp->q_logitem.qli_flush_lsn,
					&dqp->q_logitem.qli_item.li_lsn);

	 
	if (xfs_has_crc(mp)) {
		dqblk->dd_lsn = cpu_to_be64(dqp->q_logitem.qli_item.li_lsn);
		xfs_update_cksum((char *)dqblk, sizeof(struct xfs_dqblk),
				 XFS_DQUOT_CRC_OFF);
	}

	 
	bp->b_flags |= _XBF_DQUOTS;
	list_add_tail(&dqp->q_logitem.qli_item.li_bio_list, &bp->b_li_list);

	 
	if (xfs_buf_ispinned(bp)) {
		trace_xfs_dqflush_force(dqp);
		xfs_log_force(mp, 0);
	}

	trace_xfs_dqflush_done(dqp);
	*bpp = bp;
	return 0;

out_abort:
	dqp->q_flags &= ~XFS_DQFLAG_DIRTY;
	xfs_trans_ail_delete(lip, 0);
	xfs_force_shutdown(mp, SHUTDOWN_CORRUPT_INCORE);
out_unlock:
	xfs_dqfunlock(dqp);
	return error;
}

 
void
xfs_dqlock2(
	struct xfs_dquot	*d1,
	struct xfs_dquot	*d2)
{
	if (d1 && d2) {
		ASSERT(d1 != d2);
		if (d1->q_id > d2->q_id) {
			mutex_lock(&d2->q_qlock);
			mutex_lock_nested(&d1->q_qlock, XFS_QLOCK_NESTED);
		} else {
			mutex_lock(&d1->q_qlock);
			mutex_lock_nested(&d2->q_qlock, XFS_QLOCK_NESTED);
		}
	} else if (d1) {
		mutex_lock(&d1->q_qlock);
	} else if (d2) {
		mutex_lock(&d2->q_qlock);
	}
}

int __init
xfs_qm_init(void)
{
	xfs_dquot_cache = kmem_cache_create("xfs_dquot",
					  sizeof(struct xfs_dquot),
					  0, 0, NULL);
	if (!xfs_dquot_cache)
		goto out;

	xfs_dqtrx_cache = kmem_cache_create("xfs_dqtrx",
					     sizeof(struct xfs_dquot_acct),
					     0, 0, NULL);
	if (!xfs_dqtrx_cache)
		goto out_free_dquot_cache;

	return 0;

out_free_dquot_cache:
	kmem_cache_destroy(xfs_dquot_cache);
out:
	return -ENOMEM;
}

void
xfs_qm_exit(void)
{
	kmem_cache_destroy(xfs_dqtrx_cache);
	kmem_cache_destroy(xfs_dquot_cache);
}

 
int
xfs_qm_dqiterate(
	struct xfs_mount	*mp,
	xfs_dqtype_t		type,
	xfs_qm_dqiterate_fn	iter_fn,
	void			*priv)
{
	struct xfs_dquot	*dq;
	xfs_dqid_t		id = 0;
	int			error;

	do {
		error = xfs_qm_dqget_next(mp, id, type, &dq);
		if (error == -ENOENT)
			return 0;
		if (error)
			return error;

		error = iter_fn(dq, type, priv);
		id = dq->q_id + 1;
		xfs_qm_dqput(dq);
	} while (error == 0 && id != 0);

	return error;
}
