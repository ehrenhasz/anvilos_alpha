
 
#include "xfs.h"
#include "xfs_fs.h"
#include "xfs_shared.h"
#include "xfs_format.h"
#include "xfs_log_format.h"
#include "xfs_trans_resv.h"
#include "xfs_mount.h"
#include "xfs_defer.h"
#include "xfs_trans.h"
#include "xfs_buf_item.h"
#include "xfs_inode.h"
#include "xfs_inode_item.h"
#include "xfs_trace.h"
#include "xfs_icache.h"
#include "xfs_log.h"
#include "xfs_rmap.h"
#include "xfs_refcount.h"
#include "xfs_bmap.h"
#include "xfs_alloc.h"
#include "xfs_buf.h"
#include "xfs_da_format.h"
#include "xfs_da_btree.h"
#include "xfs_attr.h"

static struct kmem_cache	*xfs_defer_pending_cache;

 

static const struct xfs_defer_op_type *defer_op_types[] = {
	[XFS_DEFER_OPS_TYPE_BMAP]	= &xfs_bmap_update_defer_type,
	[XFS_DEFER_OPS_TYPE_REFCOUNT]	= &xfs_refcount_update_defer_type,
	[XFS_DEFER_OPS_TYPE_RMAP]	= &xfs_rmap_update_defer_type,
	[XFS_DEFER_OPS_TYPE_FREE]	= &xfs_extent_free_defer_type,
	[XFS_DEFER_OPS_TYPE_AGFL_FREE]	= &xfs_agfl_free_defer_type,
	[XFS_DEFER_OPS_TYPE_ATTR]	= &xfs_attr_defer_type,
};

 
static int
xfs_defer_create_intent(
	struct xfs_trans		*tp,
	struct xfs_defer_pending	*dfp,
	bool				sort)
{
	const struct xfs_defer_op_type	*ops = defer_op_types[dfp->dfp_type];
	struct xfs_log_item		*lip;

	if (dfp->dfp_intent)
		return 1;

	lip = ops->create_intent(tp, &dfp->dfp_work, dfp->dfp_count, sort);
	if (!lip)
		return 0;
	if (IS_ERR(lip))
		return PTR_ERR(lip);

	dfp->dfp_intent = lip;
	return 1;
}

 
static int
xfs_defer_create_intents(
	struct xfs_trans		*tp)
{
	struct xfs_defer_pending	*dfp;
	int				ret = 0;

	list_for_each_entry(dfp, &tp->t_dfops, dfp_list) {
		int			ret2;

		trace_xfs_defer_create_intent(tp->t_mountp, dfp);
		ret2 = xfs_defer_create_intent(tp, dfp, true);
		if (ret2 < 0)
			return ret2;
		ret |= ret2;
	}
	return ret;
}

 
STATIC void
xfs_defer_trans_abort(
	struct xfs_trans		*tp,
	struct list_head		*dop_pending)
{
	struct xfs_defer_pending	*dfp;
	const struct xfs_defer_op_type	*ops;

	trace_xfs_defer_trans_abort(tp, _RET_IP_);

	 
	list_for_each_entry(dfp, dop_pending, dfp_list) {
		ops = defer_op_types[dfp->dfp_type];
		trace_xfs_defer_pending_abort(tp->t_mountp, dfp);
		if (dfp->dfp_intent && !dfp->dfp_done) {
			ops->abort_intent(dfp->dfp_intent);
			dfp->dfp_intent = NULL;
		}
	}
}

 
static int
xfs_defer_save_resources(
	struct xfs_defer_resources	*dres,
	struct xfs_trans		*tp)
{
	struct xfs_buf_log_item		*bli;
	struct xfs_inode_log_item	*ili;
	struct xfs_log_item		*lip;

	BUILD_BUG_ON(NBBY * sizeof(dres->dr_ordered) < XFS_DEFER_OPS_NR_BUFS);

	list_for_each_entry(lip, &tp->t_items, li_trans) {
		switch (lip->li_type) {
		case XFS_LI_BUF:
			bli = container_of(lip, struct xfs_buf_log_item,
					   bli_item);
			if (bli->bli_flags & XFS_BLI_HOLD) {
				if (dres->dr_bufs >= XFS_DEFER_OPS_NR_BUFS) {
					ASSERT(0);
					return -EFSCORRUPTED;
				}
				if (bli->bli_flags & XFS_BLI_ORDERED)
					dres->dr_ordered |=
							(1U << dres->dr_bufs);
				else
					xfs_trans_dirty_buf(tp, bli->bli_buf);
				dres->dr_bp[dres->dr_bufs++] = bli->bli_buf;
			}
			break;
		case XFS_LI_INODE:
			ili = container_of(lip, struct xfs_inode_log_item,
					   ili_item);
			if (ili->ili_lock_flags == 0) {
				if (dres->dr_inos >= XFS_DEFER_OPS_NR_INODES) {
					ASSERT(0);
					return -EFSCORRUPTED;
				}
				xfs_trans_log_inode(tp, ili->ili_inode,
						    XFS_ILOG_CORE);
				dres->dr_ip[dres->dr_inos++] = ili->ili_inode;
			}
			break;
		default:
			break;
		}
	}

	return 0;
}

 
static void
xfs_defer_restore_resources(
	struct xfs_trans		*tp,
	struct xfs_defer_resources	*dres)
{
	unsigned short			i;

	 
	for (i = 0; i < dres->dr_inos; i++)
		xfs_trans_ijoin(tp, dres->dr_ip[i], 0);

	 
	for (i = 0; i < dres->dr_bufs; i++) {
		xfs_trans_bjoin(tp, dres->dr_bp[i]);
		if (dres->dr_ordered & (1U << i))
			xfs_trans_ordered_buf(tp, dres->dr_bp[i]);
		xfs_trans_bhold(tp, dres->dr_bp[i]);
	}
}

 
STATIC int
xfs_defer_trans_roll(
	struct xfs_trans		**tpp)
{
	struct xfs_defer_resources	dres = { };
	int				error;

	error = xfs_defer_save_resources(&dres, *tpp);
	if (error)
		return error;

	trace_xfs_defer_trans_roll(*tpp, _RET_IP_);

	 
	error = xfs_trans_roll(tpp);

	xfs_defer_restore_resources(*tpp, &dres);

	if (error)
		trace_xfs_defer_trans_roll_error(*tpp, error);
	return error;
}

 
static void
xfs_defer_cancel_list(
	struct xfs_mount		*mp,
	struct list_head		*dop_list)
{
	struct xfs_defer_pending	*dfp;
	struct xfs_defer_pending	*pli;
	struct list_head		*pwi;
	struct list_head		*n;
	const struct xfs_defer_op_type	*ops;

	 
	list_for_each_entry_safe(dfp, pli, dop_list, dfp_list) {
		ops = defer_op_types[dfp->dfp_type];
		trace_xfs_defer_cancel_list(mp, dfp);
		list_del(&dfp->dfp_list);
		list_for_each_safe(pwi, n, &dfp->dfp_work) {
			list_del(pwi);
			dfp->dfp_count--;
			trace_xfs_defer_cancel_item(mp, dfp, pwi);
			ops->cancel_item(pwi);
		}
		ASSERT(dfp->dfp_count == 0);
		kmem_cache_free(xfs_defer_pending_cache, dfp);
	}
}

 
static int
xfs_defer_relog(
	struct xfs_trans		**tpp,
	struct list_head		*dfops)
{
	struct xlog			*log = (*tpp)->t_mountp->m_log;
	struct xfs_defer_pending	*dfp;
	xfs_lsn_t			threshold_lsn = NULLCOMMITLSN;


	ASSERT((*tpp)->t_flags & XFS_TRANS_PERM_LOG_RES);

	list_for_each_entry(dfp, dfops, dfp_list) {
		 
		if (dfp->dfp_intent == NULL ||
		    xfs_log_item_in_current_chkpt(dfp->dfp_intent))
			continue;

		 
		if (threshold_lsn == NULLCOMMITLSN) {
			threshold_lsn = xlog_grant_push_threshold(log, 0);
			if (threshold_lsn == NULLCOMMITLSN)
				break;
		}
		if (XFS_LSN_CMP(dfp->dfp_intent->li_lsn, threshold_lsn) >= 0)
			continue;

		trace_xfs_defer_relog_intent((*tpp)->t_mountp, dfp);
		XFS_STATS_INC((*tpp)->t_mountp, defer_relog);
		dfp->dfp_intent = xfs_trans_item_relog(dfp->dfp_intent, *tpp);
	}

	if ((*tpp)->t_flags & XFS_TRANS_DIRTY)
		return xfs_defer_trans_roll(tpp);
	return 0;
}

 
static int
xfs_defer_finish_one(
	struct xfs_trans		*tp,
	struct xfs_defer_pending	*dfp)
{
	const struct xfs_defer_op_type	*ops = defer_op_types[dfp->dfp_type];
	struct xfs_btree_cur		*state = NULL;
	struct list_head		*li, *n;
	int				error;

	trace_xfs_defer_pending_finish(tp->t_mountp, dfp);

	dfp->dfp_done = ops->create_done(tp, dfp->dfp_intent, dfp->dfp_count);
	list_for_each_safe(li, n, &dfp->dfp_work) {
		list_del(li);
		dfp->dfp_count--;
		trace_xfs_defer_finish_item(tp->t_mountp, dfp, li);
		error = ops->finish_item(tp, dfp->dfp_done, li, &state);
		if (error == -EAGAIN) {
			int		ret;

			 
			list_add(li, &dfp->dfp_work);
			dfp->dfp_count++;
			dfp->dfp_done = NULL;
			dfp->dfp_intent = NULL;
			ret = xfs_defer_create_intent(tp, dfp, false);
			if (ret < 0)
				error = ret;
		}

		if (error)
			goto out;
	}

	 
	list_del(&dfp->dfp_list);
	kmem_cache_free(xfs_defer_pending_cache, dfp);
out:
	if (ops->finish_cleanup)
		ops->finish_cleanup(tp, state, error);
	return error;
}

 
int
xfs_defer_finish_noroll(
	struct xfs_trans		**tp)
{
	struct xfs_defer_pending	*dfp = NULL;
	int				error = 0;
	LIST_HEAD(dop_pending);

	ASSERT((*tp)->t_flags & XFS_TRANS_PERM_LOG_RES);

	trace_xfs_defer_finish(*tp, _RET_IP_);

	 
	while (!list_empty(&dop_pending) || !list_empty(&(*tp)->t_dfops)) {
		 
		int has_intents = xfs_defer_create_intents(*tp);

		list_splice_init(&(*tp)->t_dfops, &dop_pending);

		if (has_intents < 0) {
			error = has_intents;
			goto out_shutdown;
		}
		if (has_intents || dfp) {
			error = xfs_defer_trans_roll(tp);
			if (error)
				goto out_shutdown;

			 
			error = xfs_defer_relog(tp, &dop_pending);
			if (error)
				goto out_shutdown;
		}

		dfp = list_first_entry(&dop_pending, struct xfs_defer_pending,
				       dfp_list);
		error = xfs_defer_finish_one(*tp, dfp);
		if (error && error != -EAGAIN)
			goto out_shutdown;
	}

	trace_xfs_defer_finish_done(*tp, _RET_IP_);
	return 0;

out_shutdown:
	xfs_defer_trans_abort(*tp, &dop_pending);
	xfs_force_shutdown((*tp)->t_mountp, SHUTDOWN_CORRUPT_INCORE);
	trace_xfs_defer_finish_error(*tp, error);
	xfs_defer_cancel_list((*tp)->t_mountp, &dop_pending);
	xfs_defer_cancel(*tp);
	return error;
}

int
xfs_defer_finish(
	struct xfs_trans	**tp)
{
	int			error;

	 
	error = xfs_defer_finish_noroll(tp);
	if (error)
		return error;
	if ((*tp)->t_flags & XFS_TRANS_DIRTY) {
		error = xfs_defer_trans_roll(tp);
		if (error) {
			xfs_force_shutdown((*tp)->t_mountp,
					   SHUTDOWN_CORRUPT_INCORE);
			return error;
		}
	}

	 
	ASSERT(list_empty(&(*tp)->t_dfops));
	(*tp)->t_flags &= ~XFS_TRANS_LOWMODE;
	return 0;
}

void
xfs_defer_cancel(
	struct xfs_trans	*tp)
{
	struct xfs_mount	*mp = tp->t_mountp;

	trace_xfs_defer_cancel(tp, _RET_IP_);
	xfs_defer_cancel_list(mp, &tp->t_dfops);
}

 
void
xfs_defer_add(
	struct xfs_trans		*tp,
	enum xfs_defer_ops_type		type,
	struct list_head		*li)
{
	struct xfs_defer_pending	*dfp = NULL;
	const struct xfs_defer_op_type	*ops = defer_op_types[type];

	ASSERT(tp->t_flags & XFS_TRANS_PERM_LOG_RES);
	BUILD_BUG_ON(ARRAY_SIZE(defer_op_types) != XFS_DEFER_OPS_TYPE_MAX);

	 
	if (!list_empty(&tp->t_dfops)) {
		dfp = list_last_entry(&tp->t_dfops,
				struct xfs_defer_pending, dfp_list);
		if (dfp->dfp_type != type ||
		    (ops->max_items && dfp->dfp_count >= ops->max_items))
			dfp = NULL;
	}
	if (!dfp) {
		dfp = kmem_cache_zalloc(xfs_defer_pending_cache,
				GFP_NOFS | __GFP_NOFAIL);
		dfp->dfp_type = type;
		dfp->dfp_intent = NULL;
		dfp->dfp_done = NULL;
		dfp->dfp_count = 0;
		INIT_LIST_HEAD(&dfp->dfp_work);
		list_add_tail(&dfp->dfp_list, &tp->t_dfops);
	}

	list_add_tail(li, &dfp->dfp_work);
	trace_xfs_defer_add_item(tp->t_mountp, dfp, li);
	dfp->dfp_count++;
}

 
void
xfs_defer_move(
	struct xfs_trans	*dtp,
	struct xfs_trans	*stp)
{
	list_splice_init(&stp->t_dfops, &dtp->t_dfops);

	 
	dtp->t_flags |= (stp->t_flags & XFS_TRANS_LOWMODE);
	stp->t_flags &= ~XFS_TRANS_LOWMODE;
}

 
static struct xfs_defer_capture *
xfs_defer_ops_capture(
	struct xfs_trans		*tp)
{
	struct xfs_defer_capture	*dfc;
	unsigned short			i;
	int				error;

	if (list_empty(&tp->t_dfops))
		return NULL;

	error = xfs_defer_create_intents(tp);
	if (error < 0)
		return ERR_PTR(error);

	 
	dfc = kmem_zalloc(sizeof(*dfc), KM_NOFS);
	INIT_LIST_HEAD(&dfc->dfc_list);
	INIT_LIST_HEAD(&dfc->dfc_dfops);

	 
	list_splice_init(&tp->t_dfops, &dfc->dfc_dfops);
	dfc->dfc_tpflags = tp->t_flags & XFS_TRANS_LOWMODE;
	tp->t_flags &= ~XFS_TRANS_LOWMODE;

	 
	dfc->dfc_blkres = tp->t_blk_res - tp->t_blk_res_used;
	dfc->dfc_rtxres = tp->t_rtx_res - tp->t_rtx_res_used;

	 
	dfc->dfc_logres = tp->t_log_res;

	error = xfs_defer_save_resources(&dfc->dfc_held, tp);
	if (error) {
		 
		xfs_force_shutdown(tp->t_mountp, SHUTDOWN_CORRUPT_INCORE);
	}

	 
	for (i = 0; i < dfc->dfc_held.dr_inos; i++) {
		ASSERT(xfs_isilocked(dfc->dfc_held.dr_ip[i], XFS_ILOCK_EXCL));
		ihold(VFS_I(dfc->dfc_held.dr_ip[i]));
	}

	for (i = 0; i < dfc->dfc_held.dr_bufs; i++)
		xfs_buf_hold(dfc->dfc_held.dr_bp[i]);

	return dfc;
}

 
void
xfs_defer_ops_capture_free(
	struct xfs_mount		*mp,
	struct xfs_defer_capture	*dfc)
{
	unsigned short			i;

	xfs_defer_cancel_list(mp, &dfc->dfc_dfops);

	for (i = 0; i < dfc->dfc_held.dr_bufs; i++)
		xfs_buf_relse(dfc->dfc_held.dr_bp[i]);

	for (i = 0; i < dfc->dfc_held.dr_inos; i++)
		xfs_irele(dfc->dfc_held.dr_ip[i]);

	kmem_free(dfc);
}

 
int
xfs_defer_ops_capture_and_commit(
	struct xfs_trans		*tp,
	struct list_head		*capture_list)
{
	struct xfs_mount		*mp = tp->t_mountp;
	struct xfs_defer_capture	*dfc;
	int				error;

	 
	dfc = xfs_defer_ops_capture(tp);
	if (IS_ERR(dfc)) {
		xfs_trans_cancel(tp);
		return PTR_ERR(dfc);
	}
	if (!dfc)
		return xfs_trans_commit(tp);

	 
	error = xfs_trans_commit(tp);
	if (error) {
		xfs_defer_ops_capture_free(mp, dfc);
		return error;
	}

	list_add_tail(&dfc->dfc_list, capture_list);
	return 0;
}

 
void
xfs_defer_ops_continue(
	struct xfs_defer_capture	*dfc,
	struct xfs_trans		*tp,
	struct xfs_defer_resources	*dres)
{
	unsigned int			i;

	ASSERT(tp->t_flags & XFS_TRANS_PERM_LOG_RES);
	ASSERT(!(tp->t_flags & XFS_TRANS_DIRTY));

	 
	if (dfc->dfc_held.dr_inos == 2)
		xfs_lock_two_inodes(dfc->dfc_held.dr_ip[0], XFS_ILOCK_EXCL,
				    dfc->dfc_held.dr_ip[1], XFS_ILOCK_EXCL);
	else if (dfc->dfc_held.dr_inos == 1)
		xfs_ilock(dfc->dfc_held.dr_ip[0], XFS_ILOCK_EXCL);

	for (i = 0; i < dfc->dfc_held.dr_bufs; i++)
		xfs_buf_lock(dfc->dfc_held.dr_bp[i]);

	 
	xfs_defer_restore_resources(tp, &dfc->dfc_held);
	memcpy(dres, &dfc->dfc_held, sizeof(struct xfs_defer_resources));
	dres->dr_bufs = 0;

	 
	list_splice_init(&dfc->dfc_dfops, &tp->t_dfops);
	tp->t_flags |= dfc->dfc_tpflags;

	kmem_free(dfc);
}

 
void
xfs_defer_resources_rele(
	struct xfs_defer_resources	*dres)
{
	unsigned short			i;

	for (i = 0; i < dres->dr_inos; i++) {
		xfs_iunlock(dres->dr_ip[i], XFS_ILOCK_EXCL);
		xfs_irele(dres->dr_ip[i]);
		dres->dr_ip[i] = NULL;
	}

	for (i = 0; i < dres->dr_bufs; i++) {
		xfs_buf_relse(dres->dr_bp[i]);
		dres->dr_bp[i] = NULL;
	}

	dres->dr_inos = 0;
	dres->dr_bufs = 0;
	dres->dr_ordered = 0;
}

static inline int __init
xfs_defer_init_cache(void)
{
	xfs_defer_pending_cache = kmem_cache_create("xfs_defer_pending",
			sizeof(struct xfs_defer_pending),
			0, 0, NULL);

	return xfs_defer_pending_cache != NULL ? 0 : -ENOMEM;
}

static inline void
xfs_defer_destroy_cache(void)
{
	kmem_cache_destroy(xfs_defer_pending_cache);
	xfs_defer_pending_cache = NULL;
}

 
int __init
xfs_defer_init_item_caches(void)
{
	int				error;

	error = xfs_defer_init_cache();
	if (error)
		return error;
	error = xfs_rmap_intent_init_cache();
	if (error)
		goto err;
	error = xfs_refcount_intent_init_cache();
	if (error)
		goto err;
	error = xfs_bmap_intent_init_cache();
	if (error)
		goto err;
	error = xfs_extfree_intent_init_cache();
	if (error)
		goto err;
	error = xfs_attr_intent_init_cache();
	if (error)
		goto err;
	return 0;
err:
	xfs_defer_destroy_item_caches();
	return error;
}

 
void
xfs_defer_destroy_item_caches(void)
{
	xfs_attr_intent_destroy_cache();
	xfs_extfree_intent_destroy_cache();
	xfs_bmap_intent_destroy_cache();
	xfs_refcount_intent_destroy_cache();
	xfs_rmap_intent_destroy_cache();
	xfs_defer_destroy_cache();
}
