
 

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/bio.h>
#include <linux/blkdev.h>
#include <linux/gfp.h>
#include <linux/part_stat.h>

#include "blk.h"
#include "blk-mq.h"
#include "blk-mq-sched.h"

 
enum {
	REQ_FSEQ_PREFLUSH	= (1 << 0),  
	REQ_FSEQ_DATA		= (1 << 1),  
	REQ_FSEQ_POSTFLUSH	= (1 << 2),  
	REQ_FSEQ_DONE		= (1 << 3),

	REQ_FSEQ_ACTIONS	= REQ_FSEQ_PREFLUSH | REQ_FSEQ_DATA |
				  REQ_FSEQ_POSTFLUSH,

	 
	FLUSH_PENDING_TIMEOUT	= 5 * HZ,
};

static void blk_kick_flush(struct request_queue *q,
			   struct blk_flush_queue *fq, blk_opf_t flags);

static inline struct blk_flush_queue *
blk_get_flush_queue(struct request_queue *q, struct blk_mq_ctx *ctx)
{
	return blk_mq_map_queue(q, REQ_OP_FLUSH, ctx)->fq;
}

static unsigned int blk_flush_policy(unsigned long fflags, struct request *rq)
{
	unsigned int policy = 0;

	if (blk_rq_sectors(rq))
		policy |= REQ_FSEQ_DATA;

	if (fflags & (1UL << QUEUE_FLAG_WC)) {
		if (rq->cmd_flags & REQ_PREFLUSH)
			policy |= REQ_FSEQ_PREFLUSH;
		if (!(fflags & (1UL << QUEUE_FLAG_FUA)) &&
		    (rq->cmd_flags & REQ_FUA))
			policy |= REQ_FSEQ_POSTFLUSH;
	}
	return policy;
}

static unsigned int blk_flush_cur_seq(struct request *rq)
{
	return 1 << ffz(rq->flush.seq);
}

static void blk_flush_restore_request(struct request *rq)
{
	 
	rq->bio = rq->biotail;

	 
	rq->rq_flags &= ~RQF_FLUSH_SEQ;
	rq->end_io = rq->flush.saved_end_io;
}

static void blk_account_io_flush(struct request *rq)
{
	struct block_device *part = rq->q->disk->part0;

	part_stat_lock();
	part_stat_inc(part, ios[STAT_FLUSH]);
	part_stat_add(part, nsecs[STAT_FLUSH],
		      ktime_get_ns() - rq->start_time_ns);
	part_stat_unlock();
}

 
static void blk_flush_complete_seq(struct request *rq,
				   struct blk_flush_queue *fq,
				   unsigned int seq, blk_status_t error)
{
	struct request_queue *q = rq->q;
	struct list_head *pending = &fq->flush_queue[fq->flush_pending_idx];
	blk_opf_t cmd_flags;

	BUG_ON(rq->flush.seq & seq);
	rq->flush.seq |= seq;
	cmd_flags = rq->cmd_flags;

	if (likely(!error))
		seq = blk_flush_cur_seq(rq);
	else
		seq = REQ_FSEQ_DONE;

	switch (seq) {
	case REQ_FSEQ_PREFLUSH:
	case REQ_FSEQ_POSTFLUSH:
		 
		if (list_empty(pending))
			fq->flush_pending_since = jiffies;
		list_move_tail(&rq->queuelist, pending);
		break;

	case REQ_FSEQ_DATA:
		fq->flush_data_in_flight++;
		spin_lock(&q->requeue_lock);
		list_move(&rq->queuelist, &q->requeue_list);
		spin_unlock(&q->requeue_lock);
		blk_mq_kick_requeue_list(q);
		break;

	case REQ_FSEQ_DONE:
		 
		list_del_init(&rq->queuelist);
		blk_flush_restore_request(rq);
		blk_mq_end_request(rq, error);
		break;

	default:
		BUG();
	}

	blk_kick_flush(q, fq, cmd_flags);
}

static enum rq_end_io_ret flush_end_io(struct request *flush_rq,
				       blk_status_t error)
{
	struct request_queue *q = flush_rq->q;
	struct list_head *running;
	struct request *rq, *n;
	unsigned long flags = 0;
	struct blk_flush_queue *fq = blk_get_flush_queue(q, flush_rq->mq_ctx);

	 
	spin_lock_irqsave(&fq->mq_flush_lock, flags);

	if (!req_ref_put_and_test(flush_rq)) {
		fq->rq_status = error;
		spin_unlock_irqrestore(&fq->mq_flush_lock, flags);
		return RQ_END_IO_NONE;
	}

	blk_account_io_flush(flush_rq);
	 
	WRITE_ONCE(flush_rq->state, MQ_RQ_IDLE);
	if (fq->rq_status != BLK_STS_OK) {
		error = fq->rq_status;
		fq->rq_status = BLK_STS_OK;
	}

	if (!q->elevator) {
		flush_rq->tag = BLK_MQ_NO_TAG;
	} else {
		blk_mq_put_driver_tag(flush_rq);
		flush_rq->internal_tag = BLK_MQ_NO_TAG;
	}

	running = &fq->flush_queue[fq->flush_running_idx];
	BUG_ON(fq->flush_pending_idx == fq->flush_running_idx);

	 
	fq->flush_running_idx ^= 1;

	 
	list_for_each_entry_safe(rq, n, running, queuelist) {
		unsigned int seq = blk_flush_cur_seq(rq);

		BUG_ON(seq != REQ_FSEQ_PREFLUSH && seq != REQ_FSEQ_POSTFLUSH);
		blk_flush_complete_seq(rq, fq, seq, error);
	}

	spin_unlock_irqrestore(&fq->mq_flush_lock, flags);
	return RQ_END_IO_NONE;
}

bool is_flush_rq(struct request *rq)
{
	return rq->end_io == flush_end_io;
}

 
static void blk_kick_flush(struct request_queue *q, struct blk_flush_queue *fq,
			   blk_opf_t flags)
{
	struct list_head *pending = &fq->flush_queue[fq->flush_pending_idx];
	struct request *first_rq =
		list_first_entry(pending, struct request, queuelist);
	struct request *flush_rq = fq->flush_rq;

	 
	if (fq->flush_pending_idx != fq->flush_running_idx || list_empty(pending))
		return;

	 
	if (fq->flush_data_in_flight &&
	    time_before(jiffies,
			fq->flush_pending_since + FLUSH_PENDING_TIMEOUT))
		return;

	 
	fq->flush_pending_idx ^= 1;

	blk_rq_init(q, flush_rq);

	 
	flush_rq->mq_ctx = first_rq->mq_ctx;
	flush_rq->mq_hctx = first_rq->mq_hctx;

	if (!q->elevator) {
		flush_rq->tag = first_rq->tag;

		 
		flush_rq->rq_flags |= RQF_MQ_INFLIGHT;
	} else
		flush_rq->internal_tag = first_rq->internal_tag;

	flush_rq->cmd_flags = REQ_OP_FLUSH | REQ_PREFLUSH;
	flush_rq->cmd_flags |= (flags & REQ_DRV) | (flags & REQ_FAILFAST_MASK);
	flush_rq->rq_flags |= RQF_FLUSH_SEQ;
	flush_rq->end_io = flush_end_io;
	 
	smp_wmb();
	req_ref_set(flush_rq, 1);

	spin_lock(&q->requeue_lock);
	list_add_tail(&flush_rq->queuelist, &q->flush_list);
	spin_unlock(&q->requeue_lock);

	blk_mq_kick_requeue_list(q);
}

static enum rq_end_io_ret mq_flush_data_end_io(struct request *rq,
					       blk_status_t error)
{
	struct request_queue *q = rq->q;
	struct blk_mq_hw_ctx *hctx = rq->mq_hctx;
	struct blk_mq_ctx *ctx = rq->mq_ctx;
	unsigned long flags;
	struct blk_flush_queue *fq = blk_get_flush_queue(q, ctx);

	if (q->elevator) {
		WARN_ON(rq->tag < 0);
		blk_mq_put_driver_tag(rq);
	}

	 
	spin_lock_irqsave(&fq->mq_flush_lock, flags);
	fq->flush_data_in_flight--;
	 
	INIT_LIST_HEAD(&rq->queuelist);
	blk_flush_complete_seq(rq, fq, REQ_FSEQ_DATA, error);
	spin_unlock_irqrestore(&fq->mq_flush_lock, flags);

	blk_mq_sched_restart(hctx);
	return RQ_END_IO_NONE;
}

static void blk_rq_init_flush(struct request *rq)
{
	rq->flush.seq = 0;
	rq->rq_flags |= RQF_FLUSH_SEQ;
	rq->flush.saved_end_io = rq->end_io;  
	rq->end_io = mq_flush_data_end_io;
}

 
bool blk_insert_flush(struct request *rq)
{
	struct request_queue *q = rq->q;
	unsigned long fflags = q->queue_flags;	 
	unsigned int policy = blk_flush_policy(fflags, rq);
	struct blk_flush_queue *fq = blk_get_flush_queue(q, rq->mq_ctx);

	 
	WARN_ON_ONCE(rq->bio != rq->biotail);

	 
	rq->cmd_flags &= ~REQ_PREFLUSH;
	if (!(fflags & (1UL << QUEUE_FLAG_FUA)))
		rq->cmd_flags &= ~REQ_FUA;

	 
	rq->cmd_flags |= REQ_SYNC;

	switch (policy) {
	case 0:
		 
		blk_mq_end_request(rq, 0);
		return true;
	case REQ_FSEQ_DATA:
		 
		return false;
	case REQ_FSEQ_DATA | REQ_FSEQ_POSTFLUSH:
		 
		blk_rq_init_flush(rq);
		rq->flush.seq |= REQ_FSEQ_PREFLUSH;
		spin_lock_irq(&fq->mq_flush_lock);
		fq->flush_data_in_flight++;
		spin_unlock_irq(&fq->mq_flush_lock);
		return false;
	default:
		 
		blk_rq_init_flush(rq);
		spin_lock_irq(&fq->mq_flush_lock);
		blk_flush_complete_seq(rq, fq, REQ_FSEQ_ACTIONS & ~policy, 0);
		spin_unlock_irq(&fq->mq_flush_lock);
		return true;
	}
}

 
int blkdev_issue_flush(struct block_device *bdev)
{
	struct bio bio;

	bio_init(&bio, bdev, NULL, 0, REQ_OP_WRITE | REQ_PREFLUSH);
	return submit_bio_wait(&bio);
}
EXPORT_SYMBOL(blkdev_issue_flush);

struct blk_flush_queue *blk_alloc_flush_queue(int node, int cmd_size,
					      gfp_t flags)
{
	struct blk_flush_queue *fq;
	int rq_sz = sizeof(struct request);

	fq = kzalloc_node(sizeof(*fq), flags, node);
	if (!fq)
		goto fail;

	spin_lock_init(&fq->mq_flush_lock);

	rq_sz = round_up(rq_sz + cmd_size, cache_line_size());
	fq->flush_rq = kzalloc_node(rq_sz, flags, node);
	if (!fq->flush_rq)
		goto fail_rq;

	INIT_LIST_HEAD(&fq->flush_queue[0]);
	INIT_LIST_HEAD(&fq->flush_queue[1]);

	return fq;

 fail_rq:
	kfree(fq);
 fail:
	return NULL;
}

void blk_free_flush_queue(struct blk_flush_queue *fq)
{
	 
	if (!fq)
		return;

	kfree(fq->flush_rq);
	kfree(fq);
}

 
void blk_mq_hctx_set_fq_lock_class(struct blk_mq_hw_ctx *hctx,
		struct lock_class_key *key)
{
	lockdep_set_class(&hctx->fq->mq_flush_lock, key);
}
EXPORT_SYMBOL_GPL(blk_mq_hctx_set_fq_lock_class);
