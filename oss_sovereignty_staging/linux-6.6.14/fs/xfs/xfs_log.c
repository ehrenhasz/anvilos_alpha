
 
#include "xfs.h"
#include "xfs_fs.h"
#include "xfs_shared.h"
#include "xfs_format.h"
#include "xfs_log_format.h"
#include "xfs_trans_resv.h"
#include "xfs_mount.h"
#include "xfs_errortag.h"
#include "xfs_error.h"
#include "xfs_trans.h"
#include "xfs_trans_priv.h"
#include "xfs_log.h"
#include "xfs_log_priv.h"
#include "xfs_trace.h"
#include "xfs_sysfs.h"
#include "xfs_sb.h"
#include "xfs_health.h"

struct kmem_cache	*xfs_log_ticket_cache;

 
STATIC struct xlog *
xlog_alloc_log(
	struct xfs_mount	*mp,
	struct xfs_buftarg	*log_target,
	xfs_daddr_t		blk_offset,
	int			num_bblks);
STATIC int
xlog_space_left(
	struct xlog		*log,
	atomic64_t		*head);
STATIC void
xlog_dealloc_log(
	struct xlog		*log);

 
STATIC void xlog_state_done_syncing(
	struct xlog_in_core	*iclog);
STATIC void xlog_state_do_callback(
	struct xlog		*log);
STATIC int
xlog_state_get_iclog_space(
	struct xlog		*log,
	int			len,
	struct xlog_in_core	**iclog,
	struct xlog_ticket	*ticket,
	int			*logoffsetp);
STATIC void
xlog_grant_push_ail(
	struct xlog		*log,
	int			need_bytes);
STATIC void
xlog_sync(
	struct xlog		*log,
	struct xlog_in_core	*iclog,
	struct xlog_ticket	*ticket);
#if defined(DEBUG)
STATIC void
xlog_verify_grant_tail(
	struct xlog *log);
STATIC void
xlog_verify_iclog(
	struct xlog		*log,
	struct xlog_in_core	*iclog,
	int			count);
STATIC void
xlog_verify_tail_lsn(
	struct xlog		*log,
	struct xlog_in_core	*iclog);
#else
#define xlog_verify_grant_tail(a)
#define xlog_verify_iclog(a,b,c)
#define xlog_verify_tail_lsn(a,b)
#endif

STATIC int
xlog_iclogs_empty(
	struct xlog		*log);

static int
xfs_log_cover(struct xfs_mount *);

 
void *
xlog_prepare_iovec(
	struct xfs_log_vec	*lv,
	struct xfs_log_iovec	**vecp,
	uint			type)
{
	struct xfs_log_iovec	*vec = *vecp;
	struct xlog_op_header	*oph;
	uint32_t		len;
	void			*buf;

	if (vec) {
		ASSERT(vec - lv->lv_iovecp < lv->lv_niovecs);
		vec++;
	} else {
		vec = &lv->lv_iovecp[0];
	}

	len = lv->lv_buf_len + sizeof(struct xlog_op_header);
	if (!IS_ALIGNED(len, sizeof(uint64_t))) {
		lv->lv_buf_len = round_up(len, sizeof(uint64_t)) -
					sizeof(struct xlog_op_header);
	}

	vec->i_type = type;
	vec->i_addr = lv->lv_buf + lv->lv_buf_len;

	oph = vec->i_addr;
	oph->oh_clientid = XFS_TRANSACTION;
	oph->oh_res2 = 0;
	oph->oh_flags = 0;

	buf = vec->i_addr + sizeof(struct xlog_op_header);
	ASSERT(IS_ALIGNED((unsigned long)buf, sizeof(uint64_t)));

	*vecp = vec;
	return buf;
}

static void
xlog_grant_sub_space(
	struct xlog		*log,
	atomic64_t		*head,
	int			bytes)
{
	int64_t	head_val = atomic64_read(head);
	int64_t new, old;

	do {
		int	cycle, space;

		xlog_crack_grant_head_val(head_val, &cycle, &space);

		space -= bytes;
		if (space < 0) {
			space += log->l_logsize;
			cycle--;
		}

		old = head_val;
		new = xlog_assign_grant_head_val(cycle, space);
		head_val = atomic64_cmpxchg(head, old, new);
	} while (head_val != old);
}

static void
xlog_grant_add_space(
	struct xlog		*log,
	atomic64_t		*head,
	int			bytes)
{
	int64_t	head_val = atomic64_read(head);
	int64_t new, old;

	do {
		int		tmp;
		int		cycle, space;

		xlog_crack_grant_head_val(head_val, &cycle, &space);

		tmp = log->l_logsize - space;
		if (tmp > bytes)
			space += bytes;
		else {
			space = bytes - tmp;
			cycle++;
		}

		old = head_val;
		new = xlog_assign_grant_head_val(cycle, space);
		head_val = atomic64_cmpxchg(head, old, new);
	} while (head_val != old);
}

STATIC void
xlog_grant_head_init(
	struct xlog_grant_head	*head)
{
	xlog_assign_grant_head(&head->grant, 1, 0);
	INIT_LIST_HEAD(&head->waiters);
	spin_lock_init(&head->lock);
}

STATIC void
xlog_grant_head_wake_all(
	struct xlog_grant_head	*head)
{
	struct xlog_ticket	*tic;

	spin_lock(&head->lock);
	list_for_each_entry(tic, &head->waiters, t_queue)
		wake_up_process(tic->t_task);
	spin_unlock(&head->lock);
}

static inline int
xlog_ticket_reservation(
	struct xlog		*log,
	struct xlog_grant_head	*head,
	struct xlog_ticket	*tic)
{
	if (head == &log->l_write_head) {
		ASSERT(tic->t_flags & XLOG_TIC_PERM_RESERV);
		return tic->t_unit_res;
	}

	if (tic->t_flags & XLOG_TIC_PERM_RESERV)
		return tic->t_unit_res * tic->t_cnt;

	return tic->t_unit_res;
}

STATIC bool
xlog_grant_head_wake(
	struct xlog		*log,
	struct xlog_grant_head	*head,
	int			*free_bytes)
{
	struct xlog_ticket	*tic;
	int			need_bytes;
	bool			woken_task = false;

	list_for_each_entry(tic, &head->waiters, t_queue) {

		 

		need_bytes = xlog_ticket_reservation(log, head, tic);
		if (*free_bytes < need_bytes) {
			if (!woken_task)
				xlog_grant_push_ail(log, need_bytes);
			return false;
		}

		*free_bytes -= need_bytes;
		trace_xfs_log_grant_wake_up(log, tic);
		wake_up_process(tic->t_task);
		woken_task = true;
	}

	return true;
}

STATIC int
xlog_grant_head_wait(
	struct xlog		*log,
	struct xlog_grant_head	*head,
	struct xlog_ticket	*tic,
	int			need_bytes) __releases(&head->lock)
					    __acquires(&head->lock)
{
	list_add_tail(&tic->t_queue, &head->waiters);

	do {
		if (xlog_is_shutdown(log))
			goto shutdown;
		xlog_grant_push_ail(log, need_bytes);

		__set_current_state(TASK_UNINTERRUPTIBLE);
		spin_unlock(&head->lock);

		XFS_STATS_INC(log->l_mp, xs_sleep_logspace);

		trace_xfs_log_grant_sleep(log, tic);
		schedule();
		trace_xfs_log_grant_wake(log, tic);

		spin_lock(&head->lock);
		if (xlog_is_shutdown(log))
			goto shutdown;
	} while (xlog_space_left(log, &head->grant) < need_bytes);

	list_del_init(&tic->t_queue);
	return 0;
shutdown:
	list_del_init(&tic->t_queue);
	return -EIO;
}

 
STATIC int
xlog_grant_head_check(
	struct xlog		*log,
	struct xlog_grant_head	*head,
	struct xlog_ticket	*tic,
	int			*need_bytes)
{
	int			free_bytes;
	int			error = 0;

	ASSERT(!xlog_in_recovery(log));

	 
	*need_bytes = xlog_ticket_reservation(log, head, tic);
	free_bytes = xlog_space_left(log, &head->grant);
	if (!list_empty_careful(&head->waiters)) {
		spin_lock(&head->lock);
		if (!xlog_grant_head_wake(log, head, &free_bytes) ||
		    free_bytes < *need_bytes) {
			error = xlog_grant_head_wait(log, head, tic,
						     *need_bytes);
		}
		spin_unlock(&head->lock);
	} else if (free_bytes < *need_bytes) {
		spin_lock(&head->lock);
		error = xlog_grant_head_wait(log, head, tic, *need_bytes);
		spin_unlock(&head->lock);
	}

	return error;
}

bool
xfs_log_writable(
	struct xfs_mount	*mp)
{
	 
	if (xfs_has_norecovery(mp))
		return false;
	if (xfs_readonly_buftarg(mp->m_ddev_targp))
		return false;
	if (xfs_readonly_buftarg(mp->m_log->l_targ))
		return false;
	if (xlog_is_shutdown(mp->m_log))
		return false;
	return true;
}

 
int
xfs_log_regrant(
	struct xfs_mount	*mp,
	struct xlog_ticket	*tic)
{
	struct xlog		*log = mp->m_log;
	int			need_bytes;
	int			error = 0;

	if (xlog_is_shutdown(log))
		return -EIO;

	XFS_STATS_INC(mp, xs_try_logspace);

	 
	tic->t_tid++;

	xlog_grant_push_ail(log, tic->t_unit_res);

	tic->t_curr_res = tic->t_unit_res;
	if (tic->t_cnt > 0)
		return 0;

	trace_xfs_log_regrant(log, tic);

	error = xlog_grant_head_check(log, &log->l_write_head, tic,
				      &need_bytes);
	if (error)
		goto out_error;

	xlog_grant_add_space(log, &log->l_write_head.grant, need_bytes);
	trace_xfs_log_regrant_exit(log, tic);
	xlog_verify_grant_tail(log);
	return 0;

out_error:
	 
	tic->t_curr_res = 0;
	tic->t_cnt = 0;	 
	return error;
}

 
int
xfs_log_reserve(
	struct xfs_mount	*mp,
	int			unit_bytes,
	int			cnt,
	struct xlog_ticket	**ticp,
	bool			permanent)
{
	struct xlog		*log = mp->m_log;
	struct xlog_ticket	*tic;
	int			need_bytes;
	int			error = 0;

	if (xlog_is_shutdown(log))
		return -EIO;

	XFS_STATS_INC(mp, xs_try_logspace);

	ASSERT(*ticp == NULL);
	tic = xlog_ticket_alloc(log, unit_bytes, cnt, permanent);
	*ticp = tic;

	xlog_grant_push_ail(log, tic->t_cnt ? tic->t_unit_res * tic->t_cnt
					    : tic->t_unit_res);

	trace_xfs_log_reserve(log, tic);

	error = xlog_grant_head_check(log, &log->l_reserve_head, tic,
				      &need_bytes);
	if (error)
		goto out_error;

	xlog_grant_add_space(log, &log->l_reserve_head.grant, need_bytes);
	xlog_grant_add_space(log, &log->l_write_head.grant, need_bytes);
	trace_xfs_log_reserve_exit(log, tic);
	xlog_verify_grant_tail(log);
	return 0;

out_error:
	 
	tic->t_curr_res = 0;
	tic->t_cnt = 0;	 
	return error;
}

 
static void
xlog_state_shutdown_callbacks(
	struct xlog		*log)
{
	struct xlog_in_core	*iclog;
	LIST_HEAD(cb_list);

	iclog = log->l_iclog;
	do {
		if (atomic_read(&iclog->ic_refcnt)) {
			 
			continue;
		}
		list_splice_init(&iclog->ic_callbacks, &cb_list);
		spin_unlock(&log->l_icloglock);

		xlog_cil_process_committed(&cb_list);

		spin_lock(&log->l_icloglock);
		wake_up_all(&iclog->ic_write_wait);
		wake_up_all(&iclog->ic_force_wait);
	} while ((iclog = iclog->ic_next) != log->l_iclog);

	wake_up_all(&log->l_flush_wait);
}

 
int
xlog_state_release_iclog(
	struct xlog		*log,
	struct xlog_in_core	*iclog,
	struct xlog_ticket	*ticket)
{
	xfs_lsn_t		tail_lsn;
	bool			last_ref;

	lockdep_assert_held(&log->l_icloglock);

	trace_xlog_iclog_release(iclog, _RET_IP_);
	 
	if ((iclog->ic_state == XLOG_STATE_WANT_SYNC ||
	     (iclog->ic_flags & XLOG_ICL_NEED_FUA)) &&
	    !iclog->ic_header.h_tail_lsn) {
		tail_lsn = xlog_assign_tail_lsn(log->l_mp);
		iclog->ic_header.h_tail_lsn = cpu_to_be64(tail_lsn);
	}

	last_ref = atomic_dec_and_test(&iclog->ic_refcnt);

	if (xlog_is_shutdown(log)) {
		 
		if (last_ref)
			xlog_state_shutdown_callbacks(log);
		return -EIO;
	}

	if (!last_ref)
		return 0;

	if (iclog->ic_state != XLOG_STATE_WANT_SYNC) {
		ASSERT(iclog->ic_state == XLOG_STATE_ACTIVE);
		return 0;
	}

	iclog->ic_state = XLOG_STATE_SYNCING;
	xlog_verify_tail_lsn(log, iclog);
	trace_xlog_iclog_syncing(iclog, _RET_IP_);

	spin_unlock(&log->l_icloglock);
	xlog_sync(log, iclog, ticket);
	spin_lock(&log->l_icloglock);
	return 0;
}

 
int
xfs_log_mount(
	xfs_mount_t	*mp,
	xfs_buftarg_t	*log_target,
	xfs_daddr_t	blk_offset,
	int		num_bblks)
{
	struct xlog	*log;
	int		error = 0;
	int		min_logfsbs;

	if (!xfs_has_norecovery(mp)) {
		xfs_notice(mp, "Mounting V%d Filesystem %pU",
			   XFS_SB_VERSION_NUM(&mp->m_sb),
			   &mp->m_sb.sb_uuid);
	} else {
		xfs_notice(mp,
"Mounting V%d filesystem %pU in no-recovery mode. Filesystem will be inconsistent.",
			   XFS_SB_VERSION_NUM(&mp->m_sb),
			   &mp->m_sb.sb_uuid);
		ASSERT(xfs_is_readonly(mp));
	}

	log = xlog_alloc_log(mp, log_target, blk_offset, num_bblks);
	if (IS_ERR(log)) {
		error = PTR_ERR(log);
		goto out;
	}
	mp->m_log = log;

	 
	min_logfsbs = xfs_log_calc_minimum_size(mp);
	if (mp->m_sb.sb_logblocks < min_logfsbs) {
		xfs_warn(mp,
		"Log size %d blocks too small, minimum size is %d blocks",
			 mp->m_sb.sb_logblocks, min_logfsbs);

		 
		if (xfs_has_crc(mp)) {
			xfs_crit(mp, "AAIEEE! Log failed size checks. Abort!");
			ASSERT(0);
			error = -EINVAL;
			goto out_free_log;
		}
		xfs_crit(mp, "Log size out of supported range.");
		xfs_crit(mp,
"Continuing onwards, but if log hangs are experienced then please report this message in the bug report.");
	}

	 
	error = xfs_trans_ail_init(mp);
	if (error) {
		xfs_warn(mp, "AIL initialisation failed: error %d", error);
		goto out_free_log;
	}
	log->l_ailp = mp->m_ail;

	 
	if (!xfs_has_norecovery(mp)) {
		error = xlog_recover(log);
		if (error) {
			xfs_warn(mp, "log mount/recovery failed: error %d",
				error);
			xlog_recover_cancel(log);
			goto out_destroy_ail;
		}
	}

	error = xfs_sysfs_init(&log->l_kobj, &xfs_log_ktype, &mp->m_kobj,
			       "log");
	if (error)
		goto out_destroy_ail;

	 
	clear_bit(XLOG_ACTIVE_RECOVERY, &log->l_opstate);

	 
	xlog_cil_init_post_recovery(log);

	return 0;

out_destroy_ail:
	xfs_trans_ail_destroy(mp);
out_free_log:
	xlog_dealloc_log(log);
out:
	return error;
}

 
int
xfs_log_mount_finish(
	struct xfs_mount	*mp)
{
	struct xlog		*log = mp->m_log;
	int			error = 0;

	if (xfs_has_norecovery(mp)) {
		ASSERT(xfs_is_readonly(mp));
		return 0;
	}

	 
	mp->m_super->s_flags |= SB_ACTIVE;
	xfs_log_work_queue(mp);
	if (xlog_recovery_needed(log))
		error = xlog_recover_finish(log);
	mp->m_super->s_flags &= ~SB_ACTIVE;
	evict_inodes(mp->m_super);

	 
	if (xlog_recovery_needed(log)) {
		if (!error) {
			xfs_log_force(mp, XFS_LOG_SYNC);
			xfs_ail_push_all_sync(mp->m_ail);
		}
		xfs_notice(mp, "Ending recovery (logdev: %s)",
				mp->m_logname ? mp->m_logname : "internal");
	} else {
		xfs_info(mp, "Ending clean mount");
	}
	xfs_buftarg_drain(mp->m_ddev_targp);

	clear_bit(XLOG_RECOVERY_NEEDED, &log->l_opstate);

	 
	ASSERT(!error || xlog_is_shutdown(log));

	return error;
}

 
void
xfs_log_mount_cancel(
	struct xfs_mount	*mp)
{
	xlog_recover_cancel(mp->m_log);
	xfs_log_unmount(mp);
}

 
static inline int
xlog_force_iclog(
	struct xlog_in_core	*iclog)
{
	atomic_inc(&iclog->ic_refcnt);
	iclog->ic_flags |= XLOG_ICL_NEED_FLUSH | XLOG_ICL_NEED_FUA;
	if (iclog->ic_state == XLOG_STATE_ACTIVE)
		xlog_state_switch_iclogs(iclog->ic_log, iclog, 0);
	return xlog_state_release_iclog(iclog->ic_log, iclog, NULL);
}

 
static void
xlog_wait_iclog_completion(struct xlog *log)
{
	int		i;
	struct xlog_in_core	*iclog = log->l_iclog;

	for (i = 0; i < log->l_iclog_bufs; i++) {
		down(&iclog->ic_sema);
		up(&iclog->ic_sema);
		iclog = iclog->ic_next;
	}
}

 
int
xlog_wait_on_iclog(
	struct xlog_in_core	*iclog)
		__releases(iclog->ic_log->l_icloglock)
{
	struct xlog		*log = iclog->ic_log;

	trace_xlog_iclog_wait_on(iclog, _RET_IP_);
	if (!xlog_is_shutdown(log) &&
	    iclog->ic_state != XLOG_STATE_ACTIVE &&
	    iclog->ic_state != XLOG_STATE_DIRTY) {
		XFS_STATS_INC(log->l_mp, xs_log_force_sleep);
		xlog_wait(&iclog->ic_force_wait, &log->l_icloglock);
	} else {
		spin_unlock(&log->l_icloglock);
	}

	if (xlog_is_shutdown(log))
		return -EIO;
	return 0;
}

 
static int
xlog_write_unmount_record(
	struct xlog		*log,
	struct xlog_ticket	*ticket)
{
	struct  {
		struct xlog_op_header ophdr;
		struct xfs_unmount_log_format ulf;
	} unmount_rec = {
		.ophdr = {
			.oh_clientid = XFS_LOG,
			.oh_tid = cpu_to_be32(ticket->t_tid),
			.oh_flags = XLOG_UNMOUNT_TRANS,
		},
		.ulf = {
			.magic = XLOG_UNMOUNT_TYPE,
		},
	};
	struct xfs_log_iovec reg = {
		.i_addr = &unmount_rec,
		.i_len = sizeof(unmount_rec),
		.i_type = XLOG_REG_TYPE_UNMOUNT,
	};
	struct xfs_log_vec vec = {
		.lv_niovecs = 1,
		.lv_iovecp = &reg,
	};
	LIST_HEAD(lv_chain);
	list_add(&vec.lv_list, &lv_chain);

	BUILD_BUG_ON((sizeof(struct xlog_op_header) +
		      sizeof(struct xfs_unmount_log_format)) !=
							sizeof(unmount_rec));

	 
	ticket->t_curr_res -= sizeof(unmount_rec);

	return xlog_write(log, NULL, &lv_chain, ticket, reg.i_len);
}

 
static void
xlog_unmount_write(
	struct xlog		*log)
{
	struct xfs_mount	*mp = log->l_mp;
	struct xlog_in_core	*iclog;
	struct xlog_ticket	*tic = NULL;
	int			error;

	error = xfs_log_reserve(mp, 600, 1, &tic, 0);
	if (error)
		goto out_err;

	error = xlog_write_unmount_record(log, tic);
	 
out_err:
	if (error)
		xfs_alert(mp, "%s: unmount record failed", __func__);

	spin_lock(&log->l_icloglock);
	iclog = log->l_iclog;
	error = xlog_force_iclog(iclog);
	xlog_wait_on_iclog(iclog);

	if (tic) {
		trace_xfs_log_umount_write(log, tic);
		xfs_log_ticket_ungrant(log, tic);
	}
}

static void
xfs_log_unmount_verify_iclog(
	struct xlog		*log)
{
	struct xlog_in_core	*iclog = log->l_iclog;

	do {
		ASSERT(iclog->ic_state == XLOG_STATE_ACTIVE);
		ASSERT(iclog->ic_offset == 0);
	} while ((iclog = iclog->ic_next) != log->l_iclog);
}

 
static void
xfs_log_unmount_write(
	struct xfs_mount	*mp)
{
	struct xlog		*log = mp->m_log;

	if (!xfs_log_writable(mp))
		return;

	xfs_log_force(mp, XFS_LOG_SYNC);

	if (xlog_is_shutdown(log))
		return;

	 
	if (XFS_TEST_ERROR(xfs_fs_has_sickness(mp, XFS_SICK_FS_COUNTERS), mp,
			XFS_ERRTAG_FORCE_SUMMARY_RECALC)) {
		xfs_alert(mp, "%s: will fix summary counters at next mount",
				__func__);
		return;
	}

	xfs_log_unmount_verify_iclog(log);
	xlog_unmount_write(log);
}

 
int
xfs_log_quiesce(
	struct xfs_mount	*mp)
{
	 
	if (xfs_clear_incompat_log_features(mp)) {
		int error;

		error = xfs_sync_sb(mp, false);
		if (error)
			xfs_warn(mp,
	"Failed to clear log incompat features on quiesce");
	}

	cancel_delayed_work_sync(&mp->m_log->l_work);
	xfs_log_force(mp, XFS_LOG_SYNC);

	 
	xfs_ail_push_all_sync(mp->m_ail);
	xfs_buftarg_wait(mp->m_ddev_targp);
	xfs_buf_lock(mp->m_sb_bp);
	xfs_buf_unlock(mp->m_sb_bp);

	return xfs_log_cover(mp);
}

void
xfs_log_clean(
	struct xfs_mount	*mp)
{
	xfs_log_quiesce(mp);
	xfs_log_unmount_write(mp);
}

 
void
xfs_log_unmount(
	struct xfs_mount	*mp)
{
	xfs_log_clean(mp);

	 
	xlog_wait_iclog_completion(mp->m_log);

	xfs_buftarg_drain(mp->m_ddev_targp);

	xfs_trans_ail_destroy(mp);

	xfs_sysfs_del(&mp->m_log->l_kobj);

	xlog_dealloc_log(mp->m_log);
}

void
xfs_log_item_init(
	struct xfs_mount	*mp,
	struct xfs_log_item	*item,
	int			type,
	const struct xfs_item_ops *ops)
{
	item->li_log = mp->m_log;
	item->li_ailp = mp->m_ail;
	item->li_type = type;
	item->li_ops = ops;
	item->li_lv = NULL;

	INIT_LIST_HEAD(&item->li_ail);
	INIT_LIST_HEAD(&item->li_cil);
	INIT_LIST_HEAD(&item->li_bio_list);
	INIT_LIST_HEAD(&item->li_trans);
}

 
void
xfs_log_space_wake(
	struct xfs_mount	*mp)
{
	struct xlog		*log = mp->m_log;
	int			free_bytes;

	if (xlog_is_shutdown(log))
		return;

	if (!list_empty_careful(&log->l_write_head.waiters)) {
		ASSERT(!xlog_in_recovery(log));

		spin_lock(&log->l_write_head.lock);
		free_bytes = xlog_space_left(log, &log->l_write_head.grant);
		xlog_grant_head_wake(log, &log->l_write_head, &free_bytes);
		spin_unlock(&log->l_write_head.lock);
	}

	if (!list_empty_careful(&log->l_reserve_head.waiters)) {
		ASSERT(!xlog_in_recovery(log));

		spin_lock(&log->l_reserve_head.lock);
		free_bytes = xlog_space_left(log, &log->l_reserve_head.grant);
		xlog_grant_head_wake(log, &log->l_reserve_head, &free_bytes);
		spin_unlock(&log->l_reserve_head.lock);
	}
}

 
static bool
xfs_log_need_covered(
	struct xfs_mount	*mp)
{
	struct xlog		*log = mp->m_log;
	bool			needed = false;

	if (!xlog_cil_empty(log))
		return false;

	spin_lock(&log->l_icloglock);
	switch (log->l_covered_state) {
	case XLOG_STATE_COVER_DONE:
	case XLOG_STATE_COVER_DONE2:
	case XLOG_STATE_COVER_IDLE:
		break;
	case XLOG_STATE_COVER_NEED:
	case XLOG_STATE_COVER_NEED2:
		if (xfs_ail_min_lsn(log->l_ailp))
			break;
		if (!xlog_iclogs_empty(log))
			break;

		needed = true;
		if (log->l_covered_state == XLOG_STATE_COVER_NEED)
			log->l_covered_state = XLOG_STATE_COVER_DONE;
		else
			log->l_covered_state = XLOG_STATE_COVER_DONE2;
		break;
	default:
		needed = true;
		break;
	}
	spin_unlock(&log->l_icloglock);
	return needed;
}

 
static int
xfs_log_cover(
	struct xfs_mount	*mp)
{
	int			error = 0;
	bool			need_covered;

	ASSERT((xlog_cil_empty(mp->m_log) && xlog_iclogs_empty(mp->m_log) &&
	        !xfs_ail_min_lsn(mp->m_log->l_ailp)) ||
		xlog_is_shutdown(mp->m_log));

	if (!xfs_log_writable(mp))
		return 0;

	 
	need_covered = xfs_log_need_covered(mp);
	if (!need_covered && !xfs_has_lazysbcount(mp))
		return 0;

	 
	do {
		error = xfs_sync_sb(mp, true);
		if (error)
			break;
		xfs_ail_push_all_sync(mp->m_ail);
	} while (xfs_log_need_covered(mp));

	return error;
}

 
xfs_lsn_t
xlog_assign_tail_lsn_locked(
	struct xfs_mount	*mp)
{
	struct xlog		*log = mp->m_log;
	struct xfs_log_item	*lip;
	xfs_lsn_t		tail_lsn;

	assert_spin_locked(&mp->m_ail->ail_lock);

	 
	lip = xfs_ail_min(mp->m_ail);
	if (lip)
		tail_lsn = lip->li_lsn;
	else
		tail_lsn = atomic64_read(&log->l_last_sync_lsn);
	trace_xfs_log_assign_tail_lsn(log, tail_lsn);
	atomic64_set(&log->l_tail_lsn, tail_lsn);
	return tail_lsn;
}

xfs_lsn_t
xlog_assign_tail_lsn(
	struct xfs_mount	*mp)
{
	xfs_lsn_t		tail_lsn;

	spin_lock(&mp->m_ail->ail_lock);
	tail_lsn = xlog_assign_tail_lsn_locked(mp);
	spin_unlock(&mp->m_ail->ail_lock);

	return tail_lsn;
}

 
STATIC int
xlog_space_left(
	struct xlog	*log,
	atomic64_t	*head)
{
	int		tail_bytes;
	int		tail_cycle;
	int		head_cycle;
	int		head_bytes;

	xlog_crack_grant_head(head, &head_cycle, &head_bytes);
	xlog_crack_atomic_lsn(&log->l_tail_lsn, &tail_cycle, &tail_bytes);
	tail_bytes = BBTOB(tail_bytes);
	if (tail_cycle == head_cycle && head_bytes >= tail_bytes)
		return log->l_logsize - (head_bytes - tail_bytes);
	if (tail_cycle + 1 < head_cycle)
		return 0;

	 
	if (xlog_is_shutdown(log))
		return log->l_logsize;

	if (tail_cycle < head_cycle) {
		ASSERT(tail_cycle == (head_cycle - 1));
		return tail_bytes - head_bytes;
	}

	 
	xfs_alert(log->l_mp, "xlog_space_left: head behind tail");
	xfs_alert(log->l_mp, "  tail_cycle = %d, tail_bytes = %d",
		  tail_cycle, tail_bytes);
	xfs_alert(log->l_mp, "  GH   cycle = %d, GH   bytes = %d",
		  head_cycle, head_bytes);
	ASSERT(0);
	return log->l_logsize;
}


static void
xlog_ioend_work(
	struct work_struct	*work)
{
	struct xlog_in_core     *iclog =
		container_of(work, struct xlog_in_core, ic_end_io_work);
	struct xlog		*log = iclog->ic_log;
	int			error;

	error = blk_status_to_errno(iclog->ic_bio.bi_status);
#ifdef DEBUG
	 
	if (iclog->ic_fail_crc)
		error = -EIO;
#endif

	 
	if (XFS_TEST_ERROR(error, log->l_mp, XFS_ERRTAG_IODONE_IOERR)) {
		xfs_alert(log->l_mp, "log I/O error %d", error);
		xlog_force_shutdown(log, SHUTDOWN_LOG_IO_ERROR);
	}

	xlog_state_done_syncing(iclog);
	bio_uninit(&iclog->ic_bio);

	 
	up(&iclog->ic_sema);
}

 
STATIC void
xlog_get_iclog_buffer_size(
	struct xfs_mount	*mp,
	struct xlog		*log)
{
	if (mp->m_logbufs <= 0)
		mp->m_logbufs = XLOG_MAX_ICLOGS;
	if (mp->m_logbsize <= 0)
		mp->m_logbsize = XLOG_BIG_RECORD_BSIZE;

	log->l_iclog_bufs = mp->m_logbufs;
	log->l_iclog_size = mp->m_logbsize;

	 
	log->l_iclog_heads =
		DIV_ROUND_UP(mp->m_logbsize, XLOG_HEADER_CYCLE_SIZE);
	log->l_iclog_hsize = log->l_iclog_heads << BBSHIFT;
}

void
xfs_log_work_queue(
	struct xfs_mount        *mp)
{
	queue_delayed_work(mp->m_sync_workqueue, &mp->m_log->l_work,
				msecs_to_jiffies(xfs_syncd_centisecs * 10));
}

 
static inline void
xlog_clear_incompat(
	struct xlog		*log)
{
	struct xfs_mount	*mp = log->l_mp;

	if (!xfs_sb_has_incompat_log_feature(&mp->m_sb,
				XFS_SB_FEAT_INCOMPAT_LOG_ALL))
		return;

	if (log->l_covered_state != XLOG_STATE_COVER_DONE2)
		return;

	if (!down_write_trylock(&log->l_incompat_users))
		return;

	xfs_clear_incompat_log_features(mp);
	up_write(&log->l_incompat_users);
}

 
static void
xfs_log_worker(
	struct work_struct	*work)
{
	struct xlog		*log = container_of(to_delayed_work(work),
						struct xlog, l_work);
	struct xfs_mount	*mp = log->l_mp;

	 
	if (xfs_fs_writable(mp, SB_FREEZE_WRITE) && xfs_log_need_covered(mp)) {
		 
		xlog_clear_incompat(log);
		xfs_sync_sb(mp, true);
	} else
		xfs_log_force(mp, 0);

	 
	xfs_ail_push_all(mp->m_ail);

	 
	xfs_log_work_queue(mp);
}

 
STATIC struct xlog *
xlog_alloc_log(
	struct xfs_mount	*mp,
	struct xfs_buftarg	*log_target,
	xfs_daddr_t		blk_offset,
	int			num_bblks)
{
	struct xlog		*log;
	xlog_rec_header_t	*head;
	xlog_in_core_t		**iclogp;
	xlog_in_core_t		*iclog, *prev_iclog=NULL;
	int			i;
	int			error = -ENOMEM;
	uint			log2_size = 0;

	log = kmem_zalloc(sizeof(struct xlog), KM_MAYFAIL);
	if (!log) {
		xfs_warn(mp, "Log allocation failed: No memory!");
		goto out;
	}

	log->l_mp	   = mp;
	log->l_targ	   = log_target;
	log->l_logsize     = BBTOB(num_bblks);
	log->l_logBBstart  = blk_offset;
	log->l_logBBsize   = num_bblks;
	log->l_covered_state = XLOG_STATE_COVER_IDLE;
	set_bit(XLOG_ACTIVE_RECOVERY, &log->l_opstate);
	INIT_DELAYED_WORK(&log->l_work, xfs_log_worker);

	log->l_prev_block  = -1;
	 
	xlog_assign_atomic_lsn(&log->l_tail_lsn, 1, 0);
	xlog_assign_atomic_lsn(&log->l_last_sync_lsn, 1, 0);
	log->l_curr_cycle  = 1;	     

	if (xfs_has_logv2(mp) && mp->m_sb.sb_logsunit > 1)
		log->l_iclog_roundoff = mp->m_sb.sb_logsunit;
	else
		log->l_iclog_roundoff = BBSIZE;

	xlog_grant_head_init(&log->l_reserve_head);
	xlog_grant_head_init(&log->l_write_head);

	error = -EFSCORRUPTED;
	if (xfs_has_sector(mp)) {
	        log2_size = mp->m_sb.sb_logsectlog;
		if (log2_size < BBSHIFT) {
			xfs_warn(mp, "Log sector size too small (0x%x < 0x%x)",
				log2_size, BBSHIFT);
			goto out_free_log;
		}

	        log2_size -= BBSHIFT;
		if (log2_size > mp->m_sectbb_log) {
			xfs_warn(mp, "Log sector size too large (0x%x > 0x%x)",
				log2_size, mp->m_sectbb_log);
			goto out_free_log;
		}

		 
		if (log2_size && log->l_logBBstart > 0 &&
			    !xfs_has_logv2(mp)) {
			xfs_warn(mp,
		"log sector size (0x%x) invalid for configuration.",
				log2_size);
			goto out_free_log;
		}
	}
	log->l_sectBBsize = 1 << log2_size;

	init_rwsem(&log->l_incompat_users);

	xlog_get_iclog_buffer_size(mp, log);

	spin_lock_init(&log->l_icloglock);
	init_waitqueue_head(&log->l_flush_wait);

	iclogp = &log->l_iclog;
	 
	ASSERT(log->l_iclog_size >= 4096);
	for (i = 0; i < log->l_iclog_bufs; i++) {
		size_t bvec_size = howmany(log->l_iclog_size, PAGE_SIZE) *
				sizeof(struct bio_vec);

		iclog = kmem_zalloc(sizeof(*iclog) + bvec_size, KM_MAYFAIL);
		if (!iclog)
			goto out_free_iclog;

		*iclogp = iclog;
		iclog->ic_prev = prev_iclog;
		prev_iclog = iclog;

		iclog->ic_data = kvzalloc(log->l_iclog_size,
				GFP_KERNEL | __GFP_RETRY_MAYFAIL);
		if (!iclog->ic_data)
			goto out_free_iclog;
		head = &iclog->ic_header;
		memset(head, 0, sizeof(xlog_rec_header_t));
		head->h_magicno = cpu_to_be32(XLOG_HEADER_MAGIC_NUM);
		head->h_version = cpu_to_be32(
			xfs_has_logv2(log->l_mp) ? 2 : 1);
		head->h_size = cpu_to_be32(log->l_iclog_size);
		 
		head->h_fmt = cpu_to_be32(XLOG_FMT);
		memcpy(&head->h_fs_uuid, &mp->m_sb.sb_uuid, sizeof(uuid_t));

		iclog->ic_size = log->l_iclog_size - log->l_iclog_hsize;
		iclog->ic_state = XLOG_STATE_ACTIVE;
		iclog->ic_log = log;
		atomic_set(&iclog->ic_refcnt, 0);
		INIT_LIST_HEAD(&iclog->ic_callbacks);
		iclog->ic_datap = (void *)iclog->ic_data + log->l_iclog_hsize;

		init_waitqueue_head(&iclog->ic_force_wait);
		init_waitqueue_head(&iclog->ic_write_wait);
		INIT_WORK(&iclog->ic_end_io_work, xlog_ioend_work);
		sema_init(&iclog->ic_sema, 1);

		iclogp = &iclog->ic_next;
	}
	*iclogp = log->l_iclog;			 
	log->l_iclog->ic_prev = prev_iclog;	 

	log->l_ioend_workqueue = alloc_workqueue("xfs-log/%s",
			XFS_WQFLAGS(WQ_FREEZABLE | WQ_MEM_RECLAIM |
				    WQ_HIGHPRI),
			0, mp->m_super->s_id);
	if (!log->l_ioend_workqueue)
		goto out_free_iclog;

	error = xlog_cil_init(log);
	if (error)
		goto out_destroy_workqueue;
	return log;

out_destroy_workqueue:
	destroy_workqueue(log->l_ioend_workqueue);
out_free_iclog:
	for (iclog = log->l_iclog; iclog; iclog = prev_iclog) {
		prev_iclog = iclog->ic_next;
		kmem_free(iclog->ic_data);
		kmem_free(iclog);
		if (prev_iclog == log->l_iclog)
			break;
	}
out_free_log:
	kmem_free(log);
out:
	return ERR_PTR(error);
}	 

 
xfs_lsn_t
xlog_grant_push_threshold(
	struct xlog	*log,
	int		need_bytes)
{
	xfs_lsn_t	threshold_lsn = 0;
	xfs_lsn_t	last_sync_lsn;
	int		free_blocks;
	int		free_bytes;
	int		threshold_block;
	int		threshold_cycle;
	int		free_threshold;

	ASSERT(BTOBB(need_bytes) < log->l_logBBsize);

	free_bytes = xlog_space_left(log, &log->l_reserve_head.grant);
	free_blocks = BTOBBT(free_bytes);

	 
	free_threshold = BTOBB(need_bytes);
	free_threshold = max(free_threshold, (log->l_logBBsize >> 2));
	free_threshold = max(free_threshold, 256);
	if (free_blocks >= free_threshold)
		return NULLCOMMITLSN;

	xlog_crack_atomic_lsn(&log->l_tail_lsn, &threshold_cycle,
						&threshold_block);
	threshold_block += free_threshold;
	if (threshold_block >= log->l_logBBsize) {
		threshold_block -= log->l_logBBsize;
		threshold_cycle += 1;
	}
	threshold_lsn = xlog_assign_lsn(threshold_cycle,
					threshold_block);
	 
	last_sync_lsn = atomic64_read(&log->l_last_sync_lsn);
	if (XFS_LSN_CMP(threshold_lsn, last_sync_lsn) > 0)
		threshold_lsn = last_sync_lsn;

	return threshold_lsn;
}

 
STATIC void
xlog_grant_push_ail(
	struct xlog	*log,
	int		need_bytes)
{
	xfs_lsn_t	threshold_lsn;

	threshold_lsn = xlog_grant_push_threshold(log, need_bytes);
	if (threshold_lsn == NULLCOMMITLSN || xlog_is_shutdown(log))
		return;

	 
	xfs_ail_push(log->l_ailp, threshold_lsn);
}

 
STATIC void
xlog_pack_data(
	struct xlog		*log,
	struct xlog_in_core	*iclog,
	int			roundoff)
{
	int			i, j, k;
	int			size = iclog->ic_offset + roundoff;
	__be32			cycle_lsn;
	char			*dp;

	cycle_lsn = CYCLE_LSN_DISK(iclog->ic_header.h_lsn);

	dp = iclog->ic_datap;
	for (i = 0; i < BTOBB(size); i++) {
		if (i >= (XLOG_HEADER_CYCLE_SIZE / BBSIZE))
			break;
		iclog->ic_header.h_cycle_data[i] = *(__be32 *)dp;
		*(__be32 *)dp = cycle_lsn;
		dp += BBSIZE;
	}

	if (xfs_has_logv2(log->l_mp)) {
		xlog_in_core_2_t *xhdr = iclog->ic_data;

		for ( ; i < BTOBB(size); i++) {
			j = i / (XLOG_HEADER_CYCLE_SIZE / BBSIZE);
			k = i % (XLOG_HEADER_CYCLE_SIZE / BBSIZE);
			xhdr[j].hic_xheader.xh_cycle_data[k] = *(__be32 *)dp;
			*(__be32 *)dp = cycle_lsn;
			dp += BBSIZE;
		}

		for (i = 1; i < log->l_iclog_heads; i++)
			xhdr[i].hic_xheader.xh_cycle = cycle_lsn;
	}
}

 
__le32
xlog_cksum(
	struct xlog		*log,
	struct xlog_rec_header	*rhead,
	char			*dp,
	int			size)
{
	uint32_t		crc;

	 
	crc = xfs_start_cksum_update((char *)rhead,
			      sizeof(struct xlog_rec_header),
			      offsetof(struct xlog_rec_header, h_crc));

	 
	if (xfs_has_logv2(log->l_mp)) {
		union xlog_in_core2 *xhdr = (union xlog_in_core2 *)rhead;
		int		i;
		int		xheads;

		xheads = DIV_ROUND_UP(size, XLOG_HEADER_CYCLE_SIZE);

		for (i = 1; i < xheads; i++) {
			crc = crc32c(crc, &xhdr[i].hic_xheader,
				     sizeof(struct xlog_rec_ext_header));
		}
	}

	 
	crc = crc32c(crc, dp, size);

	return xfs_end_cksum(crc);
}

static void
xlog_bio_end_io(
	struct bio		*bio)
{
	struct xlog_in_core	*iclog = bio->bi_private;

	queue_work(iclog->ic_log->l_ioend_workqueue,
		   &iclog->ic_end_io_work);
}

static int
xlog_map_iclog_data(
	struct bio		*bio,
	void			*data,
	size_t			count)
{
	do {
		struct page	*page = kmem_to_page(data);
		unsigned int	off = offset_in_page(data);
		size_t		len = min_t(size_t, count, PAGE_SIZE - off);

		if (bio_add_page(bio, page, len, off) != len)
			return -EIO;

		data += len;
		count -= len;
	} while (count);

	return 0;
}

STATIC void
xlog_write_iclog(
	struct xlog		*log,
	struct xlog_in_core	*iclog,
	uint64_t		bno,
	unsigned int		count)
{
	ASSERT(bno < log->l_logBBsize);
	trace_xlog_iclog_write(iclog, _RET_IP_);

	 
	down(&iclog->ic_sema);
	if (xlog_is_shutdown(log)) {
		 
		xlog_state_done_syncing(iclog);
		up(&iclog->ic_sema);
		return;
	}

	 
	bio_init(&iclog->ic_bio, log->l_targ->bt_bdev, iclog->ic_bvec,
		 howmany(count, PAGE_SIZE),
		 REQ_OP_WRITE | REQ_META | REQ_SYNC | REQ_IDLE);
	iclog->ic_bio.bi_iter.bi_sector = log->l_logBBstart + bno;
	iclog->ic_bio.bi_end_io = xlog_bio_end_io;
	iclog->ic_bio.bi_private = iclog;

	if (iclog->ic_flags & XLOG_ICL_NEED_FLUSH) {
		iclog->ic_bio.bi_opf |= REQ_PREFLUSH;
		 
		if (log->l_targ != log->l_mp->m_ddev_targp &&
		    blkdev_issue_flush(log->l_mp->m_ddev_targp->bt_bdev)) {
			xlog_force_shutdown(log, SHUTDOWN_LOG_IO_ERROR);
			return;
		}
	}
	if (iclog->ic_flags & XLOG_ICL_NEED_FUA)
		iclog->ic_bio.bi_opf |= REQ_FUA;

	iclog->ic_flags &= ~(XLOG_ICL_NEED_FLUSH | XLOG_ICL_NEED_FUA);

	if (xlog_map_iclog_data(&iclog->ic_bio, iclog->ic_data, count)) {
		xlog_force_shutdown(log, SHUTDOWN_LOG_IO_ERROR);
		return;
	}
	if (is_vmalloc_addr(iclog->ic_data))
		flush_kernel_vmap_range(iclog->ic_data, count);

	 
	if (bno + BTOBB(count) > log->l_logBBsize) {
		struct bio *split;

		split = bio_split(&iclog->ic_bio, log->l_logBBsize - bno,
				  GFP_NOIO, &fs_bio_set);
		bio_chain(split, &iclog->ic_bio);
		submit_bio(split);

		 
		iclog->ic_bio.bi_iter.bi_sector = log->l_logBBstart;
	}

	submit_bio(&iclog->ic_bio);
}

 
static void
xlog_split_iclog(
	struct xlog		*log,
	void			*data,
	uint64_t		bno,
	unsigned int		count)
{
	unsigned int		split_offset = BBTOB(log->l_logBBsize - bno);
	unsigned int		i;

	for (i = split_offset; i < count; i += BBSIZE) {
		uint32_t cycle = get_unaligned_be32(data + i);

		if (++cycle == XLOG_HEADER_MAGIC_NUM)
			cycle++;
		put_unaligned_be32(cycle, data + i);
	}
}

static int
xlog_calc_iclog_size(
	struct xlog		*log,
	struct xlog_in_core	*iclog,
	uint32_t		*roundoff)
{
	uint32_t		count_init, count;

	 
	count_init = log->l_iclog_hsize + iclog->ic_offset;
	count = roundup(count_init, log->l_iclog_roundoff);

	*roundoff = count - count_init;

	ASSERT(count >= count_init);
	ASSERT(*roundoff < log->l_iclog_roundoff);
	return count;
}

 
STATIC void
xlog_sync(
	struct xlog		*log,
	struct xlog_in_core	*iclog,
	struct xlog_ticket	*ticket)
{
	unsigned int		count;		 
	unsigned int		roundoff;        
	uint64_t		bno;
	unsigned int		size;

	ASSERT(atomic_read(&iclog->ic_refcnt) == 0);
	trace_xlog_iclog_sync(iclog, _RET_IP_);

	count = xlog_calc_iclog_size(log, iclog, &roundoff);

	 
	if (ticket) {
		ticket->t_curr_res -= roundoff;
	} else {
		xlog_grant_add_space(log, &log->l_reserve_head.grant, roundoff);
		xlog_grant_add_space(log, &log->l_write_head.grant, roundoff);
	}

	 
	xlog_pack_data(log, iclog, roundoff);

	 
	size = iclog->ic_offset;
	if (xfs_has_logv2(log->l_mp))
		size += roundoff;
	iclog->ic_header.h_len = cpu_to_be32(size);

	XFS_STATS_INC(log->l_mp, xs_log_writes);
	XFS_STATS_ADD(log->l_mp, xs_log_blocks, BTOBB(count));

	bno = BLOCK_LSN(be64_to_cpu(iclog->ic_header.h_lsn));

	 
	if (bno + BTOBB(count) > log->l_logBBsize)
		xlog_split_iclog(log, &iclog->ic_header, bno, count);

	 
	iclog->ic_header.h_crc = xlog_cksum(log, &iclog->ic_header,
					    iclog->ic_datap, size);
	 
#ifdef DEBUG
	if (XFS_TEST_ERROR(false, log->l_mp, XFS_ERRTAG_LOG_BAD_CRC)) {
		iclog->ic_header.h_crc &= cpu_to_le32(0xAAAAAAAA);
		iclog->ic_fail_crc = true;
		xfs_warn(log->l_mp,
	"Intentionally corrupted log record at LSN 0x%llx. Shutdown imminent.",
			 be64_to_cpu(iclog->ic_header.h_lsn));
	}
#endif
	xlog_verify_iclog(log, iclog, count);
	xlog_write_iclog(log, iclog, bno, count);
}

 
STATIC void
xlog_dealloc_log(
	struct xlog	*log)
{
	xlog_in_core_t	*iclog, *next_iclog;
	int		i;

	 
	xlog_cil_destroy(log);

	iclog = log->l_iclog;
	for (i = 0; i < log->l_iclog_bufs; i++) {
		next_iclog = iclog->ic_next;
		kmem_free(iclog->ic_data);
		kmem_free(iclog);
		iclog = next_iclog;
	}

	log->l_mp->m_log = NULL;
	destroy_workqueue(log->l_ioend_workqueue);
	kmem_free(log);
}

 
static inline void
xlog_state_finish_copy(
	struct xlog		*log,
	struct xlog_in_core	*iclog,
	int			record_cnt,
	int			copy_bytes)
{
	lockdep_assert_held(&log->l_icloglock);

	be32_add_cpu(&iclog->ic_header.h_num_logops, record_cnt);
	iclog->ic_offset += copy_bytes;
}

 
void
xlog_print_tic_res(
	struct xfs_mount	*mp,
	struct xlog_ticket	*ticket)
{
	xfs_warn(mp, "ticket reservation summary:");
	xfs_warn(mp, "  unit res    = %d bytes", ticket->t_unit_res);
	xfs_warn(mp, "  current res = %d bytes", ticket->t_curr_res);
	xfs_warn(mp, "  original count  = %d", ticket->t_ocnt);
	xfs_warn(mp, "  remaining count = %d", ticket->t_cnt);
}

 
void
xlog_print_trans(
	struct xfs_trans	*tp)
{
	struct xfs_mount	*mp = tp->t_mountp;
	struct xfs_log_item	*lip;

	 
	xfs_warn(mp, "transaction summary:");
	xfs_warn(mp, "  log res   = %d", tp->t_log_res);
	xfs_warn(mp, "  log count = %d", tp->t_log_count);
	xfs_warn(mp, "  flags     = 0x%x", tp->t_flags);

	xlog_print_tic_res(mp, tp->t_ticket);

	 
	list_for_each_entry(lip, &tp->t_items, li_trans) {
		struct xfs_log_vec	*lv = lip->li_lv;
		struct xfs_log_iovec	*vec;
		int			i;

		xfs_warn(mp, "log item: ");
		xfs_warn(mp, "  type	= 0x%x", lip->li_type);
		xfs_warn(mp, "  flags	= 0x%lx", lip->li_flags);
		if (!lv)
			continue;
		xfs_warn(mp, "  niovecs	= %d", lv->lv_niovecs);
		xfs_warn(mp, "  size	= %d", lv->lv_size);
		xfs_warn(mp, "  bytes	= %d", lv->lv_bytes);
		xfs_warn(mp, "  buf len	= %d", lv->lv_buf_len);

		 
		vec = lv->lv_iovecp;
		for (i = 0; i < lv->lv_niovecs; i++) {
			int dumplen = min(vec->i_len, 32);

			xfs_warn(mp, "  iovec[%d]", i);
			xfs_warn(mp, "    type	= 0x%x", vec->i_type);
			xfs_warn(mp, "    len	= %d", vec->i_len);
			xfs_warn(mp, "    first %d bytes of iovec[%d]:", dumplen, i);
			xfs_hex_dump(vec->i_addr, dumplen);

			vec++;
		}
	}
}

static inline void
xlog_write_iovec(
	struct xlog_in_core	*iclog,
	uint32_t		*log_offset,
	void			*data,
	uint32_t		write_len,
	int			*bytes_left,
	uint32_t		*record_cnt,
	uint32_t		*data_cnt)
{
	ASSERT(*log_offset < iclog->ic_log->l_iclog_size);
	ASSERT(*log_offset % sizeof(int32_t) == 0);
	ASSERT(write_len % sizeof(int32_t) == 0);

	memcpy(iclog->ic_datap + *log_offset, data, write_len);
	*log_offset += write_len;
	*bytes_left -= write_len;
	(*record_cnt)++;
	*data_cnt += write_len;
}

 
static void
xlog_write_full(
	struct xfs_log_vec	*lv,
	struct xlog_ticket	*ticket,
	struct xlog_in_core	*iclog,
	uint32_t		*log_offset,
	uint32_t		*len,
	uint32_t		*record_cnt,
	uint32_t		*data_cnt)
{
	int			index;

	ASSERT(*log_offset + *len <= iclog->ic_size ||
		iclog->ic_state == XLOG_STATE_WANT_SYNC);

	 
	for (index = 0; index < lv->lv_niovecs; index++) {
		struct xfs_log_iovec	*reg = &lv->lv_iovecp[index];
		struct xlog_op_header	*ophdr = reg->i_addr;

		ophdr->oh_tid = cpu_to_be32(ticket->t_tid);
		xlog_write_iovec(iclog, log_offset, reg->i_addr,
				reg->i_len, len, record_cnt, data_cnt);
	}
}

static int
xlog_write_get_more_iclog_space(
	struct xlog_ticket	*ticket,
	struct xlog_in_core	**iclogp,
	uint32_t		*log_offset,
	uint32_t		len,
	uint32_t		*record_cnt,
	uint32_t		*data_cnt)
{
	struct xlog_in_core	*iclog = *iclogp;
	struct xlog		*log = iclog->ic_log;
	int			error;

	spin_lock(&log->l_icloglock);
	ASSERT(iclog->ic_state == XLOG_STATE_WANT_SYNC);
	xlog_state_finish_copy(log, iclog, *record_cnt, *data_cnt);
	error = xlog_state_release_iclog(log, iclog, ticket);
	spin_unlock(&log->l_icloglock);
	if (error)
		return error;

	error = xlog_state_get_iclog_space(log, len, &iclog, ticket,
					log_offset);
	if (error)
		return error;
	*record_cnt = 0;
	*data_cnt = 0;
	*iclogp = iclog;
	return 0;
}

 
static int
xlog_write_partial(
	struct xfs_log_vec	*lv,
	struct xlog_ticket	*ticket,
	struct xlog_in_core	**iclogp,
	uint32_t		*log_offset,
	uint32_t		*len,
	uint32_t		*record_cnt,
	uint32_t		*data_cnt)
{
	struct xlog_in_core	*iclog = *iclogp;
	struct xlog_op_header	*ophdr;
	int			index = 0;
	uint32_t		rlen;
	int			error;

	 
	for (index = 0; index < lv->lv_niovecs; index++) {
		struct xfs_log_iovec	*reg = &lv->lv_iovecp[index];
		uint32_t		reg_offset = 0;

		 
		if (iclog->ic_size - *log_offset <=
					sizeof(struct xlog_op_header)) {
			error = xlog_write_get_more_iclog_space(ticket,
					&iclog, log_offset, *len, record_cnt,
					data_cnt);
			if (error)
				return error;
		}

		ophdr = reg->i_addr;
		rlen = min_t(uint32_t, reg->i_len, iclog->ic_size - *log_offset);

		ophdr->oh_tid = cpu_to_be32(ticket->t_tid);
		ophdr->oh_len = cpu_to_be32(rlen - sizeof(struct xlog_op_header));
		if (rlen != reg->i_len)
			ophdr->oh_flags |= XLOG_CONTINUE_TRANS;

		xlog_write_iovec(iclog, log_offset, reg->i_addr,
				rlen, len, record_cnt, data_cnt);

		 
		if (rlen == reg->i_len)
			continue;

		 
		do {
			 
			error = xlog_write_get_more_iclog_space(ticket,
					&iclog, log_offset,
					*len + sizeof(struct xlog_op_header),
					record_cnt, data_cnt);
			if (error)
				return error;

			ophdr = iclog->ic_datap + *log_offset;
			ophdr->oh_tid = cpu_to_be32(ticket->t_tid);
			ophdr->oh_clientid = XFS_TRANSACTION;
			ophdr->oh_res2 = 0;
			ophdr->oh_flags = XLOG_WAS_CONT_TRANS;

			ticket->t_curr_res -= sizeof(struct xlog_op_header);
			*log_offset += sizeof(struct xlog_op_header);
			*data_cnt += sizeof(struct xlog_op_header);

			 
			reg_offset += rlen;
			rlen = reg->i_len - reg_offset;
			if (rlen <= iclog->ic_size - *log_offset)
				ophdr->oh_flags |= XLOG_END_TRANS;
			else
				ophdr->oh_flags |= XLOG_CONTINUE_TRANS;

			rlen = min_t(uint32_t, rlen, iclog->ic_size - *log_offset);
			ophdr->oh_len = cpu_to_be32(rlen);

			xlog_write_iovec(iclog, log_offset,
					reg->i_addr + reg_offset,
					rlen, len, record_cnt, data_cnt);

		} while (ophdr->oh_flags & XLOG_CONTINUE_TRANS);
	}

	 
	*iclogp = iclog;
	return 0;
}

 
int
xlog_write(
	struct xlog		*log,
	struct xfs_cil_ctx	*ctx,
	struct list_head	*lv_chain,
	struct xlog_ticket	*ticket,
	uint32_t		len)

{
	struct xlog_in_core	*iclog = NULL;
	struct xfs_log_vec	*lv;
	uint32_t		record_cnt = 0;
	uint32_t		data_cnt = 0;
	int			error = 0;
	int			log_offset;

	if (ticket->t_curr_res < 0) {
		xfs_alert_tag(log->l_mp, XFS_PTAG_LOGRES,
		     "ctx ticket reservation ran out. Need to up reservation");
		xlog_print_tic_res(log->l_mp, ticket);
		xlog_force_shutdown(log, SHUTDOWN_LOG_IO_ERROR);
	}

	error = xlog_state_get_iclog_space(log, len, &iclog, ticket,
					   &log_offset);
	if (error)
		return error;

	ASSERT(log_offset <= iclog->ic_size - 1);

	 
	if (ctx)
		xlog_cil_set_ctx_write_state(ctx, iclog);

	list_for_each_entry(lv, lv_chain, lv_list) {
		 
		if (lv->lv_niovecs &&
		    lv->lv_bytes > iclog->ic_size - log_offset) {
			error = xlog_write_partial(lv, ticket, &iclog,
					&log_offset, &len, &record_cnt,
					&data_cnt);
			if (error) {
				 
				return error;
			}
		} else {
			xlog_write_full(lv, ticket, iclog, &log_offset,
					 &len, &record_cnt, &data_cnt);
		}
	}
	ASSERT(len == 0);

	 
	spin_lock(&log->l_icloglock);
	xlog_state_finish_copy(log, iclog, record_cnt, 0);
	error = xlog_state_release_iclog(log, iclog, ticket);
	spin_unlock(&log->l_icloglock);

	return error;
}

static void
xlog_state_activate_iclog(
	struct xlog_in_core	*iclog,
	int			*iclogs_changed)
{
	ASSERT(list_empty_careful(&iclog->ic_callbacks));
	trace_xlog_iclog_activate(iclog, _RET_IP_);

	 
	if (*iclogs_changed == 0 &&
	    iclog->ic_header.h_num_logops == cpu_to_be32(XLOG_COVER_OPS)) {
		*iclogs_changed = 1;
	} else {
		 
		*iclogs_changed = 2;
	}

	iclog->ic_state	= XLOG_STATE_ACTIVE;
	iclog->ic_offset = 0;
	iclog->ic_header.h_num_logops = 0;
	memset(iclog->ic_header.h_cycle_data, 0,
		sizeof(iclog->ic_header.h_cycle_data));
	iclog->ic_header.h_lsn = 0;
	iclog->ic_header.h_tail_lsn = 0;
}

 
static void
xlog_state_activate_iclogs(
	struct xlog		*log,
	int			*iclogs_changed)
{
	struct xlog_in_core	*iclog = log->l_iclog;

	do {
		if (iclog->ic_state == XLOG_STATE_DIRTY)
			xlog_state_activate_iclog(iclog, iclogs_changed);
		 
		else if (iclog->ic_state != XLOG_STATE_ACTIVE)
			break;
	} while ((iclog = iclog->ic_next) != log->l_iclog);
}

static int
xlog_covered_state(
	int			prev_state,
	int			iclogs_changed)
{
	 
	switch (prev_state) {
	case XLOG_STATE_COVER_IDLE:
		if (iclogs_changed == 1)
			return XLOG_STATE_COVER_IDLE;
		fallthrough;
	case XLOG_STATE_COVER_NEED:
	case XLOG_STATE_COVER_NEED2:
		break;
	case XLOG_STATE_COVER_DONE:
		if (iclogs_changed == 1)
			return XLOG_STATE_COVER_NEED2;
		break;
	case XLOG_STATE_COVER_DONE2:
		if (iclogs_changed == 1)
			return XLOG_STATE_COVER_IDLE;
		break;
	default:
		ASSERT(0);
	}

	return XLOG_STATE_COVER_NEED;
}

STATIC void
xlog_state_clean_iclog(
	struct xlog		*log,
	struct xlog_in_core	*dirty_iclog)
{
	int			iclogs_changed = 0;

	trace_xlog_iclog_clean(dirty_iclog, _RET_IP_);

	dirty_iclog->ic_state = XLOG_STATE_DIRTY;

	xlog_state_activate_iclogs(log, &iclogs_changed);
	wake_up_all(&dirty_iclog->ic_force_wait);

	if (iclogs_changed) {
		log->l_covered_state = xlog_covered_state(log->l_covered_state,
				iclogs_changed);
	}
}

STATIC xfs_lsn_t
xlog_get_lowest_lsn(
	struct xlog		*log)
{
	struct xlog_in_core	*iclog = log->l_iclog;
	xfs_lsn_t		lowest_lsn = 0, lsn;

	do {
		if (iclog->ic_state == XLOG_STATE_ACTIVE ||
		    iclog->ic_state == XLOG_STATE_DIRTY)
			continue;

		lsn = be64_to_cpu(iclog->ic_header.h_lsn);
		if ((lsn && !lowest_lsn) || XFS_LSN_CMP(lsn, lowest_lsn) < 0)
			lowest_lsn = lsn;
	} while ((iclog = iclog->ic_next) != log->l_iclog);

	return lowest_lsn;
}

 
static void
xlog_state_set_callback(
	struct xlog		*log,
	struct xlog_in_core	*iclog,
	xfs_lsn_t		header_lsn)
{
	trace_xlog_iclog_callback(iclog, _RET_IP_);
	iclog->ic_state = XLOG_STATE_CALLBACK;

	ASSERT(XFS_LSN_CMP(atomic64_read(&log->l_last_sync_lsn),
			   header_lsn) <= 0);

	if (list_empty_careful(&iclog->ic_callbacks))
		return;

	atomic64_set(&log->l_last_sync_lsn, header_lsn);
	xlog_grant_push_ail(log, 0);
}

 
static bool
xlog_state_iodone_process_iclog(
	struct xlog		*log,
	struct xlog_in_core	*iclog)
{
	xfs_lsn_t		lowest_lsn;
	xfs_lsn_t		header_lsn;

	switch (iclog->ic_state) {
	case XLOG_STATE_ACTIVE:
	case XLOG_STATE_DIRTY:
		 
		return false;
	case XLOG_STATE_DONE_SYNC:
		 
		header_lsn = be64_to_cpu(iclog->ic_header.h_lsn);
		lowest_lsn = xlog_get_lowest_lsn(log);
		if (lowest_lsn && XFS_LSN_CMP(lowest_lsn, header_lsn) < 0)
			return false;
		xlog_state_set_callback(log, iclog, header_lsn);
		return false;
	default:
		 
		return true;
	}
}

 
static bool
xlog_state_do_iclog_callbacks(
	struct xlog		*log)
		__releases(&log->l_icloglock)
		__acquires(&log->l_icloglock)
{
	struct xlog_in_core	*first_iclog = log->l_iclog;
	struct xlog_in_core	*iclog = first_iclog;
	bool			ran_callback = false;

	do {
		LIST_HEAD(cb_list);

		if (xlog_state_iodone_process_iclog(log, iclog))
			break;
		if (iclog->ic_state != XLOG_STATE_CALLBACK) {
			iclog = iclog->ic_next;
			continue;
		}
		list_splice_init(&iclog->ic_callbacks, &cb_list);
		spin_unlock(&log->l_icloglock);

		trace_xlog_iclog_callbacks_start(iclog, _RET_IP_);
		xlog_cil_process_committed(&cb_list);
		trace_xlog_iclog_callbacks_done(iclog, _RET_IP_);
		ran_callback = true;

		spin_lock(&log->l_icloglock);
		xlog_state_clean_iclog(log, iclog);
		iclog = iclog->ic_next;
	} while (iclog != first_iclog);

	return ran_callback;
}


 
STATIC void
xlog_state_do_callback(
	struct xlog		*log)
{
	int			flushcnt = 0;
	int			repeats = 0;

	spin_lock(&log->l_icloglock);
	while (xlog_state_do_iclog_callbacks(log)) {
		if (xlog_is_shutdown(log))
			break;

		if (++repeats > 5000) {
			flushcnt += repeats;
			repeats = 0;
			xfs_warn(log->l_mp,
				"%s: possible infinite loop (%d iterations)",
				__func__, flushcnt);
		}
	}

	if (log->l_iclog->ic_state == XLOG_STATE_ACTIVE)
		wake_up_all(&log->l_flush_wait);

	spin_unlock(&log->l_icloglock);
}


 
STATIC void
xlog_state_done_syncing(
	struct xlog_in_core	*iclog)
{
	struct xlog		*log = iclog->ic_log;

	spin_lock(&log->l_icloglock);
	ASSERT(atomic_read(&iclog->ic_refcnt) == 0);
	trace_xlog_iclog_sync_done(iclog, _RET_IP_);

	 
	if (!xlog_is_shutdown(log)) {
		ASSERT(iclog->ic_state == XLOG_STATE_SYNCING);
		iclog->ic_state = XLOG_STATE_DONE_SYNC;
	}

	 
	wake_up_all(&iclog->ic_write_wait);
	spin_unlock(&log->l_icloglock);
	xlog_state_do_callback(log);
}

 
STATIC int
xlog_state_get_iclog_space(
	struct xlog		*log,
	int			len,
	struct xlog_in_core	**iclogp,
	struct xlog_ticket	*ticket,
	int			*logoffsetp)
{
	int		  log_offset;
	xlog_rec_header_t *head;
	xlog_in_core_t	  *iclog;

restart:
	spin_lock(&log->l_icloglock);
	if (xlog_is_shutdown(log)) {
		spin_unlock(&log->l_icloglock);
		return -EIO;
	}

	iclog = log->l_iclog;
	if (iclog->ic_state != XLOG_STATE_ACTIVE) {
		XFS_STATS_INC(log->l_mp, xs_log_noiclogs);

		 
		xlog_wait(&log->l_flush_wait, &log->l_icloglock);
		goto restart;
	}

	head = &iclog->ic_header;

	atomic_inc(&iclog->ic_refcnt);	 
	log_offset = iclog->ic_offset;

	trace_xlog_iclog_get_space(iclog, _RET_IP_);

	 
	if (log_offset == 0) {
		ticket->t_curr_res -= log->l_iclog_hsize;
		head->h_cycle = cpu_to_be32(log->l_curr_cycle);
		head->h_lsn = cpu_to_be64(
			xlog_assign_lsn(log->l_curr_cycle, log->l_curr_block));
		ASSERT(log->l_curr_block >= 0);
	}

	 
	if (iclog->ic_size - iclog->ic_offset < 2*sizeof(xlog_op_header_t)) {
		int		error = 0;

		xlog_state_switch_iclogs(log, iclog, iclog->ic_size);

		 
		if (!atomic_add_unless(&iclog->ic_refcnt, -1, 1))
			error = xlog_state_release_iclog(log, iclog, ticket);
		spin_unlock(&log->l_icloglock);
		if (error)
			return error;
		goto restart;
	}

	 
	if (len <= iclog->ic_size - iclog->ic_offset)
		iclog->ic_offset += len;
	else
		xlog_state_switch_iclogs(log, iclog, iclog->ic_size);
	*iclogp = iclog;

	ASSERT(iclog->ic_offset <= iclog->ic_size);
	spin_unlock(&log->l_icloglock);

	*logoffsetp = log_offset;
	return 0;
}

 
void
xfs_log_ticket_regrant(
	struct xlog		*log,
	struct xlog_ticket	*ticket)
{
	trace_xfs_log_ticket_regrant(log, ticket);

	if (ticket->t_cnt > 0)
		ticket->t_cnt--;

	xlog_grant_sub_space(log, &log->l_reserve_head.grant,
					ticket->t_curr_res);
	xlog_grant_sub_space(log, &log->l_write_head.grant,
					ticket->t_curr_res);
	ticket->t_curr_res = ticket->t_unit_res;

	trace_xfs_log_ticket_regrant_sub(log, ticket);

	 
	if (!ticket->t_cnt) {
		xlog_grant_add_space(log, &log->l_reserve_head.grant,
				     ticket->t_unit_res);
		trace_xfs_log_ticket_regrant_exit(log, ticket);

		ticket->t_curr_res = ticket->t_unit_res;
	}

	xfs_log_ticket_put(ticket);
}

 
void
xfs_log_ticket_ungrant(
	struct xlog		*log,
	struct xlog_ticket	*ticket)
{
	int			bytes;

	trace_xfs_log_ticket_ungrant(log, ticket);

	if (ticket->t_cnt > 0)
		ticket->t_cnt--;

	trace_xfs_log_ticket_ungrant_sub(log, ticket);

	 
	bytes = ticket->t_curr_res;
	if (ticket->t_cnt > 0) {
		ASSERT(ticket->t_flags & XLOG_TIC_PERM_RESERV);
		bytes += ticket->t_unit_res*ticket->t_cnt;
	}

	xlog_grant_sub_space(log, &log->l_reserve_head.grant, bytes);
	xlog_grant_sub_space(log, &log->l_write_head.grant, bytes);

	trace_xfs_log_ticket_ungrant_exit(log, ticket);

	xfs_log_space_wake(log->l_mp);
	xfs_log_ticket_put(ticket);
}

 
void
xlog_state_switch_iclogs(
	struct xlog		*log,
	struct xlog_in_core	*iclog,
	int			eventual_size)
{
	ASSERT(iclog->ic_state == XLOG_STATE_ACTIVE);
	assert_spin_locked(&log->l_icloglock);
	trace_xlog_iclog_switch(iclog, _RET_IP_);

	if (!eventual_size)
		eventual_size = iclog->ic_offset;
	iclog->ic_state = XLOG_STATE_WANT_SYNC;
	iclog->ic_header.h_prev_block = cpu_to_be32(log->l_prev_block);
	log->l_prev_block = log->l_curr_block;
	log->l_prev_cycle = log->l_curr_cycle;

	 
	log->l_curr_block += BTOBB(eventual_size)+BTOBB(log->l_iclog_hsize);

	 
	if (log->l_iclog_roundoff > BBSIZE) {
		uint32_t sunit_bb = BTOBB(log->l_iclog_roundoff);
		log->l_curr_block = roundup(log->l_curr_block, sunit_bb);
	}

	if (log->l_curr_block >= log->l_logBBsize) {
		 
		log->l_curr_block -= log->l_logBBsize;
		ASSERT(log->l_curr_block >= 0);
		smp_wmb();
		log->l_curr_cycle++;
		if (log->l_curr_cycle == XLOG_HEADER_MAGIC_NUM)
			log->l_curr_cycle++;
	}
	ASSERT(iclog == log->l_iclog);
	log->l_iclog = iclog->ic_next;
}

 
static int
xlog_force_and_check_iclog(
	struct xlog_in_core	*iclog,
	bool			*completed)
{
	xfs_lsn_t		lsn = be64_to_cpu(iclog->ic_header.h_lsn);
	int			error;

	*completed = false;
	error = xlog_force_iclog(iclog);
	if (error)
		return error;

	 
	if (be64_to_cpu(iclog->ic_header.h_lsn) != lsn)
		*completed = true;
	return 0;
}

 
int
xfs_log_force(
	struct xfs_mount	*mp,
	uint			flags)
{
	struct xlog		*log = mp->m_log;
	struct xlog_in_core	*iclog;

	XFS_STATS_INC(mp, xs_log_force);
	trace_xfs_log_force(mp, 0, _RET_IP_);

	xlog_cil_force(log);

	spin_lock(&log->l_icloglock);
	if (xlog_is_shutdown(log))
		goto out_error;

	iclog = log->l_iclog;
	trace_xlog_iclog_force(iclog, _RET_IP_);

	if (iclog->ic_state == XLOG_STATE_DIRTY ||
	    (iclog->ic_state == XLOG_STATE_ACTIVE &&
	     atomic_read(&iclog->ic_refcnt) == 0 && iclog->ic_offset == 0)) {
		 
		iclog = iclog->ic_prev;
	} else if (iclog->ic_state == XLOG_STATE_ACTIVE) {
		if (atomic_read(&iclog->ic_refcnt) == 0) {
			 
			bool	completed;

			if (xlog_force_and_check_iclog(iclog, &completed))
				goto out_error;

			if (completed)
				goto out_unlock;
		} else {
			 
			xlog_state_switch_iclogs(log, iclog, 0);
		}
	}

	 
	if (iclog->ic_state == XLOG_STATE_WANT_SYNC)
		iclog->ic_flags |= XLOG_ICL_NEED_FLUSH | XLOG_ICL_NEED_FUA;

	if (flags & XFS_LOG_SYNC)
		return xlog_wait_on_iclog(iclog);
out_unlock:
	spin_unlock(&log->l_icloglock);
	return 0;
out_error:
	spin_unlock(&log->l_icloglock);
	return -EIO;
}

 
static int
xlog_force_lsn(
	struct xlog		*log,
	xfs_lsn_t		lsn,
	uint			flags,
	int			*log_flushed,
	bool			already_slept)
{
	struct xlog_in_core	*iclog;
	bool			completed;

	spin_lock(&log->l_icloglock);
	if (xlog_is_shutdown(log))
		goto out_error;

	iclog = log->l_iclog;
	while (be64_to_cpu(iclog->ic_header.h_lsn) != lsn) {
		trace_xlog_iclog_force_lsn(iclog, _RET_IP_);
		iclog = iclog->ic_next;
		if (iclog == log->l_iclog)
			goto out_unlock;
	}

	switch (iclog->ic_state) {
	case XLOG_STATE_ACTIVE:
		 
		if (!already_slept &&
		    (iclog->ic_prev->ic_state == XLOG_STATE_WANT_SYNC ||
		     iclog->ic_prev->ic_state == XLOG_STATE_SYNCING)) {
			xlog_wait(&iclog->ic_prev->ic_write_wait,
					&log->l_icloglock);
			return -EAGAIN;
		}
		if (xlog_force_and_check_iclog(iclog, &completed))
			goto out_error;
		if (log_flushed)
			*log_flushed = 1;
		if (completed)
			goto out_unlock;
		break;
	case XLOG_STATE_WANT_SYNC:
		 
		iclog->ic_flags |= XLOG_ICL_NEED_FLUSH | XLOG_ICL_NEED_FUA;
		break;
	default:
		 
		break;
	}

	if (flags & XFS_LOG_SYNC)
		return xlog_wait_on_iclog(iclog);
out_unlock:
	spin_unlock(&log->l_icloglock);
	return 0;
out_error:
	spin_unlock(&log->l_icloglock);
	return -EIO;
}

 
int
xfs_log_force_seq(
	struct xfs_mount	*mp,
	xfs_csn_t		seq,
	uint			flags,
	int			*log_flushed)
{
	struct xlog		*log = mp->m_log;
	xfs_lsn_t		lsn;
	int			ret;
	ASSERT(seq != 0);

	XFS_STATS_INC(mp, xs_log_force);
	trace_xfs_log_force(mp, seq, _RET_IP_);

	lsn = xlog_cil_force_seq(log, seq);
	if (lsn == NULLCOMMITLSN)
		return 0;

	ret = xlog_force_lsn(log, lsn, flags, log_flushed, false);
	if (ret == -EAGAIN) {
		XFS_STATS_INC(mp, xs_log_force_sleep);
		ret = xlog_force_lsn(log, lsn, flags, log_flushed, true);
	}
	return ret;
}

 
void
xfs_log_ticket_put(
	xlog_ticket_t	*ticket)
{
	ASSERT(atomic_read(&ticket->t_ref) > 0);
	if (atomic_dec_and_test(&ticket->t_ref))
		kmem_cache_free(xfs_log_ticket_cache, ticket);
}

xlog_ticket_t *
xfs_log_ticket_get(
	xlog_ticket_t	*ticket)
{
	ASSERT(atomic_read(&ticket->t_ref) > 0);
	atomic_inc(&ticket->t_ref);
	return ticket;
}

 
static int
xlog_calc_unit_res(
	struct xlog		*log,
	int			unit_bytes,
	int			*niclogs)
{
	int			iclog_space;
	uint			num_headers;

	 

	 
	unit_bytes += sizeof(xlog_op_header_t);
	unit_bytes += sizeof(xfs_trans_header_t);

	 
	unit_bytes += sizeof(xlog_op_header_t);

	 
	iclog_space = log->l_iclog_size - log->l_iclog_hsize;
	num_headers = howmany(unit_bytes, iclog_space);

	 
	unit_bytes += sizeof(xlog_op_header_t) * num_headers;

	 
	while (!num_headers ||
	       howmany(unit_bytes, iclog_space) > num_headers) {
		unit_bytes += sizeof(xlog_op_header_t);
		num_headers++;
	}
	unit_bytes += log->l_iclog_hsize * num_headers;

	 
	unit_bytes += log->l_iclog_hsize;

	 
	unit_bytes += 2 * log->l_iclog_roundoff;

	if (niclogs)
		*niclogs = num_headers;
	return unit_bytes;
}

int
xfs_log_calc_unit_res(
	struct xfs_mount	*mp,
	int			unit_bytes)
{
	return xlog_calc_unit_res(mp->m_log, unit_bytes, NULL);
}

 
struct xlog_ticket *
xlog_ticket_alloc(
	struct xlog		*log,
	int			unit_bytes,
	int			cnt,
	bool			permanent)
{
	struct xlog_ticket	*tic;
	int			unit_res;

	tic = kmem_cache_zalloc(xfs_log_ticket_cache, GFP_NOFS | __GFP_NOFAIL);

	unit_res = xlog_calc_unit_res(log, unit_bytes, &tic->t_iclog_hdrs);

	atomic_set(&tic->t_ref, 1);
	tic->t_task		= current;
	INIT_LIST_HEAD(&tic->t_queue);
	tic->t_unit_res		= unit_res;
	tic->t_curr_res		= unit_res;
	tic->t_cnt		= cnt;
	tic->t_ocnt		= cnt;
	tic->t_tid		= get_random_u32();
	if (permanent)
		tic->t_flags |= XLOG_TIC_PERM_RESERV;

	return tic;
}

#if defined(DEBUG)
 
STATIC void
xlog_verify_grant_tail(
	struct xlog	*log)
{
	int		tail_cycle, tail_blocks;
	int		cycle, space;

	xlog_crack_grant_head(&log->l_write_head.grant, &cycle, &space);
	xlog_crack_atomic_lsn(&log->l_tail_lsn, &tail_cycle, &tail_blocks);
	if (tail_cycle != cycle) {
		if (cycle - 1 != tail_cycle &&
		    !test_and_set_bit(XLOG_TAIL_WARN, &log->l_opstate)) {
			xfs_alert_tag(log->l_mp, XFS_PTAG_LOGRES,
				"%s: cycle - 1 != tail_cycle", __func__);
		}

		if (space > BBTOB(tail_blocks) &&
		    !test_and_set_bit(XLOG_TAIL_WARN, &log->l_opstate)) {
			xfs_alert_tag(log->l_mp, XFS_PTAG_LOGRES,
				"%s: space > BBTOB(tail_blocks)", __func__);
		}
	}
}

 
STATIC void
xlog_verify_tail_lsn(
	struct xlog		*log,
	struct xlog_in_core	*iclog)
{
	xfs_lsn_t	tail_lsn = be64_to_cpu(iclog->ic_header.h_tail_lsn);
	int		blocks;

    if (CYCLE_LSN(tail_lsn) == log->l_prev_cycle) {
	blocks =
	    log->l_logBBsize - (log->l_prev_block - BLOCK_LSN(tail_lsn));
	if (blocks < BTOBB(iclog->ic_offset)+BTOBB(log->l_iclog_hsize))
		xfs_emerg(log->l_mp, "%s: ran out of log space", __func__);
    } else {
	ASSERT(CYCLE_LSN(tail_lsn)+1 == log->l_prev_cycle);

	if (BLOCK_LSN(tail_lsn) == log->l_prev_block)
		xfs_emerg(log->l_mp, "%s: tail wrapped", __func__);

	blocks = BLOCK_LSN(tail_lsn) - log->l_prev_block;
	if (blocks < BTOBB(iclog->ic_offset) + 1)
		xfs_emerg(log->l_mp, "%s: ran out of log space", __func__);
    }
}

 
STATIC void
xlog_verify_iclog(
	struct xlog		*log,
	struct xlog_in_core	*iclog,
	int			count)
{
	xlog_op_header_t	*ophead;
	xlog_in_core_t		*icptr;
	xlog_in_core_2_t	*xhdr;
	void			*base_ptr, *ptr, *p;
	ptrdiff_t		field_offset;
	uint8_t			clientid;
	int			len, i, j, k, op_len;
	int			idx;

	 
	spin_lock(&log->l_icloglock);
	icptr = log->l_iclog;
	for (i = 0; i < log->l_iclog_bufs; i++, icptr = icptr->ic_next)
		ASSERT(icptr);

	if (icptr != log->l_iclog)
		xfs_emerg(log->l_mp, "%s: corrupt iclog ring", __func__);
	spin_unlock(&log->l_icloglock);

	 
	if (iclog->ic_header.h_magicno != cpu_to_be32(XLOG_HEADER_MAGIC_NUM))
		xfs_emerg(log->l_mp, "%s: invalid magic num", __func__);

	base_ptr = ptr = &iclog->ic_header;
	p = &iclog->ic_header;
	for (ptr += BBSIZE; ptr < base_ptr + count; ptr += BBSIZE) {
		if (*(__be32 *)ptr == cpu_to_be32(XLOG_HEADER_MAGIC_NUM))
			xfs_emerg(log->l_mp, "%s: unexpected magic num",
				__func__);
	}

	 
	len = be32_to_cpu(iclog->ic_header.h_num_logops);
	base_ptr = ptr = iclog->ic_datap;
	ophead = ptr;
	xhdr = iclog->ic_data;
	for (i = 0; i < len; i++) {
		ophead = ptr;

		 
		p = &ophead->oh_clientid;
		field_offset = p - base_ptr;
		if (field_offset & 0x1ff) {
			clientid = ophead->oh_clientid;
		} else {
			idx = BTOBBT((void *)&ophead->oh_clientid - iclog->ic_datap);
			if (idx >= (XLOG_HEADER_CYCLE_SIZE / BBSIZE)) {
				j = idx / (XLOG_HEADER_CYCLE_SIZE / BBSIZE);
				k = idx % (XLOG_HEADER_CYCLE_SIZE / BBSIZE);
				clientid = xlog_get_client_id(
					xhdr[j].hic_xheader.xh_cycle_data[k]);
			} else {
				clientid = xlog_get_client_id(
					iclog->ic_header.h_cycle_data[idx]);
			}
		}
		if (clientid != XFS_TRANSACTION && clientid != XFS_LOG) {
			xfs_warn(log->l_mp,
				"%s: op %d invalid clientid %d op "PTR_FMT" offset 0x%lx",
				__func__, i, clientid, ophead,
				(unsigned long)field_offset);
		}

		 
		p = &ophead->oh_len;
		field_offset = p - base_ptr;
		if (field_offset & 0x1ff) {
			op_len = be32_to_cpu(ophead->oh_len);
		} else {
			idx = BTOBBT((void *)&ophead->oh_len - iclog->ic_datap);
			if (idx >= (XLOG_HEADER_CYCLE_SIZE / BBSIZE)) {
				j = idx / (XLOG_HEADER_CYCLE_SIZE / BBSIZE);
				k = idx % (XLOG_HEADER_CYCLE_SIZE / BBSIZE);
				op_len = be32_to_cpu(xhdr[j].hic_xheader.xh_cycle_data[k]);
			} else {
				op_len = be32_to_cpu(iclog->ic_header.h_cycle_data[idx]);
			}
		}
		ptr += sizeof(xlog_op_header_t) + op_len;
	}
}
#endif

 
bool
xlog_force_shutdown(
	struct xlog	*log,
	uint32_t	shutdown_flags)
{
	bool		log_error = (shutdown_flags & SHUTDOWN_LOG_IO_ERROR);

	if (!log)
		return false;

	 
	if (!log_error && !xlog_in_recovery(log))
		xfs_log_force(log->l_mp, XFS_LOG_SYNC);

	 
	spin_lock(&log->l_icloglock);
	if (test_and_set_bit(XLOG_IO_ERROR, &log->l_opstate)) {
		spin_unlock(&log->l_icloglock);
		return false;
	}
	spin_unlock(&log->l_icloglock);

	 
	if (!test_and_set_bit(XFS_OPSTATE_SHUTDOWN, &log->l_mp->m_opstate)) {
		xfs_alert_tag(log->l_mp, XFS_PTAG_SHUTDOWN_LOGERROR,
"Filesystem has been shut down due to log error (0x%x).",
				shutdown_flags);
		xfs_alert(log->l_mp,
"Please unmount the filesystem and rectify the problem(s).");
		if (xfs_error_level >= XFS_ERRLEVEL_HIGH)
			xfs_stack_trace();
	}

	 
	xlog_grant_head_wake_all(&log->l_reserve_head);
	xlog_grant_head_wake_all(&log->l_write_head);

	 
	spin_lock(&log->l_cilp->xc_push_lock);
	wake_up_all(&log->l_cilp->xc_start_wait);
	wake_up_all(&log->l_cilp->xc_commit_wait);
	spin_unlock(&log->l_cilp->xc_push_lock);

	spin_lock(&log->l_icloglock);
	xlog_state_shutdown_callbacks(log);
	spin_unlock(&log->l_icloglock);

	wake_up_var(&log->l_opstate);
	return log_error;
}

STATIC int
xlog_iclogs_empty(
	struct xlog	*log)
{
	xlog_in_core_t	*iclog;

	iclog = log->l_iclog;
	do {
		 
		if (iclog->ic_header.h_num_logops)
			return 0;
		iclog = iclog->ic_next;
	} while (iclog != log->l_iclog);
	return 1;
}

 
bool
xfs_log_check_lsn(
	struct xfs_mount	*mp,
	xfs_lsn_t		lsn)
{
	struct xlog		*log = mp->m_log;
	bool			valid;

	 
	if (xfs_has_norecovery(mp))
		return true;

	 
	if (lsn == NULLCOMMITLSN)
		return true;

	valid = xlog_valid_lsn(mp->m_log, lsn);

	 
	if (!valid) {
		spin_lock(&log->l_icloglock);
		xfs_warn(mp,
"Corruption warning: Metadata has LSN (%d:%d) ahead of current LSN (%d:%d). "
"Please unmount and run xfs_repair (>= v4.3) to resolve.",
			 CYCLE_LSN(lsn), BLOCK_LSN(lsn),
			 log->l_curr_cycle, log->l_curr_block);
		spin_unlock(&log->l_icloglock);
	}

	return valid;
}

 
void
xlog_use_incompat_feat(
	struct xlog		*log)
{
	down_read(&log->l_incompat_users);
}

 
void
xlog_drop_incompat_feat(
	struct xlog		*log)
{
	up_read(&log->l_incompat_users);
}
