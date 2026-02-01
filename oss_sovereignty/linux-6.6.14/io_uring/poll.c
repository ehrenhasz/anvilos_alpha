
#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/fs.h>
#include <linux/file.h>
#include <linux/mm.h>
#include <linux/slab.h>
#include <linux/poll.h>
#include <linux/hashtable.h>
#include <linux/io_uring.h>

#include <trace/events/io_uring.h>

#include <uapi/linux/io_uring.h>

#include "io_uring.h"
#include "refs.h"
#include "opdef.h"
#include "kbuf.h"
#include "poll.h"
#include "cancel.h"

struct io_poll_update {
	struct file			*file;
	u64				old_user_data;
	u64				new_user_data;
	__poll_t			events;
	bool				update_events;
	bool				update_user_data;
};

struct io_poll_table {
	struct poll_table_struct pt;
	struct io_kiocb *req;
	int nr_entries;
	int error;
	bool owning;
	 
	__poll_t result_mask;
};

#define IO_POLL_CANCEL_FLAG	BIT(31)
#define IO_POLL_RETRY_FLAG	BIT(30)
#define IO_POLL_REF_MASK	GENMASK(29, 0)

 
#define IO_POLL_REF_BIAS	128

#define IO_WQE_F_DOUBLE		1

static int io_poll_wake(struct wait_queue_entry *wait, unsigned mode, int sync,
			void *key);

static inline struct io_kiocb *wqe_to_req(struct wait_queue_entry *wqe)
{
	unsigned long priv = (unsigned long)wqe->private;

	return (struct io_kiocb *)(priv & ~IO_WQE_F_DOUBLE);
}

static inline bool wqe_is_double(struct wait_queue_entry *wqe)
{
	unsigned long priv = (unsigned long)wqe->private;

	return priv & IO_WQE_F_DOUBLE;
}

static bool io_poll_get_ownership_slowpath(struct io_kiocb *req)
{
	int v;

	 
	v = atomic_fetch_or(IO_POLL_RETRY_FLAG, &req->poll_refs);
	if (v & IO_POLL_REF_MASK)
		return false;
	return !(atomic_fetch_inc(&req->poll_refs) & IO_POLL_REF_MASK);
}

 
static inline bool io_poll_get_ownership(struct io_kiocb *req)
{
	if (unlikely(atomic_read(&req->poll_refs) >= IO_POLL_REF_BIAS))
		return io_poll_get_ownership_slowpath(req);
	return !(atomic_fetch_inc(&req->poll_refs) & IO_POLL_REF_MASK);
}

static void io_poll_mark_cancelled(struct io_kiocb *req)
{
	atomic_or(IO_POLL_CANCEL_FLAG, &req->poll_refs);
}

static struct io_poll *io_poll_get_double(struct io_kiocb *req)
{
	 
	if (req->opcode == IORING_OP_POLL_ADD)
		return req->async_data;
	return req->apoll->double_poll;
}

static struct io_poll *io_poll_get_single(struct io_kiocb *req)
{
	if (req->opcode == IORING_OP_POLL_ADD)
		return io_kiocb_to_cmd(req, struct io_poll);
	return &req->apoll->poll;
}

static void io_poll_req_insert(struct io_kiocb *req)
{
	struct io_hash_table *table = &req->ctx->cancel_table;
	u32 index = hash_long(req->cqe.user_data, table->hash_bits);
	struct io_hash_bucket *hb = &table->hbs[index];

	spin_lock(&hb->lock);
	hlist_add_head(&req->hash_node, &hb->list);
	spin_unlock(&hb->lock);
}

static void io_poll_req_delete(struct io_kiocb *req, struct io_ring_ctx *ctx)
{
	struct io_hash_table *table = &req->ctx->cancel_table;
	u32 index = hash_long(req->cqe.user_data, table->hash_bits);
	spinlock_t *lock = &table->hbs[index].lock;

	spin_lock(lock);
	hash_del(&req->hash_node);
	spin_unlock(lock);
}

static void io_poll_req_insert_locked(struct io_kiocb *req)
{
	struct io_hash_table *table = &req->ctx->cancel_table_locked;
	u32 index = hash_long(req->cqe.user_data, table->hash_bits);

	lockdep_assert_held(&req->ctx->uring_lock);

	hlist_add_head(&req->hash_node, &table->hbs[index].list);
}

static void io_poll_tw_hash_eject(struct io_kiocb *req, struct io_tw_state *ts)
{
	struct io_ring_ctx *ctx = req->ctx;

	if (req->flags & REQ_F_HASH_LOCKED) {
		 
		io_tw_lock(ctx, ts);
		hash_del(&req->hash_node);
		req->flags &= ~REQ_F_HASH_LOCKED;
	} else {
		io_poll_req_delete(req, ctx);
	}
}

static void io_init_poll_iocb(struct io_poll *poll, __poll_t events)
{
	poll->head = NULL;
#define IO_POLL_UNMASK	(EPOLLERR|EPOLLHUP|EPOLLNVAL|EPOLLRDHUP)
	 
	poll->events = events | IO_POLL_UNMASK;
	INIT_LIST_HEAD(&poll->wait.entry);
	init_waitqueue_func_entry(&poll->wait, io_poll_wake);
}

static inline void io_poll_remove_entry(struct io_poll *poll)
{
	struct wait_queue_head *head = smp_load_acquire(&poll->head);

	if (head) {
		spin_lock_irq(&head->lock);
		list_del_init(&poll->wait.entry);
		poll->head = NULL;
		spin_unlock_irq(&head->lock);
	}
}

static void io_poll_remove_entries(struct io_kiocb *req)
{
	 
	if (!(req->flags & (REQ_F_SINGLE_POLL | REQ_F_DOUBLE_POLL)))
		return;

	 
	rcu_read_lock();
	if (req->flags & REQ_F_SINGLE_POLL)
		io_poll_remove_entry(io_poll_get_single(req));
	if (req->flags & REQ_F_DOUBLE_POLL)
		io_poll_remove_entry(io_poll_get_double(req));
	rcu_read_unlock();
}

enum {
	IOU_POLL_DONE = 0,
	IOU_POLL_NO_ACTION = 1,
	IOU_POLL_REMOVE_POLL_USE_RES = 2,
	IOU_POLL_REISSUE = 3,
};

 
static int io_poll_check_events(struct io_kiocb *req, struct io_tw_state *ts)
{
	int v;

	 
	if (unlikely(req->task->flags & PF_EXITING))
		return -ECANCELED;

	do {
		v = atomic_read(&req->poll_refs);

		if (unlikely(v != 1)) {
			 
			if (WARN_ON_ONCE(!(v & IO_POLL_REF_MASK)))
				return IOU_POLL_NO_ACTION;
			if (v & IO_POLL_CANCEL_FLAG)
				return -ECANCELED;
			 
			if ((v & IO_POLL_REF_MASK) != 1)
				req->cqe.res = 0;

			if (v & IO_POLL_RETRY_FLAG) {
				req->cqe.res = 0;
				 
				atomic_andnot(IO_POLL_RETRY_FLAG, &req->poll_refs);
				v &= ~IO_POLL_RETRY_FLAG;
			}
		}

		 
		if (!req->cqe.res) {
			struct poll_table_struct pt = { ._key = req->apoll_events };
			req->cqe.res = vfs_poll(req->file, &pt) & req->apoll_events;
			 
			if (unlikely(!req->cqe.res)) {
				 
				if (!(req->apoll_events & EPOLLONESHOT))
					continue;
				return IOU_POLL_REISSUE;
			}
		}
		if (req->apoll_events & EPOLLONESHOT)
			return IOU_POLL_DONE;

		 
		if (!(req->flags & REQ_F_APOLL_MULTISHOT)) {
			__poll_t mask = mangle_poll(req->cqe.res &
						    req->apoll_events);

			if (!io_fill_cqe_req_aux(req, ts->locked, mask,
						 IORING_CQE_F_MORE)) {
				io_req_set_res(req, mask, 0);
				return IOU_POLL_REMOVE_POLL_USE_RES;
			}
		} else {
			int ret = io_poll_issue(req, ts);
			if (ret == IOU_STOP_MULTISHOT)
				return IOU_POLL_REMOVE_POLL_USE_RES;
			if (ret < 0)
				return ret;
		}

		 
		req->cqe.res = 0;

		 
	} while (atomic_sub_return(v & IO_POLL_REF_MASK, &req->poll_refs) &
					IO_POLL_REF_MASK);

	return IOU_POLL_NO_ACTION;
}

void io_poll_task_func(struct io_kiocb *req, struct io_tw_state *ts)
{
	int ret;

	ret = io_poll_check_events(req, ts);
	if (ret == IOU_POLL_NO_ACTION)
		return;
	io_poll_remove_entries(req);
	io_poll_tw_hash_eject(req, ts);

	if (req->opcode == IORING_OP_POLL_ADD) {
		if (ret == IOU_POLL_DONE) {
			struct io_poll *poll;

			poll = io_kiocb_to_cmd(req, struct io_poll);
			req->cqe.res = mangle_poll(req->cqe.res & poll->events);
		} else if (ret == IOU_POLL_REISSUE) {
			io_req_task_submit(req, ts);
			return;
		} else if (ret != IOU_POLL_REMOVE_POLL_USE_RES) {
			req->cqe.res = ret;
			req_set_fail(req);
		}

		io_req_set_res(req, req->cqe.res, 0);
		io_req_task_complete(req, ts);
	} else {
		io_tw_lock(req->ctx, ts);

		if (ret == IOU_POLL_REMOVE_POLL_USE_RES)
			io_req_task_complete(req, ts);
		else if (ret == IOU_POLL_DONE || ret == IOU_POLL_REISSUE)
			io_req_task_submit(req, ts);
		else
			io_req_defer_failed(req, ret);
	}
}

static void __io_poll_execute(struct io_kiocb *req, int mask)
{
	io_req_set_res(req, mask, 0);
	req->io_task_work.func = io_poll_task_func;

	trace_io_uring_task_add(req, mask);
	io_req_task_work_add(req);
}

static inline void io_poll_execute(struct io_kiocb *req, int res)
{
	if (io_poll_get_ownership(req))
		__io_poll_execute(req, res);
}

static void io_poll_cancel_req(struct io_kiocb *req)
{
	io_poll_mark_cancelled(req);
	 
	io_poll_execute(req, 0);
}

#define IO_ASYNC_POLL_COMMON	(EPOLLONESHOT | EPOLLPRI)

static __cold int io_pollfree_wake(struct io_kiocb *req, struct io_poll *poll)
{
	io_poll_mark_cancelled(req);
	 
	io_poll_execute(req, 0);

	 
	list_del_init(&poll->wait.entry);

	 
	smp_store_release(&poll->head, NULL);
	return 1;
}

static int io_poll_wake(struct wait_queue_entry *wait, unsigned mode, int sync,
			void *key)
{
	struct io_kiocb *req = wqe_to_req(wait);
	struct io_poll *poll = container_of(wait, struct io_poll, wait);
	__poll_t mask = key_to_poll(key);

	if (unlikely(mask & POLLFREE))
		return io_pollfree_wake(req, poll);

	 
	if (mask && !(mask & (poll->events & ~IO_ASYNC_POLL_COMMON)))
		return 0;

	if (io_poll_get_ownership(req)) {
		 
		if (mask & EPOLL_URING_WAKE)
			poll->events |= EPOLLONESHOT;

		 
		if (mask && poll->events & EPOLLONESHOT) {
			list_del_init(&poll->wait.entry);
			poll->head = NULL;
			if (wqe_is_double(wait))
				req->flags &= ~REQ_F_DOUBLE_POLL;
			else
				req->flags &= ~REQ_F_SINGLE_POLL;
		}
		__io_poll_execute(req, mask);
	}
	return 1;
}

 
static bool io_poll_double_prepare(struct io_kiocb *req)
{
	struct wait_queue_head *head;
	struct io_poll *poll = io_poll_get_single(req);

	 
	rcu_read_lock();
	head = smp_load_acquire(&poll->head);
	 
	if (head) {
		spin_lock_irq(&head->lock);
		req->flags |= REQ_F_DOUBLE_POLL;
		if (req->opcode == IORING_OP_POLL_ADD)
			req->flags |= REQ_F_ASYNC_DATA;
		spin_unlock_irq(&head->lock);
	}
	rcu_read_unlock();
	return !!head;
}

static void __io_queue_proc(struct io_poll *poll, struct io_poll_table *pt,
			    struct wait_queue_head *head,
			    struct io_poll **poll_ptr)
{
	struct io_kiocb *req = pt->req;
	unsigned long wqe_private = (unsigned long) req;

	 
	if (unlikely(pt->nr_entries)) {
		struct io_poll *first = poll;

		 
		if (first->head == head)
			return;
		 
		if (*poll_ptr) {
			if ((*poll_ptr)->head == head)
				return;
			pt->error = -EINVAL;
			return;
		}

		poll = kmalloc(sizeof(*poll), GFP_ATOMIC);
		if (!poll) {
			pt->error = -ENOMEM;
			return;
		}

		 
		wqe_private |= IO_WQE_F_DOUBLE;
		io_init_poll_iocb(poll, first->events);
		if (!io_poll_double_prepare(req)) {
			 
			kfree(poll);
			return;
		}
		*poll_ptr = poll;
	} else {
		 
		req->flags |= REQ_F_SINGLE_POLL;
	}

	pt->nr_entries++;
	poll->head = head;
	poll->wait.private = (void *) wqe_private;

	if (poll->events & EPOLLEXCLUSIVE)
		add_wait_queue_exclusive(head, &poll->wait);
	else
		add_wait_queue(head, &poll->wait);
}

static void io_poll_queue_proc(struct file *file, struct wait_queue_head *head,
			       struct poll_table_struct *p)
{
	struct io_poll_table *pt = container_of(p, struct io_poll_table, pt);
	struct io_poll *poll = io_kiocb_to_cmd(pt->req, struct io_poll);

	__io_queue_proc(poll, pt, head,
			(struct io_poll **) &pt->req->async_data);
}

static bool io_poll_can_finish_inline(struct io_kiocb *req,
				      struct io_poll_table *pt)
{
	return pt->owning || io_poll_get_ownership(req);
}

static void io_poll_add_hash(struct io_kiocb *req)
{
	if (req->flags & REQ_F_HASH_LOCKED)
		io_poll_req_insert_locked(req);
	else
		io_poll_req_insert(req);
}

 
static int __io_arm_poll_handler(struct io_kiocb *req,
				 struct io_poll *poll,
				 struct io_poll_table *ipt, __poll_t mask,
				 unsigned issue_flags)
{
	struct io_ring_ctx *ctx = req->ctx;

	INIT_HLIST_NODE(&req->hash_node);
	req->work.cancel_seq = atomic_read(&ctx->cancel_seq);
	io_init_poll_iocb(poll, mask);
	poll->file = req->file;
	req->apoll_events = poll->events;

	ipt->pt._key = mask;
	ipt->req = req;
	ipt->error = 0;
	ipt->nr_entries = 0;
	 
	ipt->owning = issue_flags & IO_URING_F_UNLOCKED;
	atomic_set(&req->poll_refs, (int)ipt->owning);

	 
	if (issue_flags & IO_URING_F_UNLOCKED)
		req->flags &= ~REQ_F_HASH_LOCKED;

	mask = vfs_poll(req->file, &ipt->pt) & poll->events;

	if (unlikely(ipt->error || !ipt->nr_entries)) {
		io_poll_remove_entries(req);

		if (!io_poll_can_finish_inline(req, ipt)) {
			io_poll_mark_cancelled(req);
			return 0;
		} else if (mask && (poll->events & EPOLLET)) {
			ipt->result_mask = mask;
			return 1;
		}
		return ipt->error ?: -EINVAL;
	}

	if (mask &&
	   ((poll->events & (EPOLLET|EPOLLONESHOT)) == (EPOLLET|EPOLLONESHOT))) {
		if (!io_poll_can_finish_inline(req, ipt)) {
			io_poll_add_hash(req);
			return 0;
		}
		io_poll_remove_entries(req);
		ipt->result_mask = mask;
		 
		return 1;
	}

	io_poll_add_hash(req);

	if (mask && (poll->events & EPOLLET) &&
	    io_poll_can_finish_inline(req, ipt)) {
		__io_poll_execute(req, mask);
		return 0;
	}

	if (ipt->owning) {
		 
		if (atomic_cmpxchg(&req->poll_refs, 1, 0) != 1)
			__io_poll_execute(req, 0);
	}
	return 0;
}

static void io_async_queue_proc(struct file *file, struct wait_queue_head *head,
			       struct poll_table_struct *p)
{
	struct io_poll_table *pt = container_of(p, struct io_poll_table, pt);
	struct async_poll *apoll = pt->req->apoll;

	__io_queue_proc(&apoll->poll, pt, head, &apoll->double_poll);
}

 
#define APOLL_MAX_RETRY		128

static struct async_poll *io_req_alloc_apoll(struct io_kiocb *req,
					     unsigned issue_flags)
{
	struct io_ring_ctx *ctx = req->ctx;
	struct io_cache_entry *entry;
	struct async_poll *apoll;

	if (req->flags & REQ_F_POLLED) {
		apoll = req->apoll;
		kfree(apoll->double_poll);
	} else if (!(issue_flags & IO_URING_F_UNLOCKED)) {
		entry = io_alloc_cache_get(&ctx->apoll_cache);
		if (entry == NULL)
			goto alloc_apoll;
		apoll = container_of(entry, struct async_poll, cache);
		apoll->poll.retries = APOLL_MAX_RETRY;
	} else {
alloc_apoll:
		apoll = kmalloc(sizeof(*apoll), GFP_ATOMIC);
		if (unlikely(!apoll))
			return NULL;
		apoll->poll.retries = APOLL_MAX_RETRY;
	}
	apoll->double_poll = NULL;
	req->apoll = apoll;
	if (unlikely(!--apoll->poll.retries))
		return NULL;
	return apoll;
}

int io_arm_poll_handler(struct io_kiocb *req, unsigned issue_flags)
{
	const struct io_issue_def *def = &io_issue_defs[req->opcode];
	struct async_poll *apoll;
	struct io_poll_table ipt;
	__poll_t mask = POLLPRI | POLLERR | EPOLLET;
	int ret;

	 
	req->flags |= REQ_F_HASH_LOCKED;

	if (!def->pollin && !def->pollout)
		return IO_APOLL_ABORTED;
	if (!file_can_poll(req->file))
		return IO_APOLL_ABORTED;
	if (!(req->flags & REQ_F_APOLL_MULTISHOT))
		mask |= EPOLLONESHOT;

	if (def->pollin) {
		mask |= EPOLLIN | EPOLLRDNORM;

		 
		if (req->flags & REQ_F_CLEAR_POLLIN)
			mask &= ~EPOLLIN;
	} else {
		mask |= EPOLLOUT | EPOLLWRNORM;
	}
	if (def->poll_exclusive)
		mask |= EPOLLEXCLUSIVE;

	apoll = io_req_alloc_apoll(req, issue_flags);
	if (!apoll)
		return IO_APOLL_ABORTED;
	req->flags &= ~(REQ_F_SINGLE_POLL | REQ_F_DOUBLE_POLL);
	req->flags |= REQ_F_POLLED;
	ipt.pt._qproc = io_async_queue_proc;

	io_kbuf_recycle(req, issue_flags);

	ret = __io_arm_poll_handler(req, &apoll->poll, &ipt, mask, issue_flags);
	if (ret)
		return ret > 0 ? IO_APOLL_READY : IO_APOLL_ABORTED;
	trace_io_uring_poll_arm(req, mask, apoll->poll.events);
	return IO_APOLL_OK;
}

static __cold bool io_poll_remove_all_table(struct task_struct *tsk,
					    struct io_hash_table *table,
					    bool cancel_all)
{
	unsigned nr_buckets = 1U << table->hash_bits;
	struct hlist_node *tmp;
	struct io_kiocb *req;
	bool found = false;
	int i;

	for (i = 0; i < nr_buckets; i++) {
		struct io_hash_bucket *hb = &table->hbs[i];

		spin_lock(&hb->lock);
		hlist_for_each_entry_safe(req, tmp, &hb->list, hash_node) {
			if (io_match_task_safe(req, tsk, cancel_all)) {
				hlist_del_init(&req->hash_node);
				io_poll_cancel_req(req);
				found = true;
			}
		}
		spin_unlock(&hb->lock);
	}
	return found;
}

 
__cold bool io_poll_remove_all(struct io_ring_ctx *ctx, struct task_struct *tsk,
			       bool cancel_all)
	__must_hold(&ctx->uring_lock)
{
	bool ret;

	ret = io_poll_remove_all_table(tsk, &ctx->cancel_table, cancel_all);
	ret |= io_poll_remove_all_table(tsk, &ctx->cancel_table_locked, cancel_all);
	return ret;
}

static struct io_kiocb *io_poll_find(struct io_ring_ctx *ctx, bool poll_only,
				     struct io_cancel_data *cd,
				     struct io_hash_table *table,
				     struct io_hash_bucket **out_bucket)
{
	struct io_kiocb *req;
	u32 index = hash_long(cd->data, table->hash_bits);
	struct io_hash_bucket *hb = &table->hbs[index];

	*out_bucket = NULL;

	spin_lock(&hb->lock);
	hlist_for_each_entry(req, &hb->list, hash_node) {
		if (cd->data != req->cqe.user_data)
			continue;
		if (poll_only && req->opcode != IORING_OP_POLL_ADD)
			continue;
		if (cd->flags & IORING_ASYNC_CANCEL_ALL) {
			if (cd->seq == req->work.cancel_seq)
				continue;
			req->work.cancel_seq = cd->seq;
		}
		*out_bucket = hb;
		return req;
	}
	spin_unlock(&hb->lock);
	return NULL;
}

static struct io_kiocb *io_poll_file_find(struct io_ring_ctx *ctx,
					  struct io_cancel_data *cd,
					  struct io_hash_table *table,
					  struct io_hash_bucket **out_bucket)
{
	unsigned nr_buckets = 1U << table->hash_bits;
	struct io_kiocb *req;
	int i;

	*out_bucket = NULL;

	for (i = 0; i < nr_buckets; i++) {
		struct io_hash_bucket *hb = &table->hbs[i];

		spin_lock(&hb->lock);
		hlist_for_each_entry(req, &hb->list, hash_node) {
			if (io_cancel_req_match(req, cd)) {
				*out_bucket = hb;
				return req;
			}
		}
		spin_unlock(&hb->lock);
	}
	return NULL;
}

static int io_poll_disarm(struct io_kiocb *req)
{
	if (!req)
		return -ENOENT;
	if (!io_poll_get_ownership(req))
		return -EALREADY;
	io_poll_remove_entries(req);
	hash_del(&req->hash_node);
	return 0;
}

static int __io_poll_cancel(struct io_ring_ctx *ctx, struct io_cancel_data *cd,
			    struct io_hash_table *table)
{
	struct io_hash_bucket *bucket;
	struct io_kiocb *req;

	if (cd->flags & (IORING_ASYNC_CANCEL_FD | IORING_ASYNC_CANCEL_OP |
			 IORING_ASYNC_CANCEL_ANY))
		req = io_poll_file_find(ctx, cd, table, &bucket);
	else
		req = io_poll_find(ctx, false, cd, table, &bucket);

	if (req)
		io_poll_cancel_req(req);
	if (bucket)
		spin_unlock(&bucket->lock);
	return req ? 0 : -ENOENT;
}

int io_poll_cancel(struct io_ring_ctx *ctx, struct io_cancel_data *cd,
		   unsigned issue_flags)
{
	int ret;

	ret = __io_poll_cancel(ctx, cd, &ctx->cancel_table);
	if (ret != -ENOENT)
		return ret;

	io_ring_submit_lock(ctx, issue_flags);
	ret = __io_poll_cancel(ctx, cd, &ctx->cancel_table_locked);
	io_ring_submit_unlock(ctx, issue_flags);
	return ret;
}

static __poll_t io_poll_parse_events(const struct io_uring_sqe *sqe,
				     unsigned int flags)
{
	u32 events;

	events = READ_ONCE(sqe->poll32_events);
#ifdef __BIG_ENDIAN
	events = swahw32(events);
#endif
	if (!(flags & IORING_POLL_ADD_MULTI))
		events |= EPOLLONESHOT;
	if (!(flags & IORING_POLL_ADD_LEVEL))
		events |= EPOLLET;
	return demangle_poll(events) |
		(events & (EPOLLEXCLUSIVE|EPOLLONESHOT|EPOLLET));
}

int io_poll_remove_prep(struct io_kiocb *req, const struct io_uring_sqe *sqe)
{
	struct io_poll_update *upd = io_kiocb_to_cmd(req, struct io_poll_update);
	u32 flags;

	if (sqe->buf_index || sqe->splice_fd_in)
		return -EINVAL;
	flags = READ_ONCE(sqe->len);
	if (flags & ~(IORING_POLL_UPDATE_EVENTS | IORING_POLL_UPDATE_USER_DATA |
		      IORING_POLL_ADD_MULTI))
		return -EINVAL;
	 
	if (flags == IORING_POLL_ADD_MULTI)
		return -EINVAL;

	upd->old_user_data = READ_ONCE(sqe->addr);
	upd->update_events = flags & IORING_POLL_UPDATE_EVENTS;
	upd->update_user_data = flags & IORING_POLL_UPDATE_USER_DATA;

	upd->new_user_data = READ_ONCE(sqe->off);
	if (!upd->update_user_data && upd->new_user_data)
		return -EINVAL;
	if (upd->update_events)
		upd->events = io_poll_parse_events(sqe, flags);
	else if (sqe->poll32_events)
		return -EINVAL;

	return 0;
}

int io_poll_add_prep(struct io_kiocb *req, const struct io_uring_sqe *sqe)
{
	struct io_poll *poll = io_kiocb_to_cmd(req, struct io_poll);
	u32 flags;

	if (sqe->buf_index || sqe->off || sqe->addr)
		return -EINVAL;
	flags = READ_ONCE(sqe->len);
	if (flags & ~IORING_POLL_ADD_MULTI)
		return -EINVAL;
	if ((flags & IORING_POLL_ADD_MULTI) && (req->flags & REQ_F_CQE_SKIP))
		return -EINVAL;

	poll->events = io_poll_parse_events(sqe, flags);
	return 0;
}

int io_poll_add(struct io_kiocb *req, unsigned int issue_flags)
{
	struct io_poll *poll = io_kiocb_to_cmd(req, struct io_poll);
	struct io_poll_table ipt;
	int ret;

	ipt.pt._qproc = io_poll_queue_proc;

	 
	if (req->ctx->flags & (IORING_SETUP_SQPOLL|IORING_SETUP_SINGLE_ISSUER))
		req->flags |= REQ_F_HASH_LOCKED;

	ret = __io_arm_poll_handler(req, poll, &ipt, poll->events, issue_flags);
	if (ret > 0) {
		io_req_set_res(req, ipt.result_mask, 0);
		return IOU_OK;
	}
	return ret ?: IOU_ISSUE_SKIP_COMPLETE;
}

int io_poll_remove(struct io_kiocb *req, unsigned int issue_flags)
{
	struct io_poll_update *poll_update = io_kiocb_to_cmd(req, struct io_poll_update);
	struct io_ring_ctx *ctx = req->ctx;
	struct io_cancel_data cd = { .ctx = ctx, .data = poll_update->old_user_data, };
	struct io_hash_bucket *bucket;
	struct io_kiocb *preq;
	int ret2, ret = 0;
	struct io_tw_state ts = { .locked = true };

	io_ring_submit_lock(ctx, issue_flags);
	preq = io_poll_find(ctx, true, &cd, &ctx->cancel_table, &bucket);
	ret2 = io_poll_disarm(preq);
	if (bucket)
		spin_unlock(&bucket->lock);
	if (!ret2)
		goto found;
	if (ret2 != -ENOENT) {
		ret = ret2;
		goto out;
	}

	preq = io_poll_find(ctx, true, &cd, &ctx->cancel_table_locked, &bucket);
	ret2 = io_poll_disarm(preq);
	if (bucket)
		spin_unlock(&bucket->lock);
	if (ret2) {
		ret = ret2;
		goto out;
	}

found:
	if (WARN_ON_ONCE(preq->opcode != IORING_OP_POLL_ADD)) {
		ret = -EFAULT;
		goto out;
	}

	if (poll_update->update_events || poll_update->update_user_data) {
		 
		if (poll_update->update_events) {
			struct io_poll *poll = io_kiocb_to_cmd(preq, struct io_poll);

			poll->events &= ~0xffff;
			poll->events |= poll_update->events & 0xffff;
			poll->events |= IO_POLL_UNMASK;
		}
		if (poll_update->update_user_data)
			preq->cqe.user_data = poll_update->new_user_data;

		ret2 = io_poll_add(preq, issue_flags & ~IO_URING_F_UNLOCKED);
		 
		if (!ret2 || ret2 == -EIOCBQUEUED)
			goto out;
	}

	req_set_fail(preq);
	io_req_set_res(preq, -ECANCELED, 0);
	io_req_task_complete(preq, &ts);
out:
	io_ring_submit_unlock(ctx, issue_flags);
	if (ret < 0) {
		req_set_fail(req);
		return ret;
	}
	 
	io_req_set_res(req, ret, 0);
	return IOU_OK;
}

void io_apoll_cache_free(struct io_cache_entry *entry)
{
	kfree(container_of(entry, struct async_poll, cache));
}
