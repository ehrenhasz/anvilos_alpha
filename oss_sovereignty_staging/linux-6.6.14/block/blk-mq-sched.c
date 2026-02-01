
 
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/list_sort.h>

#include <trace/events/block.h>

#include "blk.h"
#include "blk-mq.h"
#include "blk-mq-debugfs.h"
#include "blk-mq-sched.h"
#include "blk-wbt.h"

 
void blk_mq_sched_mark_restart_hctx(struct blk_mq_hw_ctx *hctx)
{
	if (test_bit(BLK_MQ_S_SCHED_RESTART, &hctx->state))
		return;

	set_bit(BLK_MQ_S_SCHED_RESTART, &hctx->state);
}
EXPORT_SYMBOL_GPL(blk_mq_sched_mark_restart_hctx);

void __blk_mq_sched_restart(struct blk_mq_hw_ctx *hctx)
{
	clear_bit(BLK_MQ_S_SCHED_RESTART, &hctx->state);

	 
	smp_mb();

	blk_mq_run_hw_queue(hctx, true);
}

static int sched_rq_cmp(void *priv, const struct list_head *a,
			const struct list_head *b)
{
	struct request *rqa = container_of(a, struct request, queuelist);
	struct request *rqb = container_of(b, struct request, queuelist);

	return rqa->mq_hctx > rqb->mq_hctx;
}

static bool blk_mq_dispatch_hctx_list(struct list_head *rq_list)
{
	struct blk_mq_hw_ctx *hctx =
		list_first_entry(rq_list, struct request, queuelist)->mq_hctx;
	struct request *rq;
	LIST_HEAD(hctx_list);
	unsigned int count = 0;

	list_for_each_entry(rq, rq_list, queuelist) {
		if (rq->mq_hctx != hctx) {
			list_cut_before(&hctx_list, rq_list, &rq->queuelist);
			goto dispatch;
		}
		count++;
	}
	list_splice_tail_init(rq_list, &hctx_list);

dispatch:
	return blk_mq_dispatch_rq_list(hctx, &hctx_list, count);
}

#define BLK_MQ_BUDGET_DELAY	3		 

 
static int __blk_mq_do_dispatch_sched(struct blk_mq_hw_ctx *hctx)
{
	struct request_queue *q = hctx->queue;
	struct elevator_queue *e = q->elevator;
	bool multi_hctxs = false, run_queue = false;
	bool dispatched = false, busy = false;
	unsigned int max_dispatch;
	LIST_HEAD(rq_list);
	int count = 0;

	if (hctx->dispatch_busy)
		max_dispatch = 1;
	else
		max_dispatch = hctx->queue->nr_requests;

	do {
		struct request *rq;
		int budget_token;

		if (e->type->ops.has_work && !e->type->ops.has_work(hctx))
			break;

		if (!list_empty_careful(&hctx->dispatch)) {
			busy = true;
			break;
		}

		budget_token = blk_mq_get_dispatch_budget(q);
		if (budget_token < 0)
			break;

		rq = e->type->ops.dispatch_request(hctx);
		if (!rq) {
			blk_mq_put_dispatch_budget(q, budget_token);
			 
			run_queue = true;
			break;
		}

		blk_mq_set_rq_budget_token(rq, budget_token);

		 
		list_add_tail(&rq->queuelist, &rq_list);
		count++;
		if (rq->mq_hctx != hctx)
			multi_hctxs = true;

		 
		if (!blk_mq_get_driver_tag(rq))
			break;
	} while (count < max_dispatch);

	if (!count) {
		if (run_queue)
			blk_mq_delay_run_hw_queues(q, BLK_MQ_BUDGET_DELAY);
	} else if (multi_hctxs) {
		 
		list_sort(NULL, &rq_list, sched_rq_cmp);
		do {
			dispatched |= blk_mq_dispatch_hctx_list(&rq_list);
		} while (!list_empty(&rq_list));
	} else {
		dispatched = blk_mq_dispatch_rq_list(hctx, &rq_list, count);
	}

	if (busy)
		return -EAGAIN;
	return !!dispatched;
}

static int blk_mq_do_dispatch_sched(struct blk_mq_hw_ctx *hctx)
{
	unsigned long end = jiffies + HZ;
	int ret;

	do {
		ret = __blk_mq_do_dispatch_sched(hctx);
		if (ret != 1)
			break;
		if (need_resched() || time_is_before_jiffies(end)) {
			blk_mq_delay_run_hw_queue(hctx, 0);
			break;
		}
	} while (1);

	return ret;
}

static struct blk_mq_ctx *blk_mq_next_ctx(struct blk_mq_hw_ctx *hctx,
					  struct blk_mq_ctx *ctx)
{
	unsigned short idx = ctx->index_hw[hctx->type];

	if (++idx == hctx->nr_ctx)
		idx = 0;

	return hctx->ctxs[idx];
}

 
static int blk_mq_do_dispatch_ctx(struct blk_mq_hw_ctx *hctx)
{
	struct request_queue *q = hctx->queue;
	LIST_HEAD(rq_list);
	struct blk_mq_ctx *ctx = READ_ONCE(hctx->dispatch_from);
	int ret = 0;
	struct request *rq;

	do {
		int budget_token;

		if (!list_empty_careful(&hctx->dispatch)) {
			ret = -EAGAIN;
			break;
		}

		if (!sbitmap_any_bit_set(&hctx->ctx_map))
			break;

		budget_token = blk_mq_get_dispatch_budget(q);
		if (budget_token < 0)
			break;

		rq = blk_mq_dequeue_from_ctx(hctx, ctx);
		if (!rq) {
			blk_mq_put_dispatch_budget(q, budget_token);
			 
			blk_mq_delay_run_hw_queues(q, BLK_MQ_BUDGET_DELAY);
			break;
		}

		blk_mq_set_rq_budget_token(rq, budget_token);

		 
		list_add(&rq->queuelist, &rq_list);

		 
		ctx = blk_mq_next_ctx(hctx, rq->mq_ctx);

	} while (blk_mq_dispatch_rq_list(rq->mq_hctx, &rq_list, 1));

	WRITE_ONCE(hctx->dispatch_from, ctx);
	return ret;
}

static int __blk_mq_sched_dispatch_requests(struct blk_mq_hw_ctx *hctx)
{
	bool need_dispatch = false;
	LIST_HEAD(rq_list);

	 
	if (!list_empty_careful(&hctx->dispatch)) {
		spin_lock(&hctx->lock);
		if (!list_empty(&hctx->dispatch))
			list_splice_init(&hctx->dispatch, &rq_list);
		spin_unlock(&hctx->lock);
	}

	 
	if (!list_empty(&rq_list)) {
		blk_mq_sched_mark_restart_hctx(hctx);
		if (!blk_mq_dispatch_rq_list(hctx, &rq_list, 0))
			return 0;
		need_dispatch = true;
	} else {
		need_dispatch = hctx->dispatch_busy;
	}

	if (hctx->queue->elevator)
		return blk_mq_do_dispatch_sched(hctx);

	 
	if (need_dispatch)
		return blk_mq_do_dispatch_ctx(hctx);
	blk_mq_flush_busy_ctxs(hctx, &rq_list);
	blk_mq_dispatch_rq_list(hctx, &rq_list, 0);
	return 0;
}

void blk_mq_sched_dispatch_requests(struct blk_mq_hw_ctx *hctx)
{
	struct request_queue *q = hctx->queue;

	 
	if (unlikely(blk_mq_hctx_stopped(hctx) || blk_queue_quiesced(q)))
		return;

	hctx->run++;

	 
	if (__blk_mq_sched_dispatch_requests(hctx) == -EAGAIN) {
		if (__blk_mq_sched_dispatch_requests(hctx) == -EAGAIN)
			blk_mq_run_hw_queue(hctx, true);
	}
}

bool blk_mq_sched_bio_merge(struct request_queue *q, struct bio *bio,
		unsigned int nr_segs)
{
	struct elevator_queue *e = q->elevator;
	struct blk_mq_ctx *ctx;
	struct blk_mq_hw_ctx *hctx;
	bool ret = false;
	enum hctx_type type;

	if (e && e->type->ops.bio_merge) {
		ret = e->type->ops.bio_merge(q, bio, nr_segs);
		goto out_put;
	}

	ctx = blk_mq_get_ctx(q);
	hctx = blk_mq_map_queue(q, bio->bi_opf, ctx);
	type = hctx->type;
	if (!(hctx->flags & BLK_MQ_F_SHOULD_MERGE) ||
	    list_empty_careful(&ctx->rq_lists[type]))
		goto out_put;

	 
	spin_lock(&ctx->lock);
	 
	if (blk_bio_list_merge(q, &ctx->rq_lists[type], bio, nr_segs))
		ret = true;

	spin_unlock(&ctx->lock);
out_put:
	return ret;
}

bool blk_mq_sched_try_insert_merge(struct request_queue *q, struct request *rq,
				   struct list_head *free)
{
	return rq_mergeable(rq) && elv_attempt_insert_merge(q, rq, free);
}
EXPORT_SYMBOL_GPL(blk_mq_sched_try_insert_merge);

static int blk_mq_sched_alloc_map_and_rqs(struct request_queue *q,
					  struct blk_mq_hw_ctx *hctx,
					  unsigned int hctx_idx)
{
	if (blk_mq_is_shared_tags(q->tag_set->flags)) {
		hctx->sched_tags = q->sched_shared_tags;
		return 0;
	}

	hctx->sched_tags = blk_mq_alloc_map_and_rqs(q->tag_set, hctx_idx,
						    q->nr_requests);

	if (!hctx->sched_tags)
		return -ENOMEM;
	return 0;
}

static void blk_mq_exit_sched_shared_tags(struct request_queue *queue)
{
	blk_mq_free_rq_map(queue->sched_shared_tags);
	queue->sched_shared_tags = NULL;
}

 
static void blk_mq_sched_tags_teardown(struct request_queue *q, unsigned int flags)
{
	struct blk_mq_hw_ctx *hctx;
	unsigned long i;

	queue_for_each_hw_ctx(q, hctx, i) {
		if (hctx->sched_tags) {
			if (!blk_mq_is_shared_tags(flags))
				blk_mq_free_rq_map(hctx->sched_tags);
			hctx->sched_tags = NULL;
		}
	}

	if (blk_mq_is_shared_tags(flags))
		blk_mq_exit_sched_shared_tags(q);
}

static int blk_mq_init_sched_shared_tags(struct request_queue *queue)
{
	struct blk_mq_tag_set *set = queue->tag_set;

	 
	queue->sched_shared_tags = blk_mq_alloc_map_and_rqs(set,
						BLK_MQ_NO_HCTX_IDX,
						MAX_SCHED_RQ);
	if (!queue->sched_shared_tags)
		return -ENOMEM;

	blk_mq_tag_update_sched_shared_tags(queue);

	return 0;
}

 
int blk_mq_init_sched(struct request_queue *q, struct elevator_type *e)
{
	unsigned int flags = q->tag_set->flags;
	struct blk_mq_hw_ctx *hctx;
	struct elevator_queue *eq;
	unsigned long i;
	int ret;

	 
	q->nr_requests = 2 * min_t(unsigned int, q->tag_set->queue_depth,
				   BLKDEV_DEFAULT_RQ);

	if (blk_mq_is_shared_tags(flags)) {
		ret = blk_mq_init_sched_shared_tags(q);
		if (ret)
			return ret;
	}

	queue_for_each_hw_ctx(q, hctx, i) {
		ret = blk_mq_sched_alloc_map_and_rqs(q, hctx, i);
		if (ret)
			goto err_free_map_and_rqs;
	}

	ret = e->ops.init_sched(q, e);
	if (ret)
		goto err_free_map_and_rqs;

	mutex_lock(&q->debugfs_mutex);
	blk_mq_debugfs_register_sched(q);
	mutex_unlock(&q->debugfs_mutex);

	queue_for_each_hw_ctx(q, hctx, i) {
		if (e->ops.init_hctx) {
			ret = e->ops.init_hctx(hctx, i);
			if (ret) {
				eq = q->elevator;
				blk_mq_sched_free_rqs(q);
				blk_mq_exit_sched(q, eq);
				kobject_put(&eq->kobj);
				return ret;
			}
		}
		mutex_lock(&q->debugfs_mutex);
		blk_mq_debugfs_register_sched_hctx(q, hctx);
		mutex_unlock(&q->debugfs_mutex);
	}

	return 0;

err_free_map_and_rqs:
	blk_mq_sched_free_rqs(q);
	blk_mq_sched_tags_teardown(q, flags);

	q->elevator = NULL;
	return ret;
}

 
void blk_mq_sched_free_rqs(struct request_queue *q)
{
	struct blk_mq_hw_ctx *hctx;
	unsigned long i;

	if (blk_mq_is_shared_tags(q->tag_set->flags)) {
		blk_mq_free_rqs(q->tag_set, q->sched_shared_tags,
				BLK_MQ_NO_HCTX_IDX);
	} else {
		queue_for_each_hw_ctx(q, hctx, i) {
			if (hctx->sched_tags)
				blk_mq_free_rqs(q->tag_set,
						hctx->sched_tags, i);
		}
	}
}

void blk_mq_exit_sched(struct request_queue *q, struct elevator_queue *e)
{
	struct blk_mq_hw_ctx *hctx;
	unsigned long i;
	unsigned int flags = 0;

	queue_for_each_hw_ctx(q, hctx, i) {
		mutex_lock(&q->debugfs_mutex);
		blk_mq_debugfs_unregister_sched_hctx(hctx);
		mutex_unlock(&q->debugfs_mutex);

		if (e->type->ops.exit_hctx && hctx->sched_data) {
			e->type->ops.exit_hctx(hctx, i);
			hctx->sched_data = NULL;
		}
		flags = hctx->flags;
	}

	mutex_lock(&q->debugfs_mutex);
	blk_mq_debugfs_unregister_sched(q);
	mutex_unlock(&q->debugfs_mutex);

	if (e->type->ops.exit_sched)
		e->type->ops.exit_sched(e);
	blk_mq_sched_tags_teardown(q, flags);
	q->elevator = NULL;
}
