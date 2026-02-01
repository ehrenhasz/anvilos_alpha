
 

#include "xfs.h"
#include "xfs_fs.h"
#include "xfs_format.h"
#include "xfs_log_format.h"
#include "xfs_shared.h"
#include "xfs_trans_resv.h"
#include "xfs_mount.h"
#include "xfs_extent_busy.h"
#include "xfs_trans.h"
#include "xfs_trans_priv.h"
#include "xfs_log.h"
#include "xfs_log_priv.h"
#include "xfs_trace.h"
#include "xfs_discard.h"

 
static struct xlog_ticket *
xlog_cil_ticket_alloc(
	struct xlog	*log)
{
	struct xlog_ticket *tic;

	tic = xlog_ticket_alloc(log, 0, 1, 0);

	 
	tic->t_curr_res = 0;
	tic->t_iclog_hdrs = 0;
	return tic;
}

static inline void
xlog_cil_set_iclog_hdr_count(struct xfs_cil *cil)
{
	struct xlog	*log = cil->xc_log;

	atomic_set(&cil->xc_iclog_hdrs,
		   (XLOG_CIL_BLOCKING_SPACE_LIMIT(log) /
			(log->l_iclog_size - log->l_iclog_hsize)));
}

 
static bool
xlog_item_in_current_chkpt(
	struct xfs_cil		*cil,
	struct xfs_log_item	*lip)
{
	if (test_bit(XLOG_CIL_EMPTY, &cil->xc_flags))
		return false;

	 
	return lip->li_seq == READ_ONCE(cil->xc_current_sequence);
}

bool
xfs_log_item_in_current_chkpt(
	struct xfs_log_item *lip)
{
	return xlog_item_in_current_chkpt(lip->li_log->l_cilp, lip);
}

 
static void xlog_cil_push_work(struct work_struct *work);

static struct xfs_cil_ctx *
xlog_cil_ctx_alloc(void)
{
	struct xfs_cil_ctx	*ctx;

	ctx = kmem_zalloc(sizeof(*ctx), KM_NOFS);
	INIT_LIST_HEAD(&ctx->committing);
	INIT_LIST_HEAD(&ctx->busy_extents.extent_list);
	INIT_LIST_HEAD(&ctx->log_items);
	INIT_LIST_HEAD(&ctx->lv_chain);
	INIT_WORK(&ctx->push_work, xlog_cil_push_work);
	return ctx;
}

 
static void
xlog_cil_push_pcp_aggregate(
	struct xfs_cil		*cil,
	struct xfs_cil_ctx	*ctx)
{
	struct xlog_cil_pcp	*cilpcp;
	int			cpu;

	for_each_cpu(cpu, &ctx->cil_pcpmask) {
		cilpcp = per_cpu_ptr(cil->xc_pcp, cpu);

		ctx->ticket->t_curr_res += cilpcp->space_reserved;
		cilpcp->space_reserved = 0;

		if (!list_empty(&cilpcp->busy_extents)) {
			list_splice_init(&cilpcp->busy_extents,
					&ctx->busy_extents.extent_list);
		}
		if (!list_empty(&cilpcp->log_items))
			list_splice_init(&cilpcp->log_items, &ctx->log_items);

		 
		cilpcp->space_used = 0;
	}
}

 
static void
xlog_cil_insert_pcp_aggregate(
	struct xfs_cil		*cil,
	struct xfs_cil_ctx	*ctx)
{
	struct xlog_cil_pcp	*cilpcp;
	int			cpu;
	int			count = 0;

	 
	if (!test_and_clear_bit(XLOG_CIL_PCP_SPACE, &cil->xc_flags))
		return;

	 
	for_each_cpu(cpu, &ctx->cil_pcpmask) {
		int	old, prev;

		cilpcp = per_cpu_ptr(cil->xc_pcp, cpu);
		do {
			old = cilpcp->space_used;
			prev = cmpxchg(&cilpcp->space_used, old, 0);
		} while (old != prev);
		count += old;
	}
	atomic_add(count, &ctx->space_used);
}

static void
xlog_cil_ctx_switch(
	struct xfs_cil		*cil,
	struct xfs_cil_ctx	*ctx)
{
	xlog_cil_set_iclog_hdr_count(cil);
	set_bit(XLOG_CIL_EMPTY, &cil->xc_flags);
	set_bit(XLOG_CIL_PCP_SPACE, &cil->xc_flags);
	ctx->sequence = ++cil->xc_current_sequence;
	ctx->cil = cil;
	cil->xc_ctx = ctx;
}

 
void
xlog_cil_init_post_recovery(
	struct xlog	*log)
{
	log->l_cilp->xc_ctx->ticket = xlog_cil_ticket_alloc(log);
	log->l_cilp->xc_ctx->sequence = 1;
	xlog_cil_set_iclog_hdr_count(log->l_cilp);
}

static inline int
xlog_cil_iovec_space(
	uint	niovecs)
{
	return round_up((sizeof(struct xfs_log_vec) +
					niovecs * sizeof(struct xfs_log_iovec)),
			sizeof(uint64_t));
}

 
static void
xlog_cil_alloc_shadow_bufs(
	struct xlog		*log,
	struct xfs_trans	*tp)
{
	struct xfs_log_item	*lip;

	list_for_each_entry(lip, &tp->t_items, li_trans) {
		struct xfs_log_vec *lv;
		int	niovecs = 0;
		int	nbytes = 0;
		int	buf_size;
		bool	ordered = false;

		 
		if (!test_bit(XFS_LI_DIRTY, &lip->li_flags))
			continue;

		 
		lip->li_ops->iop_size(lip, &niovecs, &nbytes);

		 
		if (niovecs == XFS_LOG_VEC_ORDERED) {
			ordered = true;
			niovecs = 0;
			nbytes = 0;
		}

		 
		nbytes += niovecs *
			(sizeof(uint64_t) + sizeof(struct xlog_op_header));
		nbytes = round_up(nbytes, sizeof(uint64_t));

		 
		buf_size = nbytes + xlog_cil_iovec_space(niovecs);

		 
		if (!lip->li_lv_shadow ||
		    buf_size > lip->li_lv_shadow->lv_size) {
			 
			kmem_free(lip->li_lv_shadow);
			lv = xlog_kvmalloc(buf_size);

			memset(lv, 0, xlog_cil_iovec_space(niovecs));

			INIT_LIST_HEAD(&lv->lv_list);
			lv->lv_item = lip;
			lv->lv_size = buf_size;
			if (ordered)
				lv->lv_buf_len = XFS_LOG_VEC_ORDERED;
			else
				lv->lv_iovecp = (struct xfs_log_iovec *)&lv[1];
			lip->li_lv_shadow = lv;
		} else {
			 
			lv = lip->li_lv_shadow;
			if (ordered)
				lv->lv_buf_len = XFS_LOG_VEC_ORDERED;
			else
				lv->lv_buf_len = 0;
			lv->lv_bytes = 0;
		}

		 
		lv->lv_niovecs = niovecs;

		 
		lv->lv_buf = (char *)lv + xlog_cil_iovec_space(niovecs);
	}

}

 
STATIC void
xfs_cil_prepare_item(
	struct xlog		*log,
	struct xfs_log_vec	*lv,
	struct xfs_log_vec	*old_lv,
	int			*diff_len)
{
	 
	if (lv->lv_buf_len != XFS_LOG_VEC_ORDERED)
		*diff_len += lv->lv_bytes;

	 
	if (!old_lv) {
		if (lv->lv_item->li_ops->iop_pin)
			lv->lv_item->li_ops->iop_pin(lv->lv_item);
		lv->lv_item->li_lv_shadow = NULL;
	} else if (old_lv != lv) {
		ASSERT(lv->lv_buf_len != XFS_LOG_VEC_ORDERED);

		*diff_len -= old_lv->lv_bytes;
		lv->lv_item->li_lv_shadow = old_lv;
	}

	 
	lv->lv_item->li_lv = lv;

	 
	if (!lv->lv_item->li_seq)
		lv->lv_item->li_seq = log->l_cilp->xc_ctx->sequence;
}

 
static void
xlog_cil_insert_format_items(
	struct xlog		*log,
	struct xfs_trans	*tp,
	int			*diff_len)
{
	struct xfs_log_item	*lip;

	 
	if (list_empty(&tp->t_items)) {
		ASSERT(0);
		return;
	}

	list_for_each_entry(lip, &tp->t_items, li_trans) {
		struct xfs_log_vec *lv;
		struct xfs_log_vec *old_lv = NULL;
		struct xfs_log_vec *shadow;
		bool	ordered = false;

		 
		if (!test_bit(XFS_LI_DIRTY, &lip->li_flags))
			continue;

		 
		shadow = lip->li_lv_shadow;
		if (shadow->lv_buf_len == XFS_LOG_VEC_ORDERED)
			ordered = true;

		 
		if (!shadow->lv_niovecs && !ordered)
			continue;

		 
		old_lv = lip->li_lv;
		if (lip->li_lv && shadow->lv_size <= lip->li_lv->lv_size) {
			 
			lv = lip->li_lv;

			if (ordered)
				goto insert;

			 
			*diff_len -= lv->lv_bytes;

			 
			lv->lv_niovecs = shadow->lv_niovecs;

			 
			lv->lv_buf_len = 0;
			lv->lv_bytes = 0;
			lv->lv_buf = (char *)lv +
					xlog_cil_iovec_space(lv->lv_niovecs);
		} else {
			 
			lv = shadow;
			lv->lv_item = lip;
			if (ordered) {
				 
				ASSERT(lip->li_lv == NULL);
				goto insert;
			}
		}

		ASSERT(IS_ALIGNED((unsigned long)lv->lv_buf, sizeof(uint64_t)));
		lip->li_ops->iop_format(lip, lv);
insert:
		xfs_cil_prepare_item(log, lv, old_lv, diff_len);
	}
}

 
static inline bool
xlog_cil_over_hard_limit(
	struct xlog	*log,
	int32_t		space_used)
{
	if (waitqueue_active(&log->l_cilp->xc_push_wait))
		return true;
	if (space_used >= XLOG_CIL_BLOCKING_SPACE_LIMIT(log))
		return true;
	return false;
}

 
static void
xlog_cil_insert_items(
	struct xlog		*log,
	struct xfs_trans	*tp,
	uint32_t		released_space)
{
	struct xfs_cil		*cil = log->l_cilp;
	struct xfs_cil_ctx	*ctx = cil->xc_ctx;
	struct xfs_log_item	*lip;
	int			len = 0;
	int			iovhdr_res = 0, split_res = 0, ctx_res = 0;
	int			space_used;
	int			order;
	unsigned int		cpu_nr;
	struct xlog_cil_pcp	*cilpcp;

	ASSERT(tp);

	 
	xlog_cil_insert_format_items(log, tp, &len);

	 
	len -= released_space;

	 
	cpu_nr = get_cpu();
	cilpcp = this_cpu_ptr(cil->xc_pcp);

	 
	if (!cpumask_test_cpu(cpu_nr, &ctx->cil_pcpmask))
		cpumask_test_and_set_cpu(cpu_nr, &ctx->cil_pcpmask);

	 
	if (test_bit(XLOG_CIL_EMPTY, &cil->xc_flags) &&
	    test_and_clear_bit(XLOG_CIL_EMPTY, &cil->xc_flags))
		ctx_res = ctx->ticket->t_unit_res;

	 
	space_used = atomic_read(&ctx->space_used) + cilpcp->space_used + len;
	if (atomic_read(&cil->xc_iclog_hdrs) > 0 ||
	    xlog_cil_over_hard_limit(log, space_used)) {
		split_res = log->l_iclog_hsize +
					sizeof(struct xlog_op_header);
		if (ctx_res)
			ctx_res += split_res * (tp->t_ticket->t_iclog_hdrs - 1);
		else
			ctx_res = split_res * tp->t_ticket->t_iclog_hdrs;
		atomic_sub(tp->t_ticket->t_iclog_hdrs, &cil->xc_iclog_hdrs);
	}
	cilpcp->space_reserved += ctx_res;

	 
	if (!test_bit(XLOG_CIL_PCP_SPACE, &cil->xc_flags)) {
		atomic_add(len, &ctx->space_used);
	} else if (cilpcp->space_used + len >
			(XLOG_CIL_SPACE_LIMIT(log) / num_online_cpus())) {
		space_used = atomic_add_return(cilpcp->space_used + len,
						&ctx->space_used);
		cilpcp->space_used = 0;

		 
		if (space_used >= XLOG_CIL_SPACE_LIMIT(log))
			xlog_cil_insert_pcp_aggregate(cil, ctx);
	} else {
		cilpcp->space_used += len;
	}
	 
	if (!list_empty(&tp->t_busy))
		list_splice_init(&tp->t_busy, &cilpcp->busy_extents);

	 
	order = atomic_inc_return(&ctx->order_id);
	list_for_each_entry(lip, &tp->t_items, li_trans) {
		 
		if (!test_bit(XFS_LI_DIRTY, &lip->li_flags))
			continue;

		lip->li_order_id = order;
		if (!list_empty(&lip->li_cil))
			continue;
		list_add_tail(&lip->li_cil, &cilpcp->log_items);
	}
	put_cpu();

	 
	tp->t_ticket->t_curr_res -= ctx_res + len;
	if (WARN_ON(tp->t_ticket->t_curr_res < 0)) {
		xfs_warn(log->l_mp, "Transaction log reservation overrun:");
		xfs_warn(log->l_mp,
			 "  log items: %d bytes (iov hdrs: %d bytes)",
			 len, iovhdr_res);
		xfs_warn(log->l_mp, "  split region headers: %d bytes",
			 split_res);
		xfs_warn(log->l_mp, "  ctx ticket: %d bytes", ctx_res);
		xlog_print_trans(tp);
		xlog_force_shutdown(log, SHUTDOWN_LOG_IO_ERROR);
	}
}

static void
xlog_cil_free_logvec(
	struct list_head	*lv_chain)
{
	struct xfs_log_vec	*lv;

	while (!list_empty(lv_chain)) {
		lv = list_first_entry(lv_chain, struct xfs_log_vec, lv_list);
		list_del_init(&lv->lv_list);
		kmem_free(lv);
	}
}

 
static void
xlog_cil_committed(
	struct xfs_cil_ctx	*ctx)
{
	struct xfs_mount	*mp = ctx->cil->xc_log->l_mp;
	bool			abort = xlog_is_shutdown(ctx->cil->xc_log);

	 
	if (abort) {
		spin_lock(&ctx->cil->xc_push_lock);
		wake_up_all(&ctx->cil->xc_start_wait);
		wake_up_all(&ctx->cil->xc_commit_wait);
		spin_unlock(&ctx->cil->xc_push_lock);
	}

	xfs_trans_committed_bulk(ctx->cil->xc_log->l_ailp, &ctx->lv_chain,
					ctx->start_lsn, abort);

	xfs_extent_busy_sort(&ctx->busy_extents.extent_list);
	xfs_extent_busy_clear(mp, &ctx->busy_extents.extent_list,
			      xfs_has_discard(mp) && !abort);

	spin_lock(&ctx->cil->xc_push_lock);
	list_del(&ctx->committing);
	spin_unlock(&ctx->cil->xc_push_lock);

	xlog_cil_free_logvec(&ctx->lv_chain);

	if (!list_empty(&ctx->busy_extents.extent_list)) {
		ctx->busy_extents.mount = mp;
		ctx->busy_extents.owner = ctx;
		xfs_discard_extents(mp, &ctx->busy_extents);
		return;
	}

	kmem_free(ctx);
}

void
xlog_cil_process_committed(
	struct list_head	*list)
{
	struct xfs_cil_ctx	*ctx;

	while ((ctx = list_first_entry_or_null(list,
			struct xfs_cil_ctx, iclog_entry))) {
		list_del(&ctx->iclog_entry);
		xlog_cil_committed(ctx);
	}
}

 
void
xlog_cil_set_ctx_write_state(
	struct xfs_cil_ctx	*ctx,
	struct xlog_in_core	*iclog)
{
	struct xfs_cil		*cil = ctx->cil;
	xfs_lsn_t		lsn = be64_to_cpu(iclog->ic_header.h_lsn);

	ASSERT(!ctx->commit_lsn);
	if (!ctx->start_lsn) {
		spin_lock(&cil->xc_push_lock);
		 
		ctx->start_lsn = lsn;
		wake_up_all(&cil->xc_start_wait);
		spin_unlock(&cil->xc_push_lock);

		 
		spin_lock(&cil->xc_log->l_icloglock);
		iclog->ic_flags |= XLOG_ICL_NEED_FLUSH;
		spin_unlock(&cil->xc_log->l_icloglock);
		return;
	}

	 
	atomic_inc(&iclog->ic_refcnt);

	 
	spin_lock(&cil->xc_log->l_icloglock);
	list_add_tail(&ctx->iclog_entry, &iclog->ic_callbacks);
	spin_unlock(&cil->xc_log->l_icloglock);

	 
	spin_lock(&cil->xc_push_lock);
	ctx->commit_iclog = iclog;
	ctx->commit_lsn = lsn;
	wake_up_all(&cil->xc_commit_wait);
	spin_unlock(&cil->xc_push_lock);
}


 
enum _record_type {
	_START_RECORD,
	_COMMIT_RECORD,
};

static int
xlog_cil_order_write(
	struct xfs_cil		*cil,
	xfs_csn_t		sequence,
	enum _record_type	record)
{
	struct xfs_cil_ctx	*ctx;

restart:
	spin_lock(&cil->xc_push_lock);
	list_for_each_entry(ctx, &cil->xc_committing, committing) {
		 
		if (xlog_is_shutdown(cil->xc_log)) {
			spin_unlock(&cil->xc_push_lock);
			return -EIO;
		}

		 
		if (ctx->sequence >= sequence)
			continue;

		 
		switch (record) {
		case _START_RECORD:
			if (!ctx->start_lsn) {
				xlog_wait(&cil->xc_start_wait, &cil->xc_push_lock);
				goto restart;
			}
			break;
		case _COMMIT_RECORD:
			if (!ctx->commit_lsn) {
				xlog_wait(&cil->xc_commit_wait, &cil->xc_push_lock);
				goto restart;
			}
			break;
		}
	}
	spin_unlock(&cil->xc_push_lock);
	return 0;
}

 
static int
xlog_cil_write_chain(
	struct xfs_cil_ctx	*ctx,
	uint32_t		chain_len)
{
	struct xlog		*log = ctx->cil->xc_log;
	int			error;

	error = xlog_cil_order_write(ctx->cil, ctx->sequence, _START_RECORD);
	if (error)
		return error;
	return xlog_write(log, ctx, &ctx->lv_chain, ctx->ticket, chain_len);
}

 
static int
xlog_cil_write_commit_record(
	struct xfs_cil_ctx	*ctx)
{
	struct xlog		*log = ctx->cil->xc_log;
	struct xlog_op_header	ophdr = {
		.oh_clientid = XFS_TRANSACTION,
		.oh_tid = cpu_to_be32(ctx->ticket->t_tid),
		.oh_flags = XLOG_COMMIT_TRANS,
	};
	struct xfs_log_iovec	reg = {
		.i_addr = &ophdr,
		.i_len = sizeof(struct xlog_op_header),
		.i_type = XLOG_REG_TYPE_COMMIT,
	};
	struct xfs_log_vec	vec = {
		.lv_niovecs = 1,
		.lv_iovecp = &reg,
	};
	int			error;
	LIST_HEAD(lv_chain);
	list_add(&vec.lv_list, &lv_chain);

	if (xlog_is_shutdown(log))
		return -EIO;

	error = xlog_cil_order_write(ctx->cil, ctx->sequence, _COMMIT_RECORD);
	if (error)
		return error;

	 
	ctx->ticket->t_curr_res -= reg.i_len;
	error = xlog_write(log, ctx, &lv_chain, ctx->ticket, reg.i_len);
	if (error)
		xlog_force_shutdown(log, SHUTDOWN_LOG_IO_ERROR);
	return error;
}

struct xlog_cil_trans_hdr {
	struct xlog_op_header	oph[2];
	struct xfs_trans_header	thdr;
	struct xfs_log_iovec	lhdr[2];
};

 
static void
xlog_cil_build_trans_hdr(
	struct xfs_cil_ctx	*ctx,
	struct xlog_cil_trans_hdr *hdr,
	struct xfs_log_vec	*lvhdr,
	int			num_iovecs)
{
	struct xlog_ticket	*tic = ctx->ticket;
	__be32			tid = cpu_to_be32(tic->t_tid);

	memset(hdr, 0, sizeof(*hdr));

	 
	hdr->oph[0].oh_tid = tid;
	hdr->oph[0].oh_clientid = XFS_TRANSACTION;
	hdr->oph[0].oh_flags = XLOG_START_TRANS;

	 
	hdr->lhdr[0].i_addr = &hdr->oph[0];
	hdr->lhdr[0].i_len = sizeof(struct xlog_op_header);
	hdr->lhdr[0].i_type = XLOG_REG_TYPE_LRHEADER;

	 
	hdr->oph[1].oh_tid = tid;
	hdr->oph[1].oh_clientid = XFS_TRANSACTION;
	hdr->oph[1].oh_len = cpu_to_be32(sizeof(struct xfs_trans_header));

	 
	hdr->thdr.th_magic = XFS_TRANS_HEADER_MAGIC;
	hdr->thdr.th_type = XFS_TRANS_CHECKPOINT;
	hdr->thdr.th_tid = tic->t_tid;
	hdr->thdr.th_num_items = num_iovecs;

	 
	hdr->lhdr[1].i_addr = &hdr->oph[1];
	hdr->lhdr[1].i_len = sizeof(struct xlog_op_header) +
				sizeof(struct xfs_trans_header);
	hdr->lhdr[1].i_type = XLOG_REG_TYPE_TRANSHDR;

	lvhdr->lv_niovecs = 2;
	lvhdr->lv_iovecp = &hdr->lhdr[0];
	lvhdr->lv_bytes = hdr->lhdr[0].i_len + hdr->lhdr[1].i_len;

	tic->t_curr_res -= lvhdr->lv_bytes;
}

 
static int
xlog_cil_order_cmp(
	void			*priv,
	const struct list_head	*a,
	const struct list_head	*b)
{
	struct xfs_log_vec	*l1 = container_of(a, struct xfs_log_vec, lv_list);
	struct xfs_log_vec	*l2 = container_of(b, struct xfs_log_vec, lv_list);

	return l1->lv_order_id > l2->lv_order_id;
}

 
static void
xlog_cil_build_lv_chain(
	struct xfs_cil_ctx	*ctx,
	struct list_head	*whiteouts,
	uint32_t		*num_iovecs,
	uint32_t		*num_bytes)
{
	while (!list_empty(&ctx->log_items)) {
		struct xfs_log_item	*item;
		struct xfs_log_vec	*lv;

		item = list_first_entry(&ctx->log_items,
					struct xfs_log_item, li_cil);

		if (test_bit(XFS_LI_WHITEOUT, &item->li_flags)) {
			list_move(&item->li_cil, whiteouts);
			trace_xfs_cil_whiteout_skip(item);
			continue;
		}

		lv = item->li_lv;
		lv->lv_order_id = item->li_order_id;

		 
		if (lv->lv_buf_len != XFS_LOG_VEC_ORDERED)
			*num_bytes += lv->lv_bytes;
		*num_iovecs += lv->lv_niovecs;
		list_add_tail(&lv->lv_list, &ctx->lv_chain);

		list_del_init(&item->li_cil);
		item->li_order_id = 0;
		item->li_lv = NULL;
	}
}

static void
xlog_cil_cleanup_whiteouts(
	struct list_head	*whiteouts)
{
	while (!list_empty(whiteouts)) {
		struct xfs_log_item *item = list_first_entry(whiteouts,
						struct xfs_log_item, li_cil);
		list_del_init(&item->li_cil);
		trace_xfs_cil_whiteout_unpin(item);
		item->li_ops->iop_unpin(item, 1);
	}
}

 
static void
xlog_cil_push_work(
	struct work_struct	*work)
{
	struct xfs_cil_ctx	*ctx =
		container_of(work, struct xfs_cil_ctx, push_work);
	struct xfs_cil		*cil = ctx->cil;
	struct xlog		*log = cil->xc_log;
	struct xfs_cil_ctx	*new_ctx;
	int			num_iovecs = 0;
	int			num_bytes = 0;
	int			error = 0;
	struct xlog_cil_trans_hdr thdr;
	struct xfs_log_vec	lvhdr = {};
	xfs_csn_t		push_seq;
	bool			push_commit_stable;
	LIST_HEAD		(whiteouts);
	struct xlog_ticket	*ticket;

	new_ctx = xlog_cil_ctx_alloc();
	new_ctx->ticket = xlog_cil_ticket_alloc(log);

	down_write(&cil->xc_ctx_lock);

	spin_lock(&cil->xc_push_lock);
	push_seq = cil->xc_push_seq;
	ASSERT(push_seq <= ctx->sequence);
	push_commit_stable = cil->xc_push_commit_stable;
	cil->xc_push_commit_stable = false;

	 
	if (waitqueue_active(&cil->xc_push_wait))
		wake_up_all(&cil->xc_push_wait);

	xlog_cil_push_pcp_aggregate(cil, ctx);

	 
	if (test_bit(XLOG_CIL_EMPTY, &cil->xc_flags)) {
		cil->xc_push_seq = 0;
		spin_unlock(&cil->xc_push_lock);
		goto out_skip;
	}


	 
	if (push_seq < ctx->sequence) {
		spin_unlock(&cil->xc_push_lock);
		goto out_skip;
	}

	 
	list_add(&ctx->committing, &cil->xc_committing);
	spin_unlock(&cil->xc_push_lock);

	xlog_cil_build_lv_chain(ctx, &whiteouts, &num_iovecs, &num_bytes);

	 
	spin_lock(&cil->xc_push_lock);
	xlog_cil_ctx_switch(cil, new_ctx);
	spin_unlock(&cil->xc_push_lock);
	up_write(&cil->xc_ctx_lock);

	 
	list_sort(NULL, &ctx->lv_chain, xlog_cil_order_cmp);

	 
	xlog_cil_build_trans_hdr(ctx, &thdr, &lvhdr, num_iovecs);
	num_bytes += lvhdr.lv_bytes;
	list_add(&lvhdr.lv_list, &ctx->lv_chain);

	 
	error = xlog_cil_write_chain(ctx, num_bytes);
	list_del(&lvhdr.lv_list);
	if (error)
		goto out_abort_free_ticket;

	error = xlog_cil_write_commit_record(ctx);
	if (error)
		goto out_abort_free_ticket;

	 
	ticket = ctx->ticket;

	 
	spin_lock(&log->l_icloglock);
	if (ctx->start_lsn != ctx->commit_lsn) {
		xfs_lsn_t	plsn;

		plsn = be64_to_cpu(ctx->commit_iclog->ic_prev->ic_header.h_lsn);
		if (plsn && XFS_LSN_CMP(plsn, ctx->commit_lsn) < 0) {
			 
			xlog_wait_on_iclog(ctx->commit_iclog->ic_prev);
			spin_lock(&log->l_icloglock);
		}

		 
		ctx->commit_iclog->ic_flags |= XLOG_ICL_NEED_FLUSH;
	}

	 
	ctx->commit_iclog->ic_flags |= XLOG_ICL_NEED_FUA;
	if (push_commit_stable &&
	    ctx->commit_iclog->ic_state == XLOG_STATE_ACTIVE)
		xlog_state_switch_iclogs(log, ctx->commit_iclog, 0);
	ticket = ctx->ticket;
	xlog_state_release_iclog(log, ctx->commit_iclog, ticket);

	 

	spin_unlock(&log->l_icloglock);
	xlog_cil_cleanup_whiteouts(&whiteouts);
	xfs_log_ticket_ungrant(log, ticket);
	return;

out_skip:
	up_write(&cil->xc_ctx_lock);
	xfs_log_ticket_put(new_ctx->ticket);
	kmem_free(new_ctx);
	return;

out_abort_free_ticket:
	ASSERT(xlog_is_shutdown(log));
	xlog_cil_cleanup_whiteouts(&whiteouts);
	if (!ctx->commit_iclog) {
		xfs_log_ticket_ungrant(log, ctx->ticket);
		xlog_cil_committed(ctx);
		return;
	}
	spin_lock(&log->l_icloglock);
	ticket = ctx->ticket;
	xlog_state_release_iclog(log, ctx->commit_iclog, ticket);
	 
	spin_unlock(&log->l_icloglock);
	xfs_log_ticket_ungrant(log, ticket);
}

 
static void
xlog_cil_push_background(
	struct xlog	*log) __releases(cil->xc_ctx_lock)
{
	struct xfs_cil	*cil = log->l_cilp;
	int		space_used = atomic_read(&cil->xc_ctx->space_used);

	 
	ASSERT(!test_bit(XLOG_CIL_EMPTY, &cil->xc_flags));

	 
	if (space_used < XLOG_CIL_SPACE_LIMIT(log) ||
	    (cil->xc_push_seq == cil->xc_current_sequence &&
	     space_used < XLOG_CIL_BLOCKING_SPACE_LIMIT(log) &&
	     !waitqueue_active(&cil->xc_push_wait))) {
		up_read(&cil->xc_ctx_lock);
		return;
	}

	spin_lock(&cil->xc_push_lock);
	if (cil->xc_push_seq < cil->xc_current_sequence) {
		cil->xc_push_seq = cil->xc_current_sequence;
		queue_work(cil->xc_push_wq, &cil->xc_ctx->push_work);
	}

	 
	up_read(&cil->xc_ctx_lock);

	 
	if (xlog_cil_over_hard_limit(log, space_used)) {
		trace_xfs_log_cil_wait(log, cil->xc_ctx->ticket);
		ASSERT(space_used < log->l_logsize);
		xlog_wait(&cil->xc_push_wait, &cil->xc_push_lock);
		return;
	}

	spin_unlock(&cil->xc_push_lock);

}

 
static void
xlog_cil_push_now(
	struct xlog	*log,
	xfs_lsn_t	push_seq,
	bool		async)
{
	struct xfs_cil	*cil = log->l_cilp;

	if (!cil)
		return;

	ASSERT(push_seq && push_seq <= cil->xc_current_sequence);

	 
	if (!async)
		flush_workqueue(cil->xc_push_wq);

	spin_lock(&cil->xc_push_lock);

	 
	cil->xc_push_commit_stable = async;

	 
	if (test_bit(XLOG_CIL_EMPTY, &cil->xc_flags) ||
	    push_seq <= cil->xc_push_seq) {
		spin_unlock(&cil->xc_push_lock);
		return;
	}

	cil->xc_push_seq = push_seq;
	queue_work(cil->xc_push_wq, &cil->xc_ctx->push_work);
	spin_unlock(&cil->xc_push_lock);
}

bool
xlog_cil_empty(
	struct xlog	*log)
{
	struct xfs_cil	*cil = log->l_cilp;
	bool		empty = false;

	spin_lock(&cil->xc_push_lock);
	if (test_bit(XLOG_CIL_EMPTY, &cil->xc_flags))
		empty = true;
	spin_unlock(&cil->xc_push_lock);
	return empty;
}

 
static uint32_t
xlog_cil_process_intents(
	struct xfs_cil		*cil,
	struct xfs_trans	*tp)
{
	struct xfs_log_item	*lip, *ilip, *next;
	uint32_t		len = 0;

	list_for_each_entry_safe(lip, next, &tp->t_items, li_trans) {
		if (!(lip->li_ops->flags & XFS_ITEM_INTENT_DONE))
			continue;

		ilip = lip->li_ops->iop_intent(lip);
		if (!ilip || !xlog_item_in_current_chkpt(cil, ilip))
			continue;
		set_bit(XFS_LI_WHITEOUT, &ilip->li_flags);
		trace_xfs_cil_whiteout_mark(ilip);
		len += ilip->li_lv->lv_bytes;
		kmem_free(ilip->li_lv);
		ilip->li_lv = NULL;

		xfs_trans_del_item(lip);
		lip->li_ops->iop_release(lip);
	}
	return len;
}

 
void
xlog_cil_commit(
	struct xlog		*log,
	struct xfs_trans	*tp,
	xfs_csn_t		*commit_seq,
	bool			regrant)
{
	struct xfs_cil		*cil = log->l_cilp;
	struct xfs_log_item	*lip, *next;
	uint32_t		released_space = 0;

	 
	xlog_cil_alloc_shadow_bufs(log, tp);

	 
	down_read(&cil->xc_ctx_lock);

	if (tp->t_flags & XFS_TRANS_HAS_INTENT_DONE)
		released_space = xlog_cil_process_intents(cil, tp);

	xlog_cil_insert_items(log, tp, released_space);

	if (regrant && !xlog_is_shutdown(log))
		xfs_log_ticket_regrant(log, tp->t_ticket);
	else
		xfs_log_ticket_ungrant(log, tp->t_ticket);
	tp->t_ticket = NULL;
	xfs_trans_unreserve_and_mod_sb(tp);

	 
	trace_xfs_trans_commit_items(tp, _RET_IP_);
	list_for_each_entry_safe(lip, next, &tp->t_items, li_trans) {
		xfs_trans_del_item(lip);
		if (lip->li_ops->iop_committing)
			lip->li_ops->iop_committing(lip, cil->xc_ctx->sequence);
	}
	if (commit_seq)
		*commit_seq = cil->xc_ctx->sequence;

	 
	xlog_cil_push_background(log);
}

 
void
xlog_cil_flush(
	struct xlog	*log)
{
	xfs_csn_t	seq = log->l_cilp->xc_current_sequence;

	trace_xfs_log_force(log->l_mp, seq, _RET_IP_);
	xlog_cil_push_now(log, seq, true);

	 
	if (test_bit(XLOG_CIL_EMPTY, &log->l_cilp->xc_flags))
		xfs_log_force(log->l_mp, 0);
}

 
xfs_lsn_t
xlog_cil_force_seq(
	struct xlog	*log,
	xfs_csn_t	sequence)
{
	struct xfs_cil		*cil = log->l_cilp;
	struct xfs_cil_ctx	*ctx;
	xfs_lsn_t		commit_lsn = NULLCOMMITLSN;

	ASSERT(sequence <= cil->xc_current_sequence);

	if (!sequence)
		sequence = cil->xc_current_sequence;
	trace_xfs_log_force(log->l_mp, sequence, _RET_IP_);

	 
restart:
	xlog_cil_push_now(log, sequence, false);

	 
	spin_lock(&cil->xc_push_lock);
	list_for_each_entry(ctx, &cil->xc_committing, committing) {
		 
		if (xlog_is_shutdown(log))
			goto out_shutdown;
		if (ctx->sequence > sequence)
			continue;
		if (!ctx->commit_lsn) {
			 
			XFS_STATS_INC(log->l_mp, xs_log_force_sleep);
			xlog_wait(&cil->xc_commit_wait, &cil->xc_push_lock);
			goto restart;
		}
		if (ctx->sequence != sequence)
			continue;
		 
		commit_lsn = ctx->commit_lsn;
	}

	 
	if (sequence == cil->xc_current_sequence &&
	    !test_bit(XLOG_CIL_EMPTY, &cil->xc_flags)) {
		spin_unlock(&cil->xc_push_lock);
		goto restart;
	}

	spin_unlock(&cil->xc_push_lock);
	return commit_lsn;

	 
out_shutdown:
	spin_unlock(&cil->xc_push_lock);
	return 0;
}

 
int
xlog_cil_init(
	struct xlog		*log)
{
	struct xfs_cil		*cil;
	struct xfs_cil_ctx	*ctx;
	struct xlog_cil_pcp	*cilpcp;
	int			cpu;

	cil = kmem_zalloc(sizeof(*cil), KM_MAYFAIL);
	if (!cil)
		return -ENOMEM;
	 
	cil->xc_push_wq = alloc_workqueue("xfs-cil/%s",
			XFS_WQFLAGS(WQ_FREEZABLE | WQ_MEM_RECLAIM | WQ_UNBOUND),
			4, log->l_mp->m_super->s_id);
	if (!cil->xc_push_wq)
		goto out_destroy_cil;

	cil->xc_log = log;
	cil->xc_pcp = alloc_percpu(struct xlog_cil_pcp);
	if (!cil->xc_pcp)
		goto out_destroy_wq;

	for_each_possible_cpu(cpu) {
		cilpcp = per_cpu_ptr(cil->xc_pcp, cpu);
		INIT_LIST_HEAD(&cilpcp->busy_extents);
		INIT_LIST_HEAD(&cilpcp->log_items);
	}

	INIT_LIST_HEAD(&cil->xc_committing);
	spin_lock_init(&cil->xc_push_lock);
	init_waitqueue_head(&cil->xc_push_wait);
	init_rwsem(&cil->xc_ctx_lock);
	init_waitqueue_head(&cil->xc_start_wait);
	init_waitqueue_head(&cil->xc_commit_wait);
	log->l_cilp = cil;

	ctx = xlog_cil_ctx_alloc();
	xlog_cil_ctx_switch(cil, ctx);
	return 0;

out_destroy_wq:
	destroy_workqueue(cil->xc_push_wq);
out_destroy_cil:
	kmem_free(cil);
	return -ENOMEM;
}

void
xlog_cil_destroy(
	struct xlog	*log)
{
	struct xfs_cil	*cil = log->l_cilp;

	if (cil->xc_ctx) {
		if (cil->xc_ctx->ticket)
			xfs_log_ticket_put(cil->xc_ctx->ticket);
		kmem_free(cil->xc_ctx);
	}

	ASSERT(test_bit(XLOG_CIL_EMPTY, &cil->xc_flags));
	free_percpu(cil->xc_pcp);
	destroy_workqueue(cil->xc_push_wq);
	kmem_free(cil);
}

