
 
#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/blkdev.h>
#include <linux/bio.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/init.h>
#include <linux/compiler.h>
#include <linux/rbtree.h>
#include <linux/sbitmap.h>

#include <trace/events/block.h>

#include "elevator.h"
#include "blk.h"
#include "blk-mq.h"
#include "blk-mq-debugfs.h"
#include "blk-mq-sched.h"

 
static const int read_expire = HZ / 2;   
static const int write_expire = 5 * HZ;  
 
static const int prio_aging_expire = 10 * HZ;
static const int writes_starved = 2;     
static const int fifo_batch = 16;        

enum dd_data_dir {
	DD_READ		= READ,
	DD_WRITE	= WRITE,
};

enum { DD_DIR_COUNT = 2 };

enum dd_prio {
	DD_RT_PRIO	= 0,
	DD_BE_PRIO	= 1,
	DD_IDLE_PRIO	= 2,
	DD_PRIO_MAX	= 2,
};

enum { DD_PRIO_COUNT = 3 };

 
struct io_stats_per_prio {
	uint32_t inserted;
	uint32_t merged;
	uint32_t dispatched;
	atomic_t completed;
};

 
struct dd_per_prio {
	struct list_head dispatch;
	struct rb_root sort_list[DD_DIR_COUNT];
	struct list_head fifo_list[DD_DIR_COUNT];
	 
	sector_t latest_pos[DD_DIR_COUNT];
	struct io_stats_per_prio stats;
};

struct deadline_data {
	 

	struct dd_per_prio per_prio[DD_PRIO_COUNT];

	 
	enum dd_data_dir last_dir;
	unsigned int batching;		 
	unsigned int starved;		 

	 
	int fifo_expire[DD_DIR_COUNT];
	int fifo_batch;
	int writes_starved;
	int front_merges;
	u32 async_depth;
	int prio_aging_expire;

	spinlock_t lock;
	spinlock_t zone_lock;
};

 
static const enum dd_prio ioprio_class_to_prio[] = {
	[IOPRIO_CLASS_NONE]	= DD_BE_PRIO,
	[IOPRIO_CLASS_RT]	= DD_RT_PRIO,
	[IOPRIO_CLASS_BE]	= DD_BE_PRIO,
	[IOPRIO_CLASS_IDLE]	= DD_IDLE_PRIO,
};

static inline struct rb_root *
deadline_rb_root(struct dd_per_prio *per_prio, struct request *rq)
{
	return &per_prio->sort_list[rq_data_dir(rq)];
}

 
static u8 dd_rq_ioclass(struct request *rq)
{
	return IOPRIO_PRIO_CLASS(req_get_ioprio(rq));
}

 
static inline struct request *
deadline_earlier_request(struct request *rq)
{
	struct rb_node *node = rb_prev(&rq->rb_node);

	if (node)
		return rb_entry_rq(node);

	return NULL;
}

 
static inline struct request *
deadline_latter_request(struct request *rq)
{
	struct rb_node *node = rb_next(&rq->rb_node);

	if (node)
		return rb_entry_rq(node);

	return NULL;
}

 
static inline struct request *deadline_from_pos(struct dd_per_prio *per_prio,
				enum dd_data_dir data_dir, sector_t pos)
{
	struct rb_node *node = per_prio->sort_list[data_dir].rb_node;
	struct request *rq, *res = NULL;

	if (!node)
		return NULL;

	rq = rb_entry_rq(node);
	 
	if (blk_rq_is_seq_zoned_write(rq))
		pos = round_down(pos, rq->q->limits.chunk_sectors);

	while (node) {
		rq = rb_entry_rq(node);
		if (blk_rq_pos(rq) >= pos) {
			res = rq;
			node = node->rb_left;
		} else {
			node = node->rb_right;
		}
	}
	return res;
}

static void
deadline_add_rq_rb(struct dd_per_prio *per_prio, struct request *rq)
{
	struct rb_root *root = deadline_rb_root(per_prio, rq);

	elv_rb_add(root, rq);
}

static inline void
deadline_del_rq_rb(struct dd_per_prio *per_prio, struct request *rq)
{
	elv_rb_del(deadline_rb_root(per_prio, rq), rq);
}

 
static void deadline_remove_request(struct request_queue *q,
				    struct dd_per_prio *per_prio,
				    struct request *rq)
{
	list_del_init(&rq->queuelist);

	 
	if (!RB_EMPTY_NODE(&rq->rb_node))
		deadline_del_rq_rb(per_prio, rq);

	elv_rqhash_del(q, rq);
	if (q->last_merge == rq)
		q->last_merge = NULL;
}

static void dd_request_merged(struct request_queue *q, struct request *req,
			      enum elv_merge type)
{
	struct deadline_data *dd = q->elevator->elevator_data;
	const u8 ioprio_class = dd_rq_ioclass(req);
	const enum dd_prio prio = ioprio_class_to_prio[ioprio_class];
	struct dd_per_prio *per_prio = &dd->per_prio[prio];

	 
	if (type == ELEVATOR_FRONT_MERGE) {
		elv_rb_del(deadline_rb_root(per_prio, req), req);
		deadline_add_rq_rb(per_prio, req);
	}
}

 
static void dd_merged_requests(struct request_queue *q, struct request *req,
			       struct request *next)
{
	struct deadline_data *dd = q->elevator->elevator_data;
	const u8 ioprio_class = dd_rq_ioclass(next);
	const enum dd_prio prio = ioprio_class_to_prio[ioprio_class];

	lockdep_assert_held(&dd->lock);

	dd->per_prio[prio].stats.merged++;

	 
	if (!list_empty(&req->queuelist) && !list_empty(&next->queuelist)) {
		if (time_before((unsigned long)next->fifo_time,
				(unsigned long)req->fifo_time)) {
			list_move(&req->queuelist, &next->queuelist);
			req->fifo_time = next->fifo_time;
		}
	}

	 
	deadline_remove_request(q, &dd->per_prio[prio], next);
}

 
static void
deadline_move_request(struct deadline_data *dd, struct dd_per_prio *per_prio,
		      struct request *rq)
{
	 
	deadline_remove_request(rq->q, per_prio, rq);
}

 
static u32 dd_queued(struct deadline_data *dd, enum dd_prio prio)
{
	const struct io_stats_per_prio *stats = &dd->per_prio[prio].stats;

	lockdep_assert_held(&dd->lock);

	return stats->inserted - atomic_read(&stats->completed);
}

 
static inline bool deadline_check_fifo(struct dd_per_prio *per_prio,
				       enum dd_data_dir data_dir)
{
	struct request *rq = rq_entry_fifo(per_prio->fifo_list[data_dir].next);

	return time_is_before_eq_jiffies((unsigned long)rq->fifo_time);
}

 
static bool deadline_is_seq_write(struct deadline_data *dd, struct request *rq)
{
	struct request *prev = deadline_earlier_request(rq);

	if (!prev)
		return false;

	return blk_rq_pos(prev) + blk_rq_sectors(prev) == blk_rq_pos(rq);
}

 
static struct request *deadline_skip_seq_writes(struct deadline_data *dd,
						struct request *rq)
{
	sector_t pos = blk_rq_pos(rq);

	do {
		pos += blk_rq_sectors(rq);
		rq = deadline_latter_request(rq);
	} while (rq && blk_rq_pos(rq) == pos);

	return rq;
}

 
static struct request *
deadline_fifo_request(struct deadline_data *dd, struct dd_per_prio *per_prio,
		      enum dd_data_dir data_dir)
{
	struct request *rq, *rb_rq, *next;
	unsigned long flags;

	if (list_empty(&per_prio->fifo_list[data_dir]))
		return NULL;

	rq = rq_entry_fifo(per_prio->fifo_list[data_dir].next);
	if (data_dir == DD_READ || !blk_queue_is_zoned(rq->q))
		return rq;

	 
	spin_lock_irqsave(&dd->zone_lock, flags);
	list_for_each_entry_safe(rq, next, &per_prio->fifo_list[DD_WRITE],
				 queuelist) {
		 
		rb_rq = deadline_from_pos(per_prio, data_dir, blk_rq_pos(rq));
		if (rb_rq && blk_rq_pos(rb_rq) < blk_rq_pos(rq))
			rq = rb_rq;
		if (blk_req_can_dispatch_to_zone(rq) &&
		    (blk_queue_nonrot(rq->q) ||
		     !deadline_is_seq_write(dd, rq)))
			goto out;
	}
	rq = NULL;
out:
	spin_unlock_irqrestore(&dd->zone_lock, flags);

	return rq;
}

 
static struct request *
deadline_next_request(struct deadline_data *dd, struct dd_per_prio *per_prio,
		      enum dd_data_dir data_dir)
{
	struct request *rq;
	unsigned long flags;

	rq = deadline_from_pos(per_prio, data_dir,
			       per_prio->latest_pos[data_dir]);
	if (!rq)
		return NULL;

	if (data_dir == DD_READ || !blk_queue_is_zoned(rq->q))
		return rq;

	 
	spin_lock_irqsave(&dd->zone_lock, flags);
	while (rq) {
		if (blk_req_can_dispatch_to_zone(rq))
			break;
		if (blk_queue_nonrot(rq->q))
			rq = deadline_latter_request(rq);
		else
			rq = deadline_skip_seq_writes(dd, rq);
	}
	spin_unlock_irqrestore(&dd->zone_lock, flags);

	return rq;
}

 
static bool started_after(struct deadline_data *dd, struct request *rq,
			  unsigned long latest_start)
{
	unsigned long start_time = (unsigned long)rq->fifo_time;

	start_time -= dd->fifo_expire[rq_data_dir(rq)];

	return time_after(start_time, latest_start);
}

 
static struct request *__dd_dispatch_request(struct deadline_data *dd,
					     struct dd_per_prio *per_prio,
					     unsigned long latest_start)
{
	struct request *rq, *next_rq;
	enum dd_data_dir data_dir;
	enum dd_prio prio;
	u8 ioprio_class;

	lockdep_assert_held(&dd->lock);

	if (!list_empty(&per_prio->dispatch)) {
		rq = list_first_entry(&per_prio->dispatch, struct request,
				      queuelist);
		if (started_after(dd, rq, latest_start))
			return NULL;
		list_del_init(&rq->queuelist);
		data_dir = rq_data_dir(rq);
		goto done;
	}

	 
	rq = deadline_next_request(dd, per_prio, dd->last_dir);
	if (rq && dd->batching < dd->fifo_batch) {
		 
		data_dir = rq_data_dir(rq);
		goto dispatch_request;
	}

	 

	if (!list_empty(&per_prio->fifo_list[DD_READ])) {
		BUG_ON(RB_EMPTY_ROOT(&per_prio->sort_list[DD_READ]));

		if (deadline_fifo_request(dd, per_prio, DD_WRITE) &&
		    (dd->starved++ >= dd->writes_starved))
			goto dispatch_writes;

		data_dir = DD_READ;

		goto dispatch_find_request;
	}

	 

	if (!list_empty(&per_prio->fifo_list[DD_WRITE])) {
dispatch_writes:
		BUG_ON(RB_EMPTY_ROOT(&per_prio->sort_list[DD_WRITE]));

		dd->starved = 0;

		data_dir = DD_WRITE;

		goto dispatch_find_request;
	}

	return NULL;

dispatch_find_request:
	 
	next_rq = deadline_next_request(dd, per_prio, data_dir);
	if (deadline_check_fifo(per_prio, data_dir) || !next_rq) {
		 
		rq = deadline_fifo_request(dd, per_prio, data_dir);
	} else {
		 
		rq = next_rq;
	}

	 
	if (!rq)
		return NULL;

	dd->last_dir = data_dir;
	dd->batching = 0;

dispatch_request:
	if (started_after(dd, rq, latest_start))
		return NULL;

	 
	dd->batching++;
	deadline_move_request(dd, per_prio, rq);
done:
	ioprio_class = dd_rq_ioclass(rq);
	prio = ioprio_class_to_prio[ioprio_class];
	dd->per_prio[prio].latest_pos[data_dir] = blk_rq_pos(rq);
	dd->per_prio[prio].stats.dispatched++;
	 
	blk_req_zone_write_lock(rq);
	rq->rq_flags |= RQF_STARTED;
	return rq;
}

 
static struct request *dd_dispatch_prio_aged_requests(struct deadline_data *dd,
						      unsigned long now)
{
	struct request *rq;
	enum dd_prio prio;
	int prio_cnt;

	lockdep_assert_held(&dd->lock);

	prio_cnt = !!dd_queued(dd, DD_RT_PRIO) + !!dd_queued(dd, DD_BE_PRIO) +
		   !!dd_queued(dd, DD_IDLE_PRIO);
	if (prio_cnt < 2)
		return NULL;

	for (prio = DD_BE_PRIO; prio <= DD_PRIO_MAX; prio++) {
		rq = __dd_dispatch_request(dd, &dd->per_prio[prio],
					   now - dd->prio_aging_expire);
		if (rq)
			return rq;
	}

	return NULL;
}

 
static struct request *dd_dispatch_request(struct blk_mq_hw_ctx *hctx)
{
	struct deadline_data *dd = hctx->queue->elevator->elevator_data;
	const unsigned long now = jiffies;
	struct request *rq;
	enum dd_prio prio;

	spin_lock(&dd->lock);
	rq = dd_dispatch_prio_aged_requests(dd, now);
	if (rq)
		goto unlock;

	 
	for (prio = 0; prio <= DD_PRIO_MAX; prio++) {
		rq = __dd_dispatch_request(dd, &dd->per_prio[prio], now);
		if (rq || dd_queued(dd, prio))
			break;
	}

unlock:
	spin_unlock(&dd->lock);

	return rq;
}

 
static void dd_limit_depth(blk_opf_t opf, struct blk_mq_alloc_data *data)
{
	struct deadline_data *dd = data->q->elevator->elevator_data;

	 
	if (op_is_sync(opf) && !op_is_write(opf))
		return;

	 
	data->shallow_depth = dd->async_depth;
}

 
static void dd_depth_updated(struct blk_mq_hw_ctx *hctx)
{
	struct request_queue *q = hctx->queue;
	struct deadline_data *dd = q->elevator->elevator_data;
	struct blk_mq_tags *tags = hctx->sched_tags;
	unsigned int shift = tags->bitmap_tags.sb.shift;

	dd->async_depth = max(1U, 3 * (1U << shift)  / 4);

	sbitmap_queue_min_shallow_depth(&tags->bitmap_tags, dd->async_depth);
}

 
static int dd_init_hctx(struct blk_mq_hw_ctx *hctx, unsigned int hctx_idx)
{
	dd_depth_updated(hctx);
	return 0;
}

static void dd_exit_sched(struct elevator_queue *e)
{
	struct deadline_data *dd = e->elevator_data;
	enum dd_prio prio;

	for (prio = 0; prio <= DD_PRIO_MAX; prio++) {
		struct dd_per_prio *per_prio = &dd->per_prio[prio];
		const struct io_stats_per_prio *stats = &per_prio->stats;
		uint32_t queued;

		WARN_ON_ONCE(!list_empty(&per_prio->fifo_list[DD_READ]));
		WARN_ON_ONCE(!list_empty(&per_prio->fifo_list[DD_WRITE]));

		spin_lock(&dd->lock);
		queued = dd_queued(dd, prio);
		spin_unlock(&dd->lock);

		WARN_ONCE(queued != 0,
			  "statistics for priority %d: i %u m %u d %u c %u\n",
			  prio, stats->inserted, stats->merged,
			  stats->dispatched, atomic_read(&stats->completed));
	}

	kfree(dd);
}

 
static int dd_init_sched(struct request_queue *q, struct elevator_type *e)
{
	struct deadline_data *dd;
	struct elevator_queue *eq;
	enum dd_prio prio;
	int ret = -ENOMEM;

	eq = elevator_alloc(q, e);
	if (!eq)
		return ret;

	dd = kzalloc_node(sizeof(*dd), GFP_KERNEL, q->node);
	if (!dd)
		goto put_eq;

	eq->elevator_data = dd;

	for (prio = 0; prio <= DD_PRIO_MAX; prio++) {
		struct dd_per_prio *per_prio = &dd->per_prio[prio];

		INIT_LIST_HEAD(&per_prio->dispatch);
		INIT_LIST_HEAD(&per_prio->fifo_list[DD_READ]);
		INIT_LIST_HEAD(&per_prio->fifo_list[DD_WRITE]);
		per_prio->sort_list[DD_READ] = RB_ROOT;
		per_prio->sort_list[DD_WRITE] = RB_ROOT;
	}
	dd->fifo_expire[DD_READ] = read_expire;
	dd->fifo_expire[DD_WRITE] = write_expire;
	dd->writes_starved = writes_starved;
	dd->front_merges = 1;
	dd->last_dir = DD_WRITE;
	dd->fifo_batch = fifo_batch;
	dd->prio_aging_expire = prio_aging_expire;
	spin_lock_init(&dd->lock);
	spin_lock_init(&dd->zone_lock);

	 
	blk_queue_flag_set(QUEUE_FLAG_SQ_SCHED, q);

	q->elevator = eq;
	return 0;

put_eq:
	kobject_put(&eq->kobj);
	return ret;
}

 
static int dd_request_merge(struct request_queue *q, struct request **rq,
			    struct bio *bio)
{
	struct deadline_data *dd = q->elevator->elevator_data;
	const u8 ioprio_class = IOPRIO_PRIO_CLASS(bio->bi_ioprio);
	const enum dd_prio prio = ioprio_class_to_prio[ioprio_class];
	struct dd_per_prio *per_prio = &dd->per_prio[prio];
	sector_t sector = bio_end_sector(bio);
	struct request *__rq;

	if (!dd->front_merges)
		return ELEVATOR_NO_MERGE;

	__rq = elv_rb_find(&per_prio->sort_list[bio_data_dir(bio)], sector);
	if (__rq) {
		BUG_ON(sector != blk_rq_pos(__rq));

		if (elv_bio_merge_ok(__rq, bio)) {
			*rq = __rq;
			if (blk_discard_mergable(__rq))
				return ELEVATOR_DISCARD_MERGE;
			return ELEVATOR_FRONT_MERGE;
		}
	}

	return ELEVATOR_NO_MERGE;
}

 
static bool dd_bio_merge(struct request_queue *q, struct bio *bio,
		unsigned int nr_segs)
{
	struct deadline_data *dd = q->elevator->elevator_data;
	struct request *free = NULL;
	bool ret;

	spin_lock(&dd->lock);
	ret = blk_mq_sched_try_merge(q, bio, nr_segs, &free);
	spin_unlock(&dd->lock);

	if (free)
		blk_mq_free_request(free);

	return ret;
}

 
static void dd_insert_request(struct blk_mq_hw_ctx *hctx, struct request *rq,
			      blk_insert_t flags, struct list_head *free)
{
	struct request_queue *q = hctx->queue;
	struct deadline_data *dd = q->elevator->elevator_data;
	const enum dd_data_dir data_dir = rq_data_dir(rq);
	u16 ioprio = req_get_ioprio(rq);
	u8 ioprio_class = IOPRIO_PRIO_CLASS(ioprio);
	struct dd_per_prio *per_prio;
	enum dd_prio prio;

	lockdep_assert_held(&dd->lock);

	 
	blk_req_zone_write_unlock(rq);

	prio = ioprio_class_to_prio[ioprio_class];
	per_prio = &dd->per_prio[prio];
	if (!rq->elv.priv[0]) {
		per_prio->stats.inserted++;
		rq->elv.priv[0] = (void *)(uintptr_t)1;
	}

	if (blk_mq_sched_try_insert_merge(q, rq, free))
		return;

	trace_block_rq_insert(rq);

	if (flags & BLK_MQ_INSERT_AT_HEAD) {
		list_add(&rq->queuelist, &per_prio->dispatch);
		rq->fifo_time = jiffies;
	} else {
		struct list_head *insert_before;

		deadline_add_rq_rb(per_prio, rq);

		if (rq_mergeable(rq)) {
			elv_rqhash_add(q, rq);
			if (!q->last_merge)
				q->last_merge = rq;
		}

		 
		rq->fifo_time = jiffies + dd->fifo_expire[data_dir];
		insert_before = &per_prio->fifo_list[data_dir];
#ifdef CONFIG_BLK_DEV_ZONED
		 
		if (blk_rq_is_seq_zoned_write(rq)) {
			struct request *rq2 = deadline_latter_request(rq);

			if (rq2 && blk_rq_zone_no(rq2) == blk_rq_zone_no(rq))
				insert_before = &rq2->queuelist;
		}
#endif
		list_add_tail(&rq->queuelist, insert_before);
	}
}

 
static void dd_insert_requests(struct blk_mq_hw_ctx *hctx,
			       struct list_head *list,
			       blk_insert_t flags)
{
	struct request_queue *q = hctx->queue;
	struct deadline_data *dd = q->elevator->elevator_data;
	LIST_HEAD(free);

	spin_lock(&dd->lock);
	while (!list_empty(list)) {
		struct request *rq;

		rq = list_first_entry(list, struct request, queuelist);
		list_del_init(&rq->queuelist);
		dd_insert_request(hctx, rq, flags, &free);
	}
	spin_unlock(&dd->lock);

	blk_mq_free_requests(&free);
}

 
static void dd_prepare_request(struct request *rq)
{
	rq->elv.priv[0] = NULL;
}

static bool dd_has_write_work(struct blk_mq_hw_ctx *hctx)
{
	struct deadline_data *dd = hctx->queue->elevator->elevator_data;
	enum dd_prio p;

	for (p = 0; p <= DD_PRIO_MAX; p++)
		if (!list_empty_careful(&dd->per_prio[p].fifo_list[DD_WRITE]))
			return true;

	return false;
}

 
static void dd_finish_request(struct request *rq)
{
	struct request_queue *q = rq->q;
	struct deadline_data *dd = q->elevator->elevator_data;
	const u8 ioprio_class = dd_rq_ioclass(rq);
	const enum dd_prio prio = ioprio_class_to_prio[ioprio_class];
	struct dd_per_prio *per_prio = &dd->per_prio[prio];

	 
	if (!rq->elv.priv[0])
		return;

	atomic_inc(&per_prio->stats.completed);

	if (blk_queue_is_zoned(q)) {
		unsigned long flags;

		spin_lock_irqsave(&dd->zone_lock, flags);
		blk_req_zone_write_unlock(rq);
		spin_unlock_irqrestore(&dd->zone_lock, flags);

		if (dd_has_write_work(rq->mq_hctx))
			blk_mq_sched_mark_restart_hctx(rq->mq_hctx);
	}
}

static bool dd_has_work_for_prio(struct dd_per_prio *per_prio)
{
	return !list_empty_careful(&per_prio->dispatch) ||
		!list_empty_careful(&per_prio->fifo_list[DD_READ]) ||
		!list_empty_careful(&per_prio->fifo_list[DD_WRITE]);
}

static bool dd_has_work(struct blk_mq_hw_ctx *hctx)
{
	struct deadline_data *dd = hctx->queue->elevator->elevator_data;
	enum dd_prio prio;

	for (prio = 0; prio <= DD_PRIO_MAX; prio++)
		if (dd_has_work_for_prio(&dd->per_prio[prio]))
			return true;

	return false;
}

 
#define SHOW_INT(__FUNC, __VAR)						\
static ssize_t __FUNC(struct elevator_queue *e, char *page)		\
{									\
	struct deadline_data *dd = e->elevator_data;			\
									\
	return sysfs_emit(page, "%d\n", __VAR);				\
}
#define SHOW_JIFFIES(__FUNC, __VAR) SHOW_INT(__FUNC, jiffies_to_msecs(__VAR))
SHOW_JIFFIES(deadline_read_expire_show, dd->fifo_expire[DD_READ]);
SHOW_JIFFIES(deadline_write_expire_show, dd->fifo_expire[DD_WRITE]);
SHOW_JIFFIES(deadline_prio_aging_expire_show, dd->prio_aging_expire);
SHOW_INT(deadline_writes_starved_show, dd->writes_starved);
SHOW_INT(deadline_front_merges_show, dd->front_merges);
SHOW_INT(deadline_async_depth_show, dd->async_depth);
SHOW_INT(deadline_fifo_batch_show, dd->fifo_batch);
#undef SHOW_INT
#undef SHOW_JIFFIES

#define STORE_FUNCTION(__FUNC, __PTR, MIN, MAX, __CONV)			\
static ssize_t __FUNC(struct elevator_queue *e, const char *page, size_t count)	\
{									\
	struct deadline_data *dd = e->elevator_data;			\
	int __data, __ret;						\
									\
	__ret = kstrtoint(page, 0, &__data);				\
	if (__ret < 0)							\
		return __ret;						\
	if (__data < (MIN))						\
		__data = (MIN);						\
	else if (__data > (MAX))					\
		__data = (MAX);						\
	*(__PTR) = __CONV(__data);					\
	return count;							\
}
#define STORE_INT(__FUNC, __PTR, MIN, MAX)				\
	STORE_FUNCTION(__FUNC, __PTR, MIN, MAX, )
#define STORE_JIFFIES(__FUNC, __PTR, MIN, MAX)				\
	STORE_FUNCTION(__FUNC, __PTR, MIN, MAX, msecs_to_jiffies)
STORE_JIFFIES(deadline_read_expire_store, &dd->fifo_expire[DD_READ], 0, INT_MAX);
STORE_JIFFIES(deadline_write_expire_store, &dd->fifo_expire[DD_WRITE], 0, INT_MAX);
STORE_JIFFIES(deadline_prio_aging_expire_store, &dd->prio_aging_expire, 0, INT_MAX);
STORE_INT(deadline_writes_starved_store, &dd->writes_starved, INT_MIN, INT_MAX);
STORE_INT(deadline_front_merges_store, &dd->front_merges, 0, 1);
STORE_INT(deadline_async_depth_store, &dd->async_depth, 1, INT_MAX);
STORE_INT(deadline_fifo_batch_store, &dd->fifo_batch, 0, INT_MAX);
#undef STORE_FUNCTION
#undef STORE_INT
#undef STORE_JIFFIES

#define DD_ATTR(name) \
	__ATTR(name, 0644, deadline_##name##_show, deadline_##name##_store)

static struct elv_fs_entry deadline_attrs[] = {
	DD_ATTR(read_expire),
	DD_ATTR(write_expire),
	DD_ATTR(writes_starved),
	DD_ATTR(front_merges),
	DD_ATTR(async_depth),
	DD_ATTR(fifo_batch),
	DD_ATTR(prio_aging_expire),
	__ATTR_NULL
};

#ifdef CONFIG_BLK_DEBUG_FS
#define DEADLINE_DEBUGFS_DDIR_ATTRS(prio, data_dir, name)		\
static void *deadline_##name##_fifo_start(struct seq_file *m,		\
					  loff_t *pos)			\
	__acquires(&dd->lock)						\
{									\
	struct request_queue *q = m->private;				\
	struct deadline_data *dd = q->elevator->elevator_data;		\
	struct dd_per_prio *per_prio = &dd->per_prio[prio];		\
									\
	spin_lock(&dd->lock);						\
	return seq_list_start(&per_prio->fifo_list[data_dir], *pos);	\
}									\
									\
static void *deadline_##name##_fifo_next(struct seq_file *m, void *v,	\
					 loff_t *pos)			\
{									\
	struct request_queue *q = m->private;				\
	struct deadline_data *dd = q->elevator->elevator_data;		\
	struct dd_per_prio *per_prio = &dd->per_prio[prio];		\
									\
	return seq_list_next(v, &per_prio->fifo_list[data_dir], pos);	\
}									\
									\
static void deadline_##name##_fifo_stop(struct seq_file *m, void *v)	\
	__releases(&dd->lock)						\
{									\
	struct request_queue *q = m->private;				\
	struct deadline_data *dd = q->elevator->elevator_data;		\
									\
	spin_unlock(&dd->lock);						\
}									\
									\
static const struct seq_operations deadline_##name##_fifo_seq_ops = {	\
	.start	= deadline_##name##_fifo_start,				\
	.next	= deadline_##name##_fifo_next,				\
	.stop	= deadline_##name##_fifo_stop,				\
	.show	= blk_mq_debugfs_rq_show,				\
};									\
									\
static int deadline_##name##_next_rq_show(void *data,			\
					  struct seq_file *m)		\
{									\
	struct request_queue *q = data;					\
	struct deadline_data *dd = q->elevator->elevator_data;		\
	struct dd_per_prio *per_prio = &dd->per_prio[prio];		\
	struct request *rq;						\
									\
	rq = deadline_from_pos(per_prio, data_dir,			\
			       per_prio->latest_pos[data_dir]);		\
	if (rq)								\
		__blk_mq_debugfs_rq_show(m, rq);			\
	return 0;							\
}

DEADLINE_DEBUGFS_DDIR_ATTRS(DD_RT_PRIO, DD_READ, read0);
DEADLINE_DEBUGFS_DDIR_ATTRS(DD_RT_PRIO, DD_WRITE, write0);
DEADLINE_DEBUGFS_DDIR_ATTRS(DD_BE_PRIO, DD_READ, read1);
DEADLINE_DEBUGFS_DDIR_ATTRS(DD_BE_PRIO, DD_WRITE, write1);
DEADLINE_DEBUGFS_DDIR_ATTRS(DD_IDLE_PRIO, DD_READ, read2);
DEADLINE_DEBUGFS_DDIR_ATTRS(DD_IDLE_PRIO, DD_WRITE, write2);
#undef DEADLINE_DEBUGFS_DDIR_ATTRS

static int deadline_batching_show(void *data, struct seq_file *m)
{
	struct request_queue *q = data;
	struct deadline_data *dd = q->elevator->elevator_data;

	seq_printf(m, "%u\n", dd->batching);
	return 0;
}

static int deadline_starved_show(void *data, struct seq_file *m)
{
	struct request_queue *q = data;
	struct deadline_data *dd = q->elevator->elevator_data;

	seq_printf(m, "%u\n", dd->starved);
	return 0;
}

static int dd_async_depth_show(void *data, struct seq_file *m)
{
	struct request_queue *q = data;
	struct deadline_data *dd = q->elevator->elevator_data;

	seq_printf(m, "%u\n", dd->async_depth);
	return 0;
}

static int dd_queued_show(void *data, struct seq_file *m)
{
	struct request_queue *q = data;
	struct deadline_data *dd = q->elevator->elevator_data;
	u32 rt, be, idle;

	spin_lock(&dd->lock);
	rt = dd_queued(dd, DD_RT_PRIO);
	be = dd_queued(dd, DD_BE_PRIO);
	idle = dd_queued(dd, DD_IDLE_PRIO);
	spin_unlock(&dd->lock);

	seq_printf(m, "%u %u %u\n", rt, be, idle);

	return 0;
}

 
static u32 dd_owned_by_driver(struct deadline_data *dd, enum dd_prio prio)
{
	const struct io_stats_per_prio *stats = &dd->per_prio[prio].stats;

	lockdep_assert_held(&dd->lock);

	return stats->dispatched + stats->merged -
		atomic_read(&stats->completed);
}

static int dd_owned_by_driver_show(void *data, struct seq_file *m)
{
	struct request_queue *q = data;
	struct deadline_data *dd = q->elevator->elevator_data;
	u32 rt, be, idle;

	spin_lock(&dd->lock);
	rt = dd_owned_by_driver(dd, DD_RT_PRIO);
	be = dd_owned_by_driver(dd, DD_BE_PRIO);
	idle = dd_owned_by_driver(dd, DD_IDLE_PRIO);
	spin_unlock(&dd->lock);

	seq_printf(m, "%u %u %u\n", rt, be, idle);

	return 0;
}

#define DEADLINE_DISPATCH_ATTR(prio)					\
static void *deadline_dispatch##prio##_start(struct seq_file *m,	\
					     loff_t *pos)		\
	__acquires(&dd->lock)						\
{									\
	struct request_queue *q = m->private;				\
	struct deadline_data *dd = q->elevator->elevator_data;		\
	struct dd_per_prio *per_prio = &dd->per_prio[prio];		\
									\
	spin_lock(&dd->lock);						\
	return seq_list_start(&per_prio->dispatch, *pos);		\
}									\
									\
static void *deadline_dispatch##prio##_next(struct seq_file *m,		\
					    void *v, loff_t *pos)	\
{									\
	struct request_queue *q = m->private;				\
	struct deadline_data *dd = q->elevator->elevator_data;		\
	struct dd_per_prio *per_prio = &dd->per_prio[prio];		\
									\
	return seq_list_next(v, &per_prio->dispatch, pos);		\
}									\
									\
static void deadline_dispatch##prio##_stop(struct seq_file *m, void *v)	\
	__releases(&dd->lock)						\
{									\
	struct request_queue *q = m->private;				\
	struct deadline_data *dd = q->elevator->elevator_data;		\
									\
	spin_unlock(&dd->lock);						\
}									\
									\
static const struct seq_operations deadline_dispatch##prio##_seq_ops = { \
	.start	= deadline_dispatch##prio##_start,			\
	.next	= deadline_dispatch##prio##_next,			\
	.stop	= deadline_dispatch##prio##_stop,			\
	.show	= blk_mq_debugfs_rq_show,				\
}

DEADLINE_DISPATCH_ATTR(0);
DEADLINE_DISPATCH_ATTR(1);
DEADLINE_DISPATCH_ATTR(2);
#undef DEADLINE_DISPATCH_ATTR

#define DEADLINE_QUEUE_DDIR_ATTRS(name)					\
	{#name "_fifo_list", 0400,					\
			.seq_ops = &deadline_##name##_fifo_seq_ops}
#define DEADLINE_NEXT_RQ_ATTR(name)					\
	{#name "_next_rq", 0400, deadline_##name##_next_rq_show}
static const struct blk_mq_debugfs_attr deadline_queue_debugfs_attrs[] = {
	DEADLINE_QUEUE_DDIR_ATTRS(read0),
	DEADLINE_QUEUE_DDIR_ATTRS(write0),
	DEADLINE_QUEUE_DDIR_ATTRS(read1),
	DEADLINE_QUEUE_DDIR_ATTRS(write1),
	DEADLINE_QUEUE_DDIR_ATTRS(read2),
	DEADLINE_QUEUE_DDIR_ATTRS(write2),
	DEADLINE_NEXT_RQ_ATTR(read0),
	DEADLINE_NEXT_RQ_ATTR(write0),
	DEADLINE_NEXT_RQ_ATTR(read1),
	DEADLINE_NEXT_RQ_ATTR(write1),
	DEADLINE_NEXT_RQ_ATTR(read2),
	DEADLINE_NEXT_RQ_ATTR(write2),
	{"batching", 0400, deadline_batching_show},
	{"starved", 0400, deadline_starved_show},
	{"async_depth", 0400, dd_async_depth_show},
	{"dispatch0", 0400, .seq_ops = &deadline_dispatch0_seq_ops},
	{"dispatch1", 0400, .seq_ops = &deadline_dispatch1_seq_ops},
	{"dispatch2", 0400, .seq_ops = &deadline_dispatch2_seq_ops},
	{"owned_by_driver", 0400, dd_owned_by_driver_show},
	{"queued", 0400, dd_queued_show},
	{},
};
#undef DEADLINE_QUEUE_DDIR_ATTRS
#endif

static struct elevator_type mq_deadline = {
	.ops = {
		.depth_updated		= dd_depth_updated,
		.limit_depth		= dd_limit_depth,
		.insert_requests	= dd_insert_requests,
		.dispatch_request	= dd_dispatch_request,
		.prepare_request	= dd_prepare_request,
		.finish_request		= dd_finish_request,
		.next_request		= elv_rb_latter_request,
		.former_request		= elv_rb_former_request,
		.bio_merge		= dd_bio_merge,
		.request_merge		= dd_request_merge,
		.requests_merged	= dd_merged_requests,
		.request_merged		= dd_request_merged,
		.has_work		= dd_has_work,
		.init_sched		= dd_init_sched,
		.exit_sched		= dd_exit_sched,
		.init_hctx		= dd_init_hctx,
	},

#ifdef CONFIG_BLK_DEBUG_FS
	.queue_debugfs_attrs = deadline_queue_debugfs_attrs,
#endif
	.elevator_attrs = deadline_attrs,
	.elevator_name = "mq-deadline",
	.elevator_alias = "deadline",
	.elevator_features = ELEVATOR_F_ZBD_SEQ_WRITE,
	.elevator_owner = THIS_MODULE,
};
MODULE_ALIAS("mq-deadline-iosched");

static int __init deadline_init(void)
{
	return elv_register(&mq_deadline);
}

static void __exit deadline_exit(void)
{
	elv_unregister(&mq_deadline);
}

module_init(deadline_init);
module_exit(deadline_exit);

MODULE_AUTHOR("Jens Axboe, Damien Le Moal and Bart Van Assche");
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("MQ deadline IO scheduler");
