
 
#ifndef __XFS_TRANS_PRIV_H__
#define	__XFS_TRANS_PRIV_H__

struct xlog;
struct xfs_log_item;
struct xfs_mount;
struct xfs_trans;
struct xfs_ail;
struct xfs_log_vec;


void	xfs_trans_init(struct xfs_mount *);
void	xfs_trans_add_item(struct xfs_trans *, struct xfs_log_item *);
void	xfs_trans_del_item(struct xfs_log_item *);
void	xfs_trans_unreserve_and_mod_sb(struct xfs_trans *tp);

void	xfs_trans_committed_bulk(struct xfs_ail *ailp,
				struct list_head *lv_chain,
				xfs_lsn_t commit_lsn, bool aborted);
 
struct xfs_ail_cursor {
	struct list_head	list;
	struct xfs_log_item	*item;
};

 
struct xfs_ail {
	struct xlog		*ail_log;
	struct task_struct	*ail_task;
	struct list_head	ail_head;
	xfs_lsn_t		ail_target;
	xfs_lsn_t		ail_target_prev;
	struct list_head	ail_cursors;
	spinlock_t		ail_lock;
	xfs_lsn_t		ail_last_pushed_lsn;
	int			ail_log_flush;
	struct list_head	ail_buf_list;
	wait_queue_head_t	ail_empty;
};

 
void	xfs_trans_ail_update_bulk(struct xfs_ail *ailp,
				struct xfs_ail_cursor *cur,
				struct xfs_log_item **log_items, int nr_items,
				xfs_lsn_t lsn) __releases(ailp->ail_lock);
 
static inline struct xfs_log_item *
xfs_ail_min(
	struct xfs_ail  *ailp)
{
	return list_first_entry_or_null(&ailp->ail_head, struct xfs_log_item,
					li_ail);
}

static inline void
xfs_trans_ail_update(
	struct xfs_ail		*ailp,
	struct xfs_log_item	*lip,
	xfs_lsn_t		lsn) __releases(ailp->ail_lock)
{
	xfs_trans_ail_update_bulk(ailp, NULL, &lip, 1, lsn);
}

void xfs_trans_ail_insert(struct xfs_ail *ailp, struct xfs_log_item *lip,
		xfs_lsn_t lsn);

xfs_lsn_t xfs_ail_delete_one(struct xfs_ail *ailp, struct xfs_log_item *lip);
void xfs_ail_update_finish(struct xfs_ail *ailp, xfs_lsn_t old_lsn)
			__releases(ailp->ail_lock);
void xfs_trans_ail_delete(struct xfs_log_item *lip, int shutdown_type);

void			xfs_ail_push(struct xfs_ail *, xfs_lsn_t);
void			xfs_ail_push_all(struct xfs_ail *);
void			xfs_ail_push_all_sync(struct xfs_ail *);
struct xfs_log_item	*xfs_ail_min(struct xfs_ail  *ailp);
xfs_lsn_t		xfs_ail_min_lsn(struct xfs_ail *ailp);

struct xfs_log_item *	xfs_trans_ail_cursor_first(struct xfs_ail *ailp,
					struct xfs_ail_cursor *cur,
					xfs_lsn_t lsn);
struct xfs_log_item *	xfs_trans_ail_cursor_last(struct xfs_ail *ailp,
					struct xfs_ail_cursor *cur,
					xfs_lsn_t lsn);
struct xfs_log_item *	xfs_trans_ail_cursor_next(struct xfs_ail *ailp,
					struct xfs_ail_cursor *cur);
void			xfs_trans_ail_cursor_done(struct xfs_ail_cursor *cur);

#if BITS_PER_LONG != 64
static inline void
xfs_trans_ail_copy_lsn(
	struct xfs_ail	*ailp,
	xfs_lsn_t	*dst,
	xfs_lsn_t	*src)
{
	ASSERT(sizeof(xfs_lsn_t) == 8);	 
	spin_lock(&ailp->ail_lock);
	*dst = *src;
	spin_unlock(&ailp->ail_lock);
}
#else
static inline void
xfs_trans_ail_copy_lsn(
	struct xfs_ail	*ailp,
	xfs_lsn_t	*dst,
	xfs_lsn_t	*src)
{
	ASSERT(sizeof(xfs_lsn_t) == 8);
	*dst = *src;
}
#endif

static inline void
xfs_clear_li_failed(
	struct xfs_log_item	*lip)
{
	struct xfs_buf	*bp = lip->li_buf;

	ASSERT(test_bit(XFS_LI_IN_AIL, &lip->li_flags));
	lockdep_assert_held(&lip->li_ailp->ail_lock);

	if (test_and_clear_bit(XFS_LI_FAILED, &lip->li_flags)) {
		lip->li_buf = NULL;
		xfs_buf_rele(bp);
	}
}

static inline void
xfs_set_li_failed(
	struct xfs_log_item	*lip,
	struct xfs_buf		*bp)
{
	lockdep_assert_held(&lip->li_ailp->ail_lock);

	if (!test_and_set_bit(XFS_LI_FAILED, &lip->li_flags)) {
		xfs_buf_hold(bp);
		lip->li_buf = bp;
	}
}

#endif	 
