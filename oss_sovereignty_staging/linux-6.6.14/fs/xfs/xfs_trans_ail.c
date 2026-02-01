
 
#include "xfs.h"
#include "xfs_fs.h"
#include "xfs_shared.h"
#include "xfs_format.h"
#include "xfs_log_format.h"
#include "xfs_trans_resv.h"
#include "xfs_mount.h"
#include "xfs_trans.h"
#include "xfs_trans_priv.h"
#include "xfs_trace.h"
#include "xfs_errortag.h"
#include "xfs_error.h"
#include "xfs_log.h"
#include "xfs_log_priv.h"

#ifdef DEBUG
 
STATIC void
xfs_ail_check(
	struct xfs_ail		*ailp,
	struct xfs_log_item	*lip)
	__must_hold(&ailp->ail_lock)
{
	struct xfs_log_item	*prev_lip;
	struct xfs_log_item	*next_lip;
	xfs_lsn_t		prev_lsn = NULLCOMMITLSN;
	xfs_lsn_t		next_lsn = NULLCOMMITLSN;
	xfs_lsn_t		lsn;
	bool			in_ail;


	if (list_empty(&ailp->ail_head))
		return;

	 
	in_ail = test_bit(XFS_LI_IN_AIL, &lip->li_flags);
	prev_lip = list_entry(lip->li_ail.prev, struct xfs_log_item, li_ail);
	if (&prev_lip->li_ail != &ailp->ail_head)
		prev_lsn = prev_lip->li_lsn;
	next_lip = list_entry(lip->li_ail.next, struct xfs_log_item, li_ail);
	if (&next_lip->li_ail != &ailp->ail_head)
		next_lsn = next_lip->li_lsn;
	lsn = lip->li_lsn;

	if (in_ail &&
	    (prev_lsn == NULLCOMMITLSN || XFS_LSN_CMP(prev_lsn, lsn) <= 0) &&
	    (next_lsn == NULLCOMMITLSN || XFS_LSN_CMP(next_lsn, lsn) >= 0))
		return;

	spin_unlock(&ailp->ail_lock);
	ASSERT(in_ail);
	ASSERT(prev_lsn == NULLCOMMITLSN || XFS_LSN_CMP(prev_lsn, lsn) <= 0);
	ASSERT(next_lsn == NULLCOMMITLSN || XFS_LSN_CMP(next_lsn, lsn) >= 0);
	spin_lock(&ailp->ail_lock);
}
#else  
#define	xfs_ail_check(a,l)
#endif  

 
static struct xfs_log_item *
xfs_ail_max(
	struct xfs_ail  *ailp)
{
	if (list_empty(&ailp->ail_head))
		return NULL;

	return list_entry(ailp->ail_head.prev, struct xfs_log_item, li_ail);
}

 
static struct xfs_log_item *
xfs_ail_next(
	struct xfs_ail		*ailp,
	struct xfs_log_item	*lip)
{
	if (lip->li_ail.next == &ailp->ail_head)
		return NULL;

	return list_first_entry(&lip->li_ail, struct xfs_log_item, li_ail);
}

 
static xfs_lsn_t
__xfs_ail_min_lsn(
	struct xfs_ail		*ailp)
{
	struct xfs_log_item	*lip = xfs_ail_min(ailp);

	if (lip)
		return lip->li_lsn;
	return 0;
}

xfs_lsn_t
xfs_ail_min_lsn(
	struct xfs_ail		*ailp)
{
	xfs_lsn_t		lsn;

	spin_lock(&ailp->ail_lock);
	lsn = __xfs_ail_min_lsn(ailp);
	spin_unlock(&ailp->ail_lock);

	return lsn;
}

 
static xfs_lsn_t
xfs_ail_max_lsn(
	struct xfs_ail		*ailp)
{
	xfs_lsn_t       	lsn = 0;
	struct xfs_log_item	*lip;

	spin_lock(&ailp->ail_lock);
	lip = xfs_ail_max(ailp);
	if (lip)
		lsn = lip->li_lsn;
	spin_unlock(&ailp->ail_lock);

	return lsn;
}

 
STATIC void
xfs_trans_ail_cursor_init(
	struct xfs_ail		*ailp,
	struct xfs_ail_cursor	*cur)
{
	cur->item = NULL;
	list_add_tail(&cur->list, &ailp->ail_cursors);
}

 
struct xfs_log_item *
xfs_trans_ail_cursor_next(
	struct xfs_ail		*ailp,
	struct xfs_ail_cursor	*cur)
{
	struct xfs_log_item	*lip = cur->item;

	if ((uintptr_t)lip & 1)
		lip = xfs_ail_min(ailp);
	if (lip)
		cur->item = xfs_ail_next(ailp, lip);
	return lip;
}

 
void
xfs_trans_ail_cursor_done(
	struct xfs_ail_cursor	*cur)
{
	cur->item = NULL;
	list_del_init(&cur->list);
}

 
STATIC void
xfs_trans_ail_cursor_clear(
	struct xfs_ail		*ailp,
	struct xfs_log_item	*lip)
{
	struct xfs_ail_cursor	*cur;

	list_for_each_entry(cur, &ailp->ail_cursors, list) {
		if (cur->item == lip)
			cur->item = (struct xfs_log_item *)
					((uintptr_t)cur->item | 1);
	}
}

 
struct xfs_log_item *
xfs_trans_ail_cursor_first(
	struct xfs_ail		*ailp,
	struct xfs_ail_cursor	*cur,
	xfs_lsn_t		lsn)
{
	struct xfs_log_item	*lip;

	xfs_trans_ail_cursor_init(ailp, cur);

	if (lsn == 0) {
		lip = xfs_ail_min(ailp);
		goto out;
	}

	list_for_each_entry(lip, &ailp->ail_head, li_ail) {
		if (XFS_LSN_CMP(lip->li_lsn, lsn) >= 0)
			goto out;
	}
	return NULL;

out:
	if (lip)
		cur->item = xfs_ail_next(ailp, lip);
	return lip;
}

static struct xfs_log_item *
__xfs_trans_ail_cursor_last(
	struct xfs_ail		*ailp,
	xfs_lsn_t		lsn)
{
	struct xfs_log_item	*lip;

	list_for_each_entry_reverse(lip, &ailp->ail_head, li_ail) {
		if (XFS_LSN_CMP(lip->li_lsn, lsn) <= 0)
			return lip;
	}
	return NULL;
}

 
struct xfs_log_item *
xfs_trans_ail_cursor_last(
	struct xfs_ail		*ailp,
	struct xfs_ail_cursor	*cur,
	xfs_lsn_t		lsn)
{
	xfs_trans_ail_cursor_init(ailp, cur);
	cur->item = __xfs_trans_ail_cursor_last(ailp, lsn);
	return cur->item;
}

 
static void
xfs_ail_splice(
	struct xfs_ail		*ailp,
	struct xfs_ail_cursor	*cur,
	struct list_head	*list,
	xfs_lsn_t		lsn)
{
	struct xfs_log_item	*lip;

	ASSERT(!list_empty(list));

	 
	lip = cur ? cur->item : NULL;
	if (!lip || (uintptr_t)lip & 1)
		lip = __xfs_trans_ail_cursor_last(ailp, lsn);

	 
	if (cur)
		cur->item = list_entry(list->prev, struct xfs_log_item, li_ail);

	 
	if (lip)
		list_splice(list, &lip->li_ail);
	else
		list_splice(list, &ailp->ail_head);
}

 
static void
xfs_ail_delete(
	struct xfs_ail		*ailp,
	struct xfs_log_item	*lip)
{
	xfs_ail_check(ailp, lip);
	list_del(&lip->li_ail);
	xfs_trans_ail_cursor_clear(ailp, lip);
}

 
static inline int
xfsaild_resubmit_item(
	struct xfs_log_item	*lip,
	struct list_head	*buffer_list)
{
	struct xfs_buf		*bp = lip->li_buf;

	if (!xfs_buf_trylock(bp))
		return XFS_ITEM_LOCKED;

	if (!xfs_buf_delwri_queue(bp, buffer_list)) {
		xfs_buf_unlock(bp);
		return XFS_ITEM_FLUSHING;
	}

	 
	list_for_each_entry(lip, &bp->b_li_list, li_bio_list) {
		if (bp->b_flags & _XBF_INODES)
			clear_bit(XFS_LI_FAILED, &lip->li_flags);
		else
			xfs_clear_li_failed(lip);
	}

	xfs_buf_unlock(bp);
	return XFS_ITEM_SUCCESS;
}

static inline uint
xfsaild_push_item(
	struct xfs_ail		*ailp,
	struct xfs_log_item	*lip)
{
	 
	if (XFS_TEST_ERROR(false, ailp->ail_log->l_mp, XFS_ERRTAG_LOG_ITEM_PIN))
		return XFS_ITEM_PINNED;

	 
	if (!lip->li_ops->iop_push)
		return XFS_ITEM_PINNED;
	if (test_bit(XFS_LI_FAILED, &lip->li_flags))
		return xfsaild_resubmit_item(lip, &ailp->ail_buf_list);
	return lip->li_ops->iop_push(lip, &ailp->ail_buf_list);
}

static long
xfsaild_push(
	struct xfs_ail		*ailp)
{
	struct xfs_mount	*mp = ailp->ail_log->l_mp;
	struct xfs_ail_cursor	cur;
	struct xfs_log_item	*lip;
	xfs_lsn_t		lsn;
	xfs_lsn_t		target = NULLCOMMITLSN;
	long			tout;
	int			stuck = 0;
	int			flushing = 0;
	int			count = 0;

	 
	if (ailp->ail_log_flush && ailp->ail_last_pushed_lsn == 0 &&
	    (!list_empty_careful(&ailp->ail_buf_list) ||
	     xfs_ail_min_lsn(ailp))) {
		ailp->ail_log_flush = 0;

		XFS_STATS_INC(mp, xs_push_ail_flush);
		xlog_cil_flush(ailp->ail_log);
	}

	spin_lock(&ailp->ail_lock);

	 
	if (waitqueue_active(&ailp->ail_empty)) {
		lip = xfs_ail_max(ailp);
		if (lip)
			target = lip->li_lsn;
	} else {
		 
		smp_rmb();
		target = ailp->ail_target;
		ailp->ail_target_prev = target;
	}

	 
	lip = xfs_trans_ail_cursor_first(ailp, &cur, ailp->ail_last_pushed_lsn);
	if (!lip)
		goto out_done;

	XFS_STATS_INC(mp, xs_push_ail);

	ASSERT(target != NULLCOMMITLSN);

	lsn = lip->li_lsn;
	while ((XFS_LSN_CMP(lip->li_lsn, target) <= 0)) {
		int	lock_result;

		 
		lock_result = xfsaild_push_item(ailp, lip);
		switch (lock_result) {
		case XFS_ITEM_SUCCESS:
			XFS_STATS_INC(mp, xs_push_ail_success);
			trace_xfs_ail_push(lip);

			ailp->ail_last_pushed_lsn = lsn;
			break;

		case XFS_ITEM_FLUSHING:
			 
			XFS_STATS_INC(mp, xs_push_ail_flushing);
			trace_xfs_ail_flushing(lip);

			flushing++;
			ailp->ail_last_pushed_lsn = lsn;
			break;

		case XFS_ITEM_PINNED:
			XFS_STATS_INC(mp, xs_push_ail_pinned);
			trace_xfs_ail_pinned(lip);

			stuck++;
			ailp->ail_log_flush++;
			break;
		case XFS_ITEM_LOCKED:
			XFS_STATS_INC(mp, xs_push_ail_locked);
			trace_xfs_ail_locked(lip);

			stuck++;
			break;
		default:
			ASSERT(0);
			break;
		}

		count++;

		 
		if (stuck > 100)
			break;

		lip = xfs_trans_ail_cursor_next(ailp, &cur);
		if (lip == NULL)
			break;
		lsn = lip->li_lsn;
	}

out_done:
	xfs_trans_ail_cursor_done(&cur);
	spin_unlock(&ailp->ail_lock);

	if (xfs_buf_delwri_submit_nowait(&ailp->ail_buf_list))
		ailp->ail_log_flush++;

	if (!count || XFS_LSN_CMP(lsn, target) >= 0) {
		 
		tout = 50;
		ailp->ail_last_pushed_lsn = 0;
	} else if (((stuck + flushing) * 100) / count > 90) {
		 
		tout = 20;
		ailp->ail_last_pushed_lsn = 0;
	} else {
		 
		tout = 10;
	}

	return tout;
}

static int
xfsaild(
	void		*data)
{
	struct xfs_ail	*ailp = data;
	long		tout = 0;	 
	unsigned int	noreclaim_flag;

	noreclaim_flag = memalloc_noreclaim_save();
	set_freezable();

	while (1) {
		if (tout && tout <= 20)
			set_current_state(TASK_KILLABLE|TASK_FREEZABLE);
		else
			set_current_state(TASK_INTERRUPTIBLE|TASK_FREEZABLE);

		 
		if (kthread_should_stop()) {
			__set_current_state(TASK_RUNNING);

			 
			ASSERT(list_empty(&ailp->ail_buf_list) ||
			       xlog_is_shutdown(ailp->ail_log));
			xfs_buf_delwri_cancel(&ailp->ail_buf_list);
			break;
		}

		spin_lock(&ailp->ail_lock);

		 
		smp_rmb();
		if (!xfs_ail_min(ailp) &&
		    ailp->ail_target == ailp->ail_target_prev &&
		    list_empty(&ailp->ail_buf_list)) {
			spin_unlock(&ailp->ail_lock);
			schedule();
			tout = 0;
			continue;
		}
		spin_unlock(&ailp->ail_lock);

		if (tout)
			schedule_timeout(msecs_to_jiffies(tout));

		__set_current_state(TASK_RUNNING);

		try_to_freeze();

		tout = xfsaild_push(ailp);
	}

	memalloc_noreclaim_restore(noreclaim_flag);
	return 0;
}

 
void
xfs_ail_push(
	struct xfs_ail		*ailp,
	xfs_lsn_t		threshold_lsn)
{
	struct xfs_log_item	*lip;

	lip = xfs_ail_min(ailp);
	if (!lip || xlog_is_shutdown(ailp->ail_log) ||
	    XFS_LSN_CMP(threshold_lsn, ailp->ail_target) <= 0)
		return;

	 
	smp_wmb();
	xfs_trans_ail_copy_lsn(ailp, &ailp->ail_target, &threshold_lsn);
	smp_wmb();

	wake_up_process(ailp->ail_task);
}

 
void
xfs_ail_push_all(
	struct xfs_ail  *ailp)
{
	xfs_lsn_t       threshold_lsn = xfs_ail_max_lsn(ailp);

	if (threshold_lsn)
		xfs_ail_push(ailp, threshold_lsn);
}

 
void
xfs_ail_push_all_sync(
	struct xfs_ail  *ailp)
{
	DEFINE_WAIT(wait);

	spin_lock(&ailp->ail_lock);
	while (xfs_ail_max(ailp) != NULL) {
		prepare_to_wait(&ailp->ail_empty, &wait, TASK_UNINTERRUPTIBLE);
		wake_up_process(ailp->ail_task);
		spin_unlock(&ailp->ail_lock);
		schedule();
		spin_lock(&ailp->ail_lock);
	}
	spin_unlock(&ailp->ail_lock);

	finish_wait(&ailp->ail_empty, &wait);
}

void
xfs_ail_update_finish(
	struct xfs_ail		*ailp,
	xfs_lsn_t		old_lsn) __releases(ailp->ail_lock)
{
	struct xlog		*log = ailp->ail_log;

	 
	if (!old_lsn || old_lsn == __xfs_ail_min_lsn(ailp)) {
		spin_unlock(&ailp->ail_lock);
		return;
	}

	if (!xlog_is_shutdown(log))
		xlog_assign_tail_lsn_locked(log->l_mp);

	if (list_empty(&ailp->ail_head))
		wake_up_all(&ailp->ail_empty);
	spin_unlock(&ailp->ail_lock);
	xfs_log_space_wake(log->l_mp);
}

 
void
xfs_trans_ail_update_bulk(
	struct xfs_ail		*ailp,
	struct xfs_ail_cursor	*cur,
	struct xfs_log_item	**log_items,
	int			nr_items,
	xfs_lsn_t		lsn) __releases(ailp->ail_lock)
{
	struct xfs_log_item	*mlip;
	xfs_lsn_t		tail_lsn = 0;
	int			i;
	LIST_HEAD(tmp);

	ASSERT(nr_items > 0);		 
	mlip = xfs_ail_min(ailp);

	for (i = 0; i < nr_items; i++) {
		struct xfs_log_item *lip = log_items[i];
		if (test_and_set_bit(XFS_LI_IN_AIL, &lip->li_flags)) {
			 
			if (XFS_LSN_CMP(lsn, lip->li_lsn) <= 0)
				continue;

			trace_xfs_ail_move(lip, lip->li_lsn, lsn);
			if (mlip == lip && !tail_lsn)
				tail_lsn = lip->li_lsn;

			xfs_ail_delete(ailp, lip);
		} else {
			trace_xfs_ail_insert(lip, 0, lsn);
		}
		lip->li_lsn = lsn;
		list_add_tail(&lip->li_ail, &tmp);
	}

	if (!list_empty(&tmp))
		xfs_ail_splice(ailp, cur, &tmp, lsn);

	xfs_ail_update_finish(ailp, tail_lsn);
}

 
void
xfs_trans_ail_insert(
	struct xfs_ail		*ailp,
	struct xfs_log_item	*lip,
	xfs_lsn_t		lsn)
{
	spin_lock(&ailp->ail_lock);
	xfs_trans_ail_update_bulk(ailp, NULL, &lip, 1, lsn);
}

 
xfs_lsn_t
xfs_ail_delete_one(
	struct xfs_ail		*ailp,
	struct xfs_log_item	*lip)
{
	struct xfs_log_item	*mlip = xfs_ail_min(ailp);
	xfs_lsn_t		lsn = lip->li_lsn;

	trace_xfs_ail_delete(lip, mlip->li_lsn, lip->li_lsn);
	xfs_ail_delete(ailp, lip);
	clear_bit(XFS_LI_IN_AIL, &lip->li_flags);
	lip->li_lsn = 0;

	if (mlip == lip)
		return lsn;
	return 0;
}

void
xfs_trans_ail_delete(
	struct xfs_log_item	*lip,
	int			shutdown_type)
{
	struct xfs_ail		*ailp = lip->li_ailp;
	struct xlog		*log = ailp->ail_log;
	xfs_lsn_t		tail_lsn;

	spin_lock(&ailp->ail_lock);
	if (!test_bit(XFS_LI_IN_AIL, &lip->li_flags)) {
		spin_unlock(&ailp->ail_lock);
		if (shutdown_type && !xlog_is_shutdown(log)) {
			xfs_alert_tag(log->l_mp, XFS_PTAG_AILDELETE,
	"%s: attempting to delete a log item that is not in the AIL",
					__func__);
			xlog_force_shutdown(log, shutdown_type);
		}
		return;
	}

	 
	xfs_clear_li_failed(lip);
	tail_lsn = xfs_ail_delete_one(ailp, lip);
	xfs_ail_update_finish(ailp, tail_lsn);
}

int
xfs_trans_ail_init(
	xfs_mount_t	*mp)
{
	struct xfs_ail	*ailp;

	ailp = kmem_zalloc(sizeof(struct xfs_ail), KM_MAYFAIL);
	if (!ailp)
		return -ENOMEM;

	ailp->ail_log = mp->m_log;
	INIT_LIST_HEAD(&ailp->ail_head);
	INIT_LIST_HEAD(&ailp->ail_cursors);
	spin_lock_init(&ailp->ail_lock);
	INIT_LIST_HEAD(&ailp->ail_buf_list);
	init_waitqueue_head(&ailp->ail_empty);

	ailp->ail_task = kthread_run(xfsaild, ailp, "xfsaild/%s",
				mp->m_super->s_id);
	if (IS_ERR(ailp->ail_task))
		goto out_free_ailp;

	mp->m_ail = ailp;
	return 0;

out_free_ailp:
	kmem_free(ailp);
	return -ENOMEM;
}

void
xfs_trans_ail_destroy(
	xfs_mount_t	*mp)
{
	struct xfs_ail	*ailp = mp->m_ail;

	kthread_stop(ailp->ail_task);
	kmem_free(ailp);
}
