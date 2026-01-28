#ifndef	__XFS_LOG_PRIV_H__
#define __XFS_LOG_PRIV_H__
#include "xfs_extent_busy.h"	 
struct xfs_buf;
struct xlog;
struct xlog_ticket;
struct xfs_mount;
static inline uint xlog_get_client_id(__be32 i)
{
	return be32_to_cpu(i) >> 24;
}
enum xlog_iclog_state {
	XLOG_STATE_ACTIVE,	 
	XLOG_STATE_WANT_SYNC,	 
	XLOG_STATE_SYNCING,	 
	XLOG_STATE_DONE_SYNC,	 
	XLOG_STATE_CALLBACK,	 
	XLOG_STATE_DIRTY,	 
};
#define XLOG_STATE_STRINGS \
	{ XLOG_STATE_ACTIVE,	"XLOG_STATE_ACTIVE" }, \
	{ XLOG_STATE_WANT_SYNC,	"XLOG_STATE_WANT_SYNC" }, \
	{ XLOG_STATE_SYNCING,	"XLOG_STATE_SYNCING" }, \
	{ XLOG_STATE_DONE_SYNC,	"XLOG_STATE_DONE_SYNC" }, \
	{ XLOG_STATE_CALLBACK,	"XLOG_STATE_CALLBACK" }, \
	{ XLOG_STATE_DIRTY,	"XLOG_STATE_DIRTY" }
#define XLOG_ICL_NEED_FLUSH	(1u << 0)	 
#define XLOG_ICL_NEED_FUA	(1u << 1)	 
#define XLOG_ICL_STRINGS \
	{ XLOG_ICL_NEED_FLUSH,	"XLOG_ICL_NEED_FLUSH" }, \
	{ XLOG_ICL_NEED_FUA,	"XLOG_ICL_NEED_FUA" }
#define XLOG_TIC_PERM_RESERV	(1u << 0)	 
#define XLOG_TIC_FLAGS \
	{ XLOG_TIC_PERM_RESERV,	"XLOG_TIC_PERM_RESERV" }
#define XLOG_STATE_COVER_IDLE	0
#define XLOG_STATE_COVER_NEED	1
#define XLOG_STATE_COVER_DONE	2
#define XLOG_STATE_COVER_NEED2	3
#define XLOG_STATE_COVER_DONE2	4
#define XLOG_COVER_OPS		5
typedef struct xlog_ticket {
	struct list_head	t_queue;	 
	struct task_struct	*t_task;	 
	xlog_tid_t		t_tid;		 
	atomic_t		t_ref;		 
	int			t_curr_res;	 
	int			t_unit_res;	 
	char			t_ocnt;		 
	char			t_cnt;		 
	uint8_t			t_flags;	 
	int			t_iclog_hdrs;	 
} xlog_ticket_t;
typedef struct xlog_in_core {
	wait_queue_head_t	ic_force_wait;
	wait_queue_head_t	ic_write_wait;
	struct xlog_in_core	*ic_next;
	struct xlog_in_core	*ic_prev;
	struct xlog		*ic_log;
	u32			ic_size;
	u32			ic_offset;
	enum xlog_iclog_state	ic_state;
	unsigned int		ic_flags;
	void			*ic_datap;	 
	struct list_head	ic_callbacks;
	atomic_t		ic_refcnt ____cacheline_aligned_in_smp;
	xlog_in_core_2_t	*ic_data;
#define ic_header	ic_data->hic_header
#ifdef DEBUG
	bool			ic_fail_crc : 1;
#endif
	struct semaphore	ic_sema;
	struct work_struct	ic_end_io_work;
	struct bio		ic_bio;
	struct bio_vec		ic_bvec[];
} xlog_in_core_t;
struct xfs_cil;
struct xfs_cil_ctx {
	struct xfs_cil		*cil;
	xfs_csn_t		sequence;	 
	xfs_lsn_t		start_lsn;	 
	xfs_lsn_t		commit_lsn;	 
	struct xlog_in_core	*commit_iclog;
	struct xlog_ticket	*ticket;	 
	atomic_t		space_used;	 
	struct xfs_busy_extents	busy_extents;
	struct list_head	log_items;	 
	struct list_head	lv_chain;	 
	struct list_head	iclog_entry;
	struct list_head	committing;	 
	struct work_struct	push_work;
	atomic_t		order_id;
	struct cpumask		cil_pcpmask;
};
struct xlog_cil_pcp {
	int32_t			space_used;
	uint32_t		space_reserved;
	struct list_head	busy_extents;
	struct list_head	log_items;
};
struct xfs_cil {
	struct xlog		*xc_log;
	unsigned long		xc_flags;
	atomic_t		xc_iclog_hdrs;
	struct workqueue_struct	*xc_push_wq;
	struct rw_semaphore	xc_ctx_lock ____cacheline_aligned_in_smp;
	struct xfs_cil_ctx	*xc_ctx;
	spinlock_t		xc_push_lock ____cacheline_aligned_in_smp;
	xfs_csn_t		xc_push_seq;
	bool			xc_push_commit_stable;
	struct list_head	xc_committing;
	wait_queue_head_t	xc_commit_wait;
	wait_queue_head_t	xc_start_wait;
	xfs_csn_t		xc_current_sequence;
	wait_queue_head_t	xc_push_wait;	 
	void __percpu		*xc_pcp;	 
} ____cacheline_aligned_in_smp;
#define	XLOG_CIL_EMPTY		1
#define XLOG_CIL_PCP_SPACE	2
#define XLOG_CIL_SPACE_LIMIT(log)	\
	min_t(int, (log)->l_logsize >> 3, BBTOB(XLOG_TOTAL_REC_SHIFT(log)) << 4)
#define XLOG_CIL_BLOCKING_SPACE_LIMIT(log)	\
	(XLOG_CIL_SPACE_LIMIT(log) * 2)
struct xlog_grant_head {
	spinlock_t		lock ____cacheline_aligned_in_smp;
	struct list_head	waiters;
	atomic64_t		grant;
};
struct xlog {
	struct xfs_mount	*l_mp;	         
	struct xfs_ail		*l_ailp;	 
	struct xfs_cil		*l_cilp;	 
	struct xfs_buftarg	*l_targ;         
	struct workqueue_struct	*l_ioend_workqueue;  
	struct delayed_work	l_work;		 
	long			l_opstate;	 
	uint			l_quotaoffs_flag;  
	struct list_head	*l_buf_cancel_table;
	int			l_iclog_hsize;   
	int			l_iclog_heads;   
	uint			l_sectBBsize;    
	int			l_iclog_size;	 
	int			l_iclog_bufs;	 
	xfs_daddr_t		l_logBBstart;    
	int			l_logsize;       
	int			l_logBBsize;     
	wait_queue_head_t	l_flush_wait ____cacheline_aligned_in_smp;
	int			l_covered_state; 
	xlog_in_core_t		*l_iclog;        
	spinlock_t		l_icloglock;     
	int			l_curr_cycle;    
	int			l_prev_cycle;    
	int			l_curr_block;    
	int			l_prev_block;    
	atomic64_t		l_last_sync_lsn ____cacheline_aligned_in_smp;
	atomic64_t		l_tail_lsn ____cacheline_aligned_in_smp;
	struct xlog_grant_head	l_reserve_head;
	struct xlog_grant_head	l_write_head;
	struct xfs_kobj		l_kobj;
	xfs_lsn_t		l_recovery_lsn;
	uint32_t		l_iclog_roundoff; 
	struct rw_semaphore	l_incompat_users;
};
#define XLOG_ACTIVE_RECOVERY	0	 
#define XLOG_RECOVERY_NEEDED	1	 
#define XLOG_IO_ERROR		2	 
#define XLOG_TAIL_WARN		3	 
static inline bool
xlog_recovery_needed(struct xlog *log)
{
	return test_bit(XLOG_RECOVERY_NEEDED, &log->l_opstate);
}
static inline bool
xlog_in_recovery(struct xlog *log)
{
	return test_bit(XLOG_ACTIVE_RECOVERY, &log->l_opstate);
}
static inline bool
xlog_is_shutdown(struct xlog *log)
{
	return test_bit(XLOG_IO_ERROR, &log->l_opstate);
}
static inline void
xlog_shutdown_wait(
	struct xlog	*log)
{
	wait_var_event(&log->l_opstate, xlog_is_shutdown(log));
}
extern int
xlog_recover(
	struct xlog		*log);
extern int
xlog_recover_finish(
	struct xlog		*log);
extern void
xlog_recover_cancel(struct xlog *);
extern __le32	 xlog_cksum(struct xlog *log, struct xlog_rec_header *rhead,
			    char *dp, int size);
extern struct kmem_cache *xfs_log_ticket_cache;
struct xlog_ticket *xlog_ticket_alloc(struct xlog *log, int unit_bytes,
		int count, bool permanent);
void	xlog_print_tic_res(struct xfs_mount *mp, struct xlog_ticket *ticket);
void	xlog_print_trans(struct xfs_trans *);
int	xlog_write(struct xlog *log, struct xfs_cil_ctx *ctx,
		struct list_head *lv_chain, struct xlog_ticket *tic,
		uint32_t len);
void	xfs_log_ticket_ungrant(struct xlog *log, struct xlog_ticket *ticket);
void	xfs_log_ticket_regrant(struct xlog *log, struct xlog_ticket *ticket);
void xlog_state_switch_iclogs(struct xlog *log, struct xlog_in_core *iclog,
		int eventual_size);
int xlog_state_release_iclog(struct xlog *log, struct xlog_in_core *iclog,
		struct xlog_ticket *ticket);
static inline void
xlog_crack_atomic_lsn(atomic64_t *lsn, uint *cycle, uint *block)
{
	xfs_lsn_t val = atomic64_read(lsn);
	*cycle = CYCLE_LSN(val);
	*block = BLOCK_LSN(val);
}
static inline void
xlog_assign_atomic_lsn(atomic64_t *lsn, uint cycle, uint block)
{
	atomic64_set(lsn, xlog_assign_lsn(cycle, block));
}
static inline void
xlog_crack_grant_head_val(int64_t val, int *cycle, int *space)
{
	*cycle = val >> 32;
	*space = val & 0xffffffff;
}
static inline void
xlog_crack_grant_head(atomic64_t *head, int *cycle, int *space)
{
	xlog_crack_grant_head_val(atomic64_read(head), cycle, space);
}
static inline int64_t
xlog_assign_grant_head_val(int cycle, int space)
{
	return ((int64_t)cycle << 32) | space;
}
static inline void
xlog_assign_grant_head(atomic64_t *head, int cycle, int space)
{
	atomic64_set(head, xlog_assign_grant_head_val(cycle, space));
}
int	xlog_cil_init(struct xlog *log);
void	xlog_cil_init_post_recovery(struct xlog *log);
void	xlog_cil_destroy(struct xlog *log);
bool	xlog_cil_empty(struct xlog *log);
void	xlog_cil_commit(struct xlog *log, struct xfs_trans *tp,
			xfs_csn_t *commit_seq, bool regrant);
void	xlog_cil_set_ctx_write_state(struct xfs_cil_ctx *ctx,
			struct xlog_in_core *iclog);
void xlog_cil_flush(struct xlog *log);
xfs_lsn_t xlog_cil_force_seq(struct xlog *log, xfs_csn_t sequence);
static inline void
xlog_cil_force(struct xlog *log)
{
	xlog_cil_force_seq(log, log->l_cilp->xc_current_sequence);
}
static inline void
xlog_wait(
	struct wait_queue_head	*wq,
	struct spinlock		*lock)
		__releases(lock)
{
	DECLARE_WAITQUEUE(wait, current);
	add_wait_queue_exclusive(wq, &wait);
	__set_current_state(TASK_UNINTERRUPTIBLE);
	spin_unlock(lock);
	schedule();
	remove_wait_queue(wq, &wait);
}
int xlog_wait_on_iclog(struct xlog_in_core *iclog);
static inline bool
xlog_valid_lsn(
	struct xlog	*log,
	xfs_lsn_t	lsn)
{
	int		cur_cycle;
	int		cur_block;
	bool		valid = true;
	cur_cycle = READ_ONCE(log->l_curr_cycle);
	smp_rmb();
	cur_block = READ_ONCE(log->l_curr_block);
	if ((CYCLE_LSN(lsn) > cur_cycle) ||
	    (CYCLE_LSN(lsn) == cur_cycle && BLOCK_LSN(lsn) > cur_block)) {
		spin_lock(&log->l_icloglock);
		cur_cycle = log->l_curr_cycle;
		cur_block = log->l_curr_block;
		spin_unlock(&log->l_icloglock);
		if ((CYCLE_LSN(lsn) > cur_cycle) ||
		    (CYCLE_LSN(lsn) == cur_cycle && BLOCK_LSN(lsn) > cur_block))
			valid = false;
	}
	return valid;
}
static inline void *
xlog_kvmalloc(
	size_t		buf_size)
{
	gfp_t		flags = GFP_KERNEL;
	void		*p;
	flags &= ~__GFP_DIRECT_RECLAIM;
	flags |= __GFP_NOWARN | __GFP_NORETRY;
	do {
		p = kmalloc(buf_size, flags);
		if (!p)
			p = vmalloc(buf_size);
	} while (!p);
	return p;
}
#endif	 
