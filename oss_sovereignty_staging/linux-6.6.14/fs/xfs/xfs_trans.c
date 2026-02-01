
 
#include "xfs.h"
#include "xfs_fs.h"
#include "xfs_shared.h"
#include "xfs_format.h"
#include "xfs_log_format.h"
#include "xfs_trans_resv.h"
#include "xfs_mount.h"
#include "xfs_extent_busy.h"
#include "xfs_quota.h"
#include "xfs_trans.h"
#include "xfs_trans_priv.h"
#include "xfs_log.h"
#include "xfs_log_priv.h"
#include "xfs_trace.h"
#include "xfs_error.h"
#include "xfs_defer.h"
#include "xfs_inode.h"
#include "xfs_dquot_item.h"
#include "xfs_dquot.h"
#include "xfs_icache.h"

struct kmem_cache	*xfs_trans_cache;

#if defined(CONFIG_TRACEPOINTS)
static void
xfs_trans_trace_reservations(
	struct xfs_mount	*mp)
{
	struct xfs_trans_res	*res;
	struct xfs_trans_res	*end_res;
	int			i;

	res = (struct xfs_trans_res *)M_RES(mp);
	end_res = (struct xfs_trans_res *)(M_RES(mp) + 1);
	for (i = 0; res < end_res; i++, res++)
		trace_xfs_trans_resv_calc(mp, i, res);
}
#else
# define xfs_trans_trace_reservations(mp)
#endif

 
void
xfs_trans_init(
	struct xfs_mount	*mp)
{
	xfs_trans_resv_calc(mp, M_RES(mp));
	xfs_trans_trace_reservations(mp);
}

 
STATIC void
xfs_trans_free(
	struct xfs_trans	*tp)
{
	xfs_extent_busy_sort(&tp->t_busy);
	xfs_extent_busy_clear(tp->t_mountp, &tp->t_busy, false);

	trace_xfs_trans_free(tp, _RET_IP_);
	xfs_trans_clear_context(tp);
	if (!(tp->t_flags & XFS_TRANS_NO_WRITECOUNT))
		sb_end_intwrite(tp->t_mountp->m_super);
	xfs_trans_free_dqinfo(tp);
	kmem_cache_free(xfs_trans_cache, tp);
}

 
STATIC struct xfs_trans *
xfs_trans_dup(
	struct xfs_trans	*tp)
{
	struct xfs_trans	*ntp;

	trace_xfs_trans_dup(tp, _RET_IP_);

	ntp = kmem_cache_zalloc(xfs_trans_cache, GFP_KERNEL | __GFP_NOFAIL);

	 
	ntp->t_magic = XFS_TRANS_HEADER_MAGIC;
	ntp->t_mountp = tp->t_mountp;
	INIT_LIST_HEAD(&ntp->t_items);
	INIT_LIST_HEAD(&ntp->t_busy);
	INIT_LIST_HEAD(&ntp->t_dfops);
	ntp->t_highest_agno = NULLAGNUMBER;

	ASSERT(tp->t_flags & XFS_TRANS_PERM_LOG_RES);
	ASSERT(tp->t_ticket != NULL);

	ntp->t_flags = XFS_TRANS_PERM_LOG_RES |
		       (tp->t_flags & XFS_TRANS_RESERVE) |
		       (tp->t_flags & XFS_TRANS_NO_WRITECOUNT) |
		       (tp->t_flags & XFS_TRANS_RES_FDBLKS);
	 
	tp->t_flags |= XFS_TRANS_NO_WRITECOUNT;
	ntp->t_ticket = xfs_log_ticket_get(tp->t_ticket);

	ASSERT(tp->t_blk_res >= tp->t_blk_res_used);
	ntp->t_blk_res = tp->t_blk_res - tp->t_blk_res_used;
	tp->t_blk_res = tp->t_blk_res_used;

	ntp->t_rtx_res = tp->t_rtx_res - tp->t_rtx_res_used;
	tp->t_rtx_res = tp->t_rtx_res_used;

	xfs_trans_switch_context(tp, ntp);

	 
	xfs_defer_move(ntp, tp);

	xfs_trans_dup_dqinfo(tp, ntp);
	return ntp;
}

 
static int
xfs_trans_reserve(
	struct xfs_trans	*tp,
	struct xfs_trans_res	*resp,
	uint			blocks,
	uint			rtextents)
{
	struct xfs_mount	*mp = tp->t_mountp;
	int			error = 0;
	bool			rsvd = (tp->t_flags & XFS_TRANS_RESERVE) != 0;

	 
	if (blocks > 0) {
		error = xfs_mod_fdblocks(mp, -((int64_t)blocks), rsvd);
		if (error != 0)
			return -ENOSPC;
		tp->t_blk_res += blocks;
	}

	 
	if (resp->tr_logres > 0) {
		bool	permanent = false;

		ASSERT(tp->t_log_res == 0 ||
		       tp->t_log_res == resp->tr_logres);
		ASSERT(tp->t_log_count == 0 ||
		       tp->t_log_count == resp->tr_logcount);

		if (resp->tr_logflags & XFS_TRANS_PERM_LOG_RES) {
			tp->t_flags |= XFS_TRANS_PERM_LOG_RES;
			permanent = true;
		} else {
			ASSERT(tp->t_ticket == NULL);
			ASSERT(!(tp->t_flags & XFS_TRANS_PERM_LOG_RES));
		}

		if (tp->t_ticket != NULL) {
			ASSERT(resp->tr_logflags & XFS_TRANS_PERM_LOG_RES);
			error = xfs_log_regrant(mp, tp->t_ticket);
		} else {
			error = xfs_log_reserve(mp, resp->tr_logres,
						resp->tr_logcount,
						&tp->t_ticket, permanent);
		}

		if (error)
			goto undo_blocks;

		tp->t_log_res = resp->tr_logres;
		tp->t_log_count = resp->tr_logcount;
	}

	 
	if (rtextents > 0) {
		error = xfs_mod_frextents(mp, -((int64_t)rtextents));
		if (error) {
			error = -ENOSPC;
			goto undo_log;
		}
		tp->t_rtx_res += rtextents;
	}

	return 0;

	 
undo_log:
	if (resp->tr_logres > 0) {
		xfs_log_ticket_ungrant(mp->m_log, tp->t_ticket);
		tp->t_ticket = NULL;
		tp->t_log_res = 0;
		tp->t_flags &= ~XFS_TRANS_PERM_LOG_RES;
	}

undo_blocks:
	if (blocks > 0) {
		xfs_mod_fdblocks(mp, (int64_t)blocks, rsvd);
		tp->t_blk_res = 0;
	}
	return error;
}

int
xfs_trans_alloc(
	struct xfs_mount	*mp,
	struct xfs_trans_res	*resp,
	uint			blocks,
	uint			rtextents,
	uint			flags,
	struct xfs_trans	**tpp)
{
	struct xfs_trans	*tp;
	bool			want_retry = true;
	int			error;

	 
retry:
	tp = kmem_cache_zalloc(xfs_trans_cache, GFP_KERNEL | __GFP_NOFAIL);
	if (!(flags & XFS_TRANS_NO_WRITECOUNT))
		sb_start_intwrite(mp->m_super);
	xfs_trans_set_context(tp);

	 
	WARN_ON(resp->tr_logres > 0 &&
		mp->m_super->s_writers.frozen == SB_FREEZE_COMPLETE);
	ASSERT(!(flags & XFS_TRANS_RES_FDBLKS) ||
	       xfs_has_lazysbcount(mp));

	tp->t_magic = XFS_TRANS_HEADER_MAGIC;
	tp->t_flags = flags;
	tp->t_mountp = mp;
	INIT_LIST_HEAD(&tp->t_items);
	INIT_LIST_HEAD(&tp->t_busy);
	INIT_LIST_HEAD(&tp->t_dfops);
	tp->t_highest_agno = NULLAGNUMBER;

	error = xfs_trans_reserve(tp, resp, blocks, rtextents);
	if (error == -ENOSPC && want_retry) {
		xfs_trans_cancel(tp);

		 
		error = xfs_blockgc_flush_all(mp);
		if (error)
			return error;
		want_retry = false;
		goto retry;
	}
	if (error) {
		xfs_trans_cancel(tp);
		return error;
	}

	trace_xfs_trans_alloc(tp, _RET_IP_);

	*tpp = tp;
	return 0;
}

 
int
xfs_trans_alloc_empty(
	struct xfs_mount		*mp,
	struct xfs_trans		**tpp)
{
	struct xfs_trans_res		resv = {0};

	return xfs_trans_alloc(mp, &resv, 0, 0, XFS_TRANS_NO_WRITECOUNT, tpp);
}

 
void
xfs_trans_mod_sb(
	xfs_trans_t	*tp,
	uint		field,
	int64_t		delta)
{
	uint32_t	flags = (XFS_TRANS_DIRTY|XFS_TRANS_SB_DIRTY);
	xfs_mount_t	*mp = tp->t_mountp;

	switch (field) {
	case XFS_TRANS_SB_ICOUNT:
		tp->t_icount_delta += delta;
		if (xfs_has_lazysbcount(mp))
			flags &= ~XFS_TRANS_SB_DIRTY;
		break;
	case XFS_TRANS_SB_IFREE:
		tp->t_ifree_delta += delta;
		if (xfs_has_lazysbcount(mp))
			flags &= ~XFS_TRANS_SB_DIRTY;
		break;
	case XFS_TRANS_SB_FDBLOCKS:
		 
		if (delta < 0) {
			tp->t_blk_res_used += (uint)-delta;
			if (tp->t_blk_res_used > tp->t_blk_res)
				xfs_force_shutdown(mp, SHUTDOWN_CORRUPT_INCORE);
		} else if (delta > 0 && (tp->t_flags & XFS_TRANS_RES_FDBLKS)) {
			int64_t	blkres_delta;

			 
			blkres_delta = min_t(int64_t, delta,
					     UINT_MAX - tp->t_blk_res);
			tp->t_blk_res += blkres_delta;
			delta -= blkres_delta;
		}
		tp->t_fdblocks_delta += delta;
		if (xfs_has_lazysbcount(mp))
			flags &= ~XFS_TRANS_SB_DIRTY;
		break;
	case XFS_TRANS_SB_RES_FDBLOCKS:
		 
		tp->t_res_fdblocks_delta += delta;
		if (xfs_has_lazysbcount(mp))
			flags &= ~XFS_TRANS_SB_DIRTY;
		break;
	case XFS_TRANS_SB_FREXTENTS:
		 
		if (delta < 0) {
			tp->t_rtx_res_used += (uint)-delta;
			ASSERT(tp->t_rtx_res_used <= tp->t_rtx_res);
		}
		tp->t_frextents_delta += delta;
		break;
	case XFS_TRANS_SB_RES_FREXTENTS:
		 
		ASSERT(delta < 0);
		tp->t_res_frextents_delta += delta;
		break;
	case XFS_TRANS_SB_DBLOCKS:
		tp->t_dblocks_delta += delta;
		break;
	case XFS_TRANS_SB_AGCOUNT:
		ASSERT(delta > 0);
		tp->t_agcount_delta += delta;
		break;
	case XFS_TRANS_SB_IMAXPCT:
		tp->t_imaxpct_delta += delta;
		break;
	case XFS_TRANS_SB_REXTSIZE:
		tp->t_rextsize_delta += delta;
		break;
	case XFS_TRANS_SB_RBMBLOCKS:
		tp->t_rbmblocks_delta += delta;
		break;
	case XFS_TRANS_SB_RBLOCKS:
		tp->t_rblocks_delta += delta;
		break;
	case XFS_TRANS_SB_REXTENTS:
		tp->t_rextents_delta += delta;
		break;
	case XFS_TRANS_SB_REXTSLOG:
		tp->t_rextslog_delta += delta;
		break;
	default:
		ASSERT(0);
		return;
	}

	tp->t_flags |= flags;
}

 
STATIC void
xfs_trans_apply_sb_deltas(
	xfs_trans_t	*tp)
{
	struct xfs_dsb	*sbp;
	struct xfs_buf	*bp;
	int		whole = 0;

	bp = xfs_trans_getsb(tp);
	sbp = bp->b_addr;

	 
	if (!xfs_has_lazysbcount((tp->t_mountp))) {
		if (tp->t_icount_delta)
			be64_add_cpu(&sbp->sb_icount, tp->t_icount_delta);
		if (tp->t_ifree_delta)
			be64_add_cpu(&sbp->sb_ifree, tp->t_ifree_delta);
		if (tp->t_fdblocks_delta)
			be64_add_cpu(&sbp->sb_fdblocks, tp->t_fdblocks_delta);
		if (tp->t_res_fdblocks_delta)
			be64_add_cpu(&sbp->sb_fdblocks, tp->t_res_fdblocks_delta);
	}

	 
	if (tp->t_frextents_delta || tp->t_res_frextents_delta) {
		struct xfs_mount	*mp = tp->t_mountp;
		int64_t			rtxdelta;

		rtxdelta = tp->t_frextents_delta + tp->t_res_frextents_delta;

		spin_lock(&mp->m_sb_lock);
		be64_add_cpu(&sbp->sb_frextents, rtxdelta);
		mp->m_sb.sb_frextents += rtxdelta;
		spin_unlock(&mp->m_sb_lock);
	}

	if (tp->t_dblocks_delta) {
		be64_add_cpu(&sbp->sb_dblocks, tp->t_dblocks_delta);
		whole = 1;
	}
	if (tp->t_agcount_delta) {
		be32_add_cpu(&sbp->sb_agcount, tp->t_agcount_delta);
		whole = 1;
	}
	if (tp->t_imaxpct_delta) {
		sbp->sb_imax_pct += tp->t_imaxpct_delta;
		whole = 1;
	}
	if (tp->t_rextsize_delta) {
		be32_add_cpu(&sbp->sb_rextsize, tp->t_rextsize_delta);
		whole = 1;
	}
	if (tp->t_rbmblocks_delta) {
		be32_add_cpu(&sbp->sb_rbmblocks, tp->t_rbmblocks_delta);
		whole = 1;
	}
	if (tp->t_rblocks_delta) {
		be64_add_cpu(&sbp->sb_rblocks, tp->t_rblocks_delta);
		whole = 1;
	}
	if (tp->t_rextents_delta) {
		be64_add_cpu(&sbp->sb_rextents, tp->t_rextents_delta);
		whole = 1;
	}
	if (tp->t_rextslog_delta) {
		sbp->sb_rextslog += tp->t_rextslog_delta;
		whole = 1;
	}

	xfs_trans_buf_set_type(tp, bp, XFS_BLFT_SB_BUF);
	if (whole)
		 
		xfs_trans_log_buf(tp, bp, 0, sizeof(struct xfs_dsb) - 1);
	else
		 
		xfs_trans_log_buf(tp, bp, offsetof(struct xfs_dsb, sb_icount),
				  offsetof(struct xfs_dsb, sb_frextents) +
				  sizeof(sbp->sb_frextents) - 1);
}

 
#define XFS_ICOUNT_BATCH	128

void
xfs_trans_unreserve_and_mod_sb(
	struct xfs_trans	*tp)
{
	struct xfs_mount	*mp = tp->t_mountp;
	bool			rsvd = (tp->t_flags & XFS_TRANS_RESERVE) != 0;
	int64_t			blkdelta = 0;
	int64_t			rtxdelta = 0;
	int64_t			idelta = 0;
	int64_t			ifreedelta = 0;
	int			error;

	 
	if (tp->t_blk_res > 0)
		blkdelta = tp->t_blk_res;
	if ((tp->t_fdblocks_delta != 0) &&
	    (xfs_has_lazysbcount(mp) ||
	     (tp->t_flags & XFS_TRANS_SB_DIRTY)))
	        blkdelta += tp->t_fdblocks_delta;

	if (tp->t_rtx_res > 0)
		rtxdelta = tp->t_rtx_res;
	if ((tp->t_frextents_delta != 0) &&
	    (tp->t_flags & XFS_TRANS_SB_DIRTY))
		rtxdelta += tp->t_frextents_delta;

	if (xfs_has_lazysbcount(mp) ||
	     (tp->t_flags & XFS_TRANS_SB_DIRTY)) {
		idelta = tp->t_icount_delta;
		ifreedelta = tp->t_ifree_delta;
	}

	 
	if (blkdelta) {
		error = xfs_mod_fdblocks(mp, blkdelta, rsvd);
		ASSERT(!error);
	}

	if (idelta)
		percpu_counter_add_batch(&mp->m_icount, idelta,
					 XFS_ICOUNT_BATCH);

	if (ifreedelta)
		percpu_counter_add(&mp->m_ifree, ifreedelta);

	if (rtxdelta) {
		error = xfs_mod_frextents(mp, rtxdelta);
		ASSERT(!error);
	}

	if (!(tp->t_flags & XFS_TRANS_SB_DIRTY))
		return;

	 
	spin_lock(&mp->m_sb_lock);
	mp->m_sb.sb_fdblocks += tp->t_fdblocks_delta + tp->t_res_fdblocks_delta;
	mp->m_sb.sb_icount += idelta;
	mp->m_sb.sb_ifree += ifreedelta;
	 
	mp->m_sb.sb_dblocks += tp->t_dblocks_delta;
	mp->m_sb.sb_agcount += tp->t_agcount_delta;
	mp->m_sb.sb_imax_pct += tp->t_imaxpct_delta;
	mp->m_sb.sb_rextsize += tp->t_rextsize_delta;
	mp->m_sb.sb_rbmblocks += tp->t_rbmblocks_delta;
	mp->m_sb.sb_rblocks += tp->t_rblocks_delta;
	mp->m_sb.sb_rextents += tp->t_rextents_delta;
	mp->m_sb.sb_rextslog += tp->t_rextslog_delta;
	spin_unlock(&mp->m_sb_lock);

	 
	ASSERT(mp->m_sb.sb_imax_pct >= 0);
	ASSERT(mp->m_sb.sb_rextslog >= 0);
	return;
}

 
void
xfs_trans_add_item(
	struct xfs_trans	*tp,
	struct xfs_log_item	*lip)
{
	ASSERT(lip->li_log == tp->t_mountp->m_log);
	ASSERT(lip->li_ailp == tp->t_mountp->m_ail);
	ASSERT(list_empty(&lip->li_trans));
	ASSERT(!test_bit(XFS_LI_DIRTY, &lip->li_flags));

	list_add_tail(&lip->li_trans, &tp->t_items);
	trace_xfs_trans_add_item(tp, _RET_IP_);
}

 
void
xfs_trans_del_item(
	struct xfs_log_item	*lip)
{
	clear_bit(XFS_LI_DIRTY, &lip->li_flags);
	list_del_init(&lip->li_trans);
}

 
static void
xfs_trans_free_items(
	struct xfs_trans	*tp,
	bool			abort)
{
	struct xfs_log_item	*lip, *next;

	trace_xfs_trans_free_items(tp, _RET_IP_);

	list_for_each_entry_safe(lip, next, &tp->t_items, li_trans) {
		xfs_trans_del_item(lip);
		if (abort)
			set_bit(XFS_LI_ABORTED, &lip->li_flags);
		if (lip->li_ops->iop_release)
			lip->li_ops->iop_release(lip);
	}
}

static inline void
xfs_log_item_batch_insert(
	struct xfs_ail		*ailp,
	struct xfs_ail_cursor	*cur,
	struct xfs_log_item	**log_items,
	int			nr_items,
	xfs_lsn_t		commit_lsn)
{
	int	i;

	spin_lock(&ailp->ail_lock);
	 
	xfs_trans_ail_update_bulk(ailp, cur, log_items, nr_items, commit_lsn);

	for (i = 0; i < nr_items; i++) {
		struct xfs_log_item *lip = log_items[i];

		if (lip->li_ops->iop_unpin)
			lip->li_ops->iop_unpin(lip, 0);
	}
}

 
void
xfs_trans_committed_bulk(
	struct xfs_ail		*ailp,
	struct list_head	*lv_chain,
	xfs_lsn_t		commit_lsn,
	bool			aborted)
{
#define LOG_ITEM_BATCH_SIZE	32
	struct xfs_log_item	*log_items[LOG_ITEM_BATCH_SIZE];
	struct xfs_log_vec	*lv;
	struct xfs_ail_cursor	cur;
	int			i = 0;

	spin_lock(&ailp->ail_lock);
	xfs_trans_ail_cursor_last(ailp, &cur, commit_lsn);
	spin_unlock(&ailp->ail_lock);

	 
	list_for_each_entry(lv, lv_chain, lv_list) {
		struct xfs_log_item	*lip = lv->lv_item;
		xfs_lsn_t		item_lsn;

		if (aborted)
			set_bit(XFS_LI_ABORTED, &lip->li_flags);

		if (lip->li_ops->flags & XFS_ITEM_RELEASE_WHEN_COMMITTED) {
			lip->li_ops->iop_release(lip);
			continue;
		}

		if (lip->li_ops->iop_committed)
			item_lsn = lip->li_ops->iop_committed(lip, commit_lsn);
		else
			item_lsn = commit_lsn;

		 
		if (XFS_LSN_CMP(item_lsn, (xfs_lsn_t)-1) == 0)
			continue;

		 
		if (aborted) {
			ASSERT(xlog_is_shutdown(ailp->ail_log));
			if (lip->li_ops->iop_unpin)
				lip->li_ops->iop_unpin(lip, 1);
			continue;
		}

		if (item_lsn != commit_lsn) {

			 
			spin_lock(&ailp->ail_lock);
			if (XFS_LSN_CMP(item_lsn, lip->li_lsn) > 0)
				xfs_trans_ail_update(ailp, lip, item_lsn);
			else
				spin_unlock(&ailp->ail_lock);
			if (lip->li_ops->iop_unpin)
				lip->li_ops->iop_unpin(lip, 0);
			continue;
		}

		 
		log_items[i++] = lv->lv_item;
		if (i >= LOG_ITEM_BATCH_SIZE) {
			xfs_log_item_batch_insert(ailp, &cur, log_items,
					LOG_ITEM_BATCH_SIZE, commit_lsn);
			i = 0;
		}
	}

	 
	if (i)
		xfs_log_item_batch_insert(ailp, &cur, log_items, i, commit_lsn);

	spin_lock(&ailp->ail_lock);
	xfs_trans_ail_cursor_done(&cur);
	spin_unlock(&ailp->ail_lock);
}

 
static int
xfs_trans_precommit_sort(
	void			*unused_arg,
	const struct list_head	*a,
	const struct list_head	*b)
{
	struct xfs_log_item	*lia = container_of(a,
					struct xfs_log_item, li_trans);
	struct xfs_log_item	*lib = container_of(b,
					struct xfs_log_item, li_trans);
	int64_t			diff;

	 
	if (!lia->li_ops->iop_sort && !lib->li_ops->iop_sort)
		return 0;
	if (!lia->li_ops->iop_sort)
		return 1;
	if (!lib->li_ops->iop_sort)
		return -1;

	diff = lia->li_ops->iop_sort(lia) - lib->li_ops->iop_sort(lib);
	if (diff < 0)
		return -1;
	if (diff > 0)
		return 1;
	return 0;
}

 
static int
xfs_trans_run_precommits(
	struct xfs_trans	*tp)
{
	struct xfs_mount	*mp = tp->t_mountp;
	struct xfs_log_item	*lip, *n;
	int			error = 0;

	 
	list_sort(NULL, &tp->t_items, xfs_trans_precommit_sort);

	 
	list_for_each_entry_safe(lip, n, &tp->t_items, li_trans) {
		if (!test_bit(XFS_LI_DIRTY, &lip->li_flags))
			continue;
		if (lip->li_ops->iop_precommit) {
			error = lip->li_ops->iop_precommit(tp, lip);
			if (error)
				break;
		}
	}
	if (error)
		xfs_force_shutdown(mp, SHUTDOWN_CORRUPT_INCORE);
	return error;
}

 
static int
__xfs_trans_commit(
	struct xfs_trans	*tp,
	bool			regrant)
{
	struct xfs_mount	*mp = tp->t_mountp;
	struct xlog		*log = mp->m_log;
	xfs_csn_t		commit_seq = 0;
	int			error = 0;
	int			sync = tp->t_flags & XFS_TRANS_SYNC;

	trace_xfs_trans_commit(tp, _RET_IP_);

	error = xfs_trans_run_precommits(tp);
	if (error) {
		if (tp->t_flags & XFS_TRANS_PERM_LOG_RES)
			xfs_defer_cancel(tp);
		goto out_unreserve;
	}

	 
	WARN_ON_ONCE(!list_empty(&tp->t_dfops) &&
		     !(tp->t_flags & XFS_TRANS_PERM_LOG_RES));
	if (!regrant && (tp->t_flags & XFS_TRANS_PERM_LOG_RES)) {
		error = xfs_defer_finish_noroll(&tp);
		if (error)
			goto out_unreserve;

		 
		error = xfs_trans_run_precommits(tp);
		if (error)
			goto out_unreserve;
	}

	 
	if (!(tp->t_flags & XFS_TRANS_DIRTY))
		goto out_unreserve;

	 
	if (xlog_is_shutdown(log)) {
		error = -EIO;
		goto out_unreserve;
	}

	ASSERT(tp->t_ticket != NULL);

	 
	if (tp->t_flags & XFS_TRANS_SB_DIRTY)
		xfs_trans_apply_sb_deltas(tp);
	xfs_trans_apply_dquot_deltas(tp);

	xlog_cil_commit(log, tp, &commit_seq, regrant);

	xfs_trans_free(tp);

	 
	if (sync) {
		error = xfs_log_force_seq(mp, commit_seq, XFS_LOG_SYNC, NULL);
		XFS_STATS_INC(mp, xs_trans_sync);
	} else {
		XFS_STATS_INC(mp, xs_trans_async);
	}

	return error;

out_unreserve:
	xfs_trans_unreserve_and_mod_sb(tp);

	 
	xfs_trans_unreserve_and_mod_dquots(tp);
	if (tp->t_ticket) {
		if (regrant && !xlog_is_shutdown(log))
			xfs_log_ticket_regrant(log, tp->t_ticket);
		else
			xfs_log_ticket_ungrant(log, tp->t_ticket);
		tp->t_ticket = NULL;
	}
	xfs_trans_free_items(tp, !!error);
	xfs_trans_free(tp);

	XFS_STATS_INC(mp, xs_trans_empty);
	return error;
}

int
xfs_trans_commit(
	struct xfs_trans	*tp)
{
	return __xfs_trans_commit(tp, false);
}

 
void
xfs_trans_cancel(
	struct xfs_trans	*tp)
{
	struct xfs_mount	*mp = tp->t_mountp;
	struct xlog		*log = mp->m_log;
	bool			dirty = (tp->t_flags & XFS_TRANS_DIRTY);

	trace_xfs_trans_cancel(tp, _RET_IP_);

	 
	if (!list_empty(&tp->t_dfops)) {
		ASSERT(tp->t_flags & XFS_TRANS_PERM_LOG_RES);
		dirty = true;
		xfs_defer_cancel(tp);
	}

	 
	if (dirty && !xfs_is_shutdown(mp)) {
		XFS_ERROR_REPORT("xfs_trans_cancel", XFS_ERRLEVEL_LOW, mp);
		xfs_force_shutdown(mp, SHUTDOWN_CORRUPT_INCORE);
	}
#ifdef DEBUG
	 
	if (!dirty && !xlog_is_shutdown(log)) {
		struct xfs_log_item *lip;

		list_for_each_entry(lip, &tp->t_items, li_trans)
			ASSERT(!xlog_item_is_intent_done(lip));
	}
#endif
	xfs_trans_unreserve_and_mod_sb(tp);
	xfs_trans_unreserve_and_mod_dquots(tp);

	if (tp->t_ticket) {
		xfs_log_ticket_ungrant(log, tp->t_ticket);
		tp->t_ticket = NULL;
	}

	xfs_trans_free_items(tp, dirty);
	xfs_trans_free(tp);
}

 
int
xfs_trans_roll(
	struct xfs_trans	**tpp)
{
	struct xfs_trans	*trans = *tpp;
	struct xfs_trans_res	tres;
	int			error;

	trace_xfs_trans_roll(trans, _RET_IP_);

	 
	tres.tr_logres = trans->t_log_res;
	tres.tr_logcount = trans->t_log_count;

	*tpp = xfs_trans_dup(trans);

	 
	error = __xfs_trans_commit(trans, true);
	if (error)
		return error;

	 
	tres.tr_logflags = XFS_TRANS_PERM_LOG_RES;
	return xfs_trans_reserve(*tpp, &tres, 0, 0);
}

 
int
xfs_trans_alloc_inode(
	struct xfs_inode	*ip,
	struct xfs_trans_res	*resv,
	unsigned int		dblocks,
	unsigned int		rblocks,
	bool			force,
	struct xfs_trans	**tpp)
{
	struct xfs_trans	*tp;
	struct xfs_mount	*mp = ip->i_mount;
	bool			retried = false;
	int			error;

retry:
	error = xfs_trans_alloc(mp, resv, dblocks,
			rblocks / mp->m_sb.sb_rextsize,
			force ? XFS_TRANS_RESERVE : 0, &tp);
	if (error)
		return error;

	xfs_ilock(ip, XFS_ILOCK_EXCL);
	xfs_trans_ijoin(tp, ip, 0);

	error = xfs_qm_dqattach_locked(ip, false);
	if (error) {
		 
		ASSERT(error != -ENOENT);
		goto out_cancel;
	}

	error = xfs_trans_reserve_quota_nblks(tp, ip, dblocks, rblocks, force);
	if ((error == -EDQUOT || error == -ENOSPC) && !retried) {
		xfs_trans_cancel(tp);
		xfs_iunlock(ip, XFS_ILOCK_EXCL);
		xfs_blockgc_free_quota(ip, 0);
		retried = true;
		goto retry;
	}
	if (error)
		goto out_cancel;

	*tpp = tp;
	return 0;

out_cancel:
	xfs_trans_cancel(tp);
	xfs_iunlock(ip, XFS_ILOCK_EXCL);
	return error;
}

 
int
xfs_trans_alloc_icreate(
	struct xfs_mount	*mp,
	struct xfs_trans_res	*resv,
	struct xfs_dquot	*udqp,
	struct xfs_dquot	*gdqp,
	struct xfs_dquot	*pdqp,
	unsigned int		dblocks,
	struct xfs_trans	**tpp)
{
	struct xfs_trans	*tp;
	bool			retried = false;
	int			error;

retry:
	error = xfs_trans_alloc(mp, resv, dblocks, 0, 0, &tp);
	if (error)
		return error;

	error = xfs_trans_reserve_quota_icreate(tp, udqp, gdqp, pdqp, dblocks);
	if ((error == -EDQUOT || error == -ENOSPC) && !retried) {
		xfs_trans_cancel(tp);
		xfs_blockgc_free_dquots(mp, udqp, gdqp, pdqp, 0);
		retried = true;
		goto retry;
	}
	if (error) {
		xfs_trans_cancel(tp);
		return error;
	}

	*tpp = tp;
	return 0;
}

 
int
xfs_trans_alloc_ichange(
	struct xfs_inode	*ip,
	struct xfs_dquot	*new_udqp,
	struct xfs_dquot	*new_gdqp,
	struct xfs_dquot	*new_pdqp,
	bool			force,
	struct xfs_trans	**tpp)
{
	struct xfs_trans	*tp;
	struct xfs_mount	*mp = ip->i_mount;
	struct xfs_dquot	*udqp;
	struct xfs_dquot	*gdqp;
	struct xfs_dquot	*pdqp;
	bool			retried = false;
	int			error;

retry:
	error = xfs_trans_alloc(mp, &M_RES(mp)->tr_ichange, 0, 0, 0, &tp);
	if (error)
		return error;

	xfs_ilock(ip, XFS_ILOCK_EXCL);
	xfs_trans_ijoin(tp, ip, XFS_ILOCK_EXCL);

	error = xfs_qm_dqattach_locked(ip, false);
	if (error) {
		 
		ASSERT(error != -ENOENT);
		goto out_cancel;
	}

	 
	udqp = (new_udqp != ip->i_udquot) ? new_udqp : NULL;
	gdqp = (new_gdqp != ip->i_gdquot) ? new_gdqp : NULL;
	pdqp = (new_pdqp != ip->i_pdquot) ? new_pdqp : NULL;
	if (udqp || gdqp || pdqp) {
		unsigned int	qflags = XFS_QMOPT_RES_REGBLKS;

		if (force)
			qflags |= XFS_QMOPT_FORCE_RES;

		 
		error = xfs_trans_reserve_quota_bydquots(tp, mp, udqp, gdqp,
				pdqp, ip->i_nblocks + ip->i_delayed_blks,
				1, qflags);
		if ((error == -EDQUOT || error == -ENOSPC) && !retried) {
			xfs_trans_cancel(tp);
			xfs_blockgc_free_dquots(mp, udqp, gdqp, pdqp, 0);
			retried = true;
			goto retry;
		}
		if (error)
			goto out_cancel;
	}

	*tpp = tp;
	return 0;

out_cancel:
	xfs_trans_cancel(tp);
	return error;
}

 
int
xfs_trans_alloc_dir(
	struct xfs_inode	*dp,
	struct xfs_trans_res	*resv,
	struct xfs_inode	*ip,
	unsigned int		*dblocks,
	struct xfs_trans	**tpp,
	int			*nospace_error)
{
	struct xfs_trans	*tp;
	struct xfs_mount	*mp = ip->i_mount;
	unsigned int		resblks;
	bool			retried = false;
	int			error;

retry:
	*nospace_error = 0;
	resblks = *dblocks;
	error = xfs_trans_alloc(mp, resv, resblks, 0, 0, &tp);
	if (error == -ENOSPC) {
		*nospace_error = error;
		resblks = 0;
		error = xfs_trans_alloc(mp, resv, resblks, 0, 0, &tp);
	}
	if (error)
		return error;

	xfs_lock_two_inodes(dp, XFS_ILOCK_EXCL, ip, XFS_ILOCK_EXCL);

	xfs_trans_ijoin(tp, dp, XFS_ILOCK_EXCL);
	xfs_trans_ijoin(tp, ip, XFS_ILOCK_EXCL);

	error = xfs_qm_dqattach_locked(dp, false);
	if (error) {
		 
		ASSERT(error != -ENOENT);
		goto out_cancel;
	}

	error = xfs_qm_dqattach_locked(ip, false);
	if (error) {
		 
		ASSERT(error != -ENOENT);
		goto out_cancel;
	}

	if (resblks == 0)
		goto done;

	error = xfs_trans_reserve_quota_nblks(tp, dp, resblks, 0, false);
	if (error == -EDQUOT || error == -ENOSPC) {
		if (!retried) {
			xfs_trans_cancel(tp);
			xfs_blockgc_free_quota(dp, 0);
			retried = true;
			goto retry;
		}

		*nospace_error = error;
		resblks = 0;
		error = 0;
	}
	if (error)
		goto out_cancel;

done:
	*tpp = tp;
	*dblocks = resblks;
	return 0;

out_cancel:
	xfs_trans_cancel(tp);
	return error;
}
