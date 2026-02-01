
 

 
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/bio.h>
#include <linux/blkdev.h>
#include <linux/blk-pm.h>
#include <linux/blk-integrity.h>
#include <linux/highmem.h>
#include <linux/mm.h>
#include <linux/pagemap.h>
#include <linux/kernel_stat.h>
#include <linux/string.h>
#include <linux/init.h>
#include <linux/completion.h>
#include <linux/slab.h>
#include <linux/swap.h>
#include <linux/writeback.h>
#include <linux/task_io_accounting_ops.h>
#include <linux/fault-inject.h>
#include <linux/list_sort.h>
#include <linux/delay.h>
#include <linux/ratelimit.h>
#include <linux/pm_runtime.h>
#include <linux/t10-pi.h>
#include <linux/debugfs.h>
#include <linux/bpf.h>
#include <linux/part_stat.h>
#include <linux/sched/sysctl.h>
#include <linux/blk-crypto.h>

#define CREATE_TRACE_POINTS
#include <trace/events/block.h>

#include "blk.h"
#include "blk-mq-sched.h"
#include "blk-pm.h"
#include "blk-cgroup.h"
#include "blk-throttle.h"

struct dentry *blk_debugfs_root;

EXPORT_TRACEPOINT_SYMBOL_GPL(block_bio_remap);
EXPORT_TRACEPOINT_SYMBOL_GPL(block_rq_remap);
EXPORT_TRACEPOINT_SYMBOL_GPL(block_bio_complete);
EXPORT_TRACEPOINT_SYMBOL_GPL(block_split);
EXPORT_TRACEPOINT_SYMBOL_GPL(block_unplug);
EXPORT_TRACEPOINT_SYMBOL_GPL(block_rq_insert);

static DEFINE_IDA(blk_queue_ida);

 
static struct kmem_cache *blk_requestq_cachep;

 
static struct workqueue_struct *kblockd_workqueue;

 
void blk_queue_flag_set(unsigned int flag, struct request_queue *q)
{
	set_bit(flag, &q->queue_flags);
}
EXPORT_SYMBOL(blk_queue_flag_set);

 
void blk_queue_flag_clear(unsigned int flag, struct request_queue *q)
{
	clear_bit(flag, &q->queue_flags);
}
EXPORT_SYMBOL(blk_queue_flag_clear);

 
bool blk_queue_flag_test_and_set(unsigned int flag, struct request_queue *q)
{
	return test_and_set_bit(flag, &q->queue_flags);
}
EXPORT_SYMBOL_GPL(blk_queue_flag_test_and_set);

#define REQ_OP_NAME(name) [REQ_OP_##name] = #name
static const char *const blk_op_name[] = {
	REQ_OP_NAME(READ),
	REQ_OP_NAME(WRITE),
	REQ_OP_NAME(FLUSH),
	REQ_OP_NAME(DISCARD),
	REQ_OP_NAME(SECURE_ERASE),
	REQ_OP_NAME(ZONE_RESET),
	REQ_OP_NAME(ZONE_RESET_ALL),
	REQ_OP_NAME(ZONE_OPEN),
	REQ_OP_NAME(ZONE_CLOSE),
	REQ_OP_NAME(ZONE_FINISH),
	REQ_OP_NAME(ZONE_APPEND),
	REQ_OP_NAME(WRITE_ZEROES),
	REQ_OP_NAME(DRV_IN),
	REQ_OP_NAME(DRV_OUT),
};
#undef REQ_OP_NAME

 
inline const char *blk_op_str(enum req_op op)
{
	const char *op_str = "UNKNOWN";

	if (op < ARRAY_SIZE(blk_op_name) && blk_op_name[op])
		op_str = blk_op_name[op];

	return op_str;
}
EXPORT_SYMBOL_GPL(blk_op_str);

static const struct {
	int		errno;
	const char	*name;
} blk_errors[] = {
	[BLK_STS_OK]		= { 0,		"" },
	[BLK_STS_NOTSUPP]	= { -EOPNOTSUPP, "operation not supported" },
	[BLK_STS_TIMEOUT]	= { -ETIMEDOUT,	"timeout" },
	[BLK_STS_NOSPC]		= { -ENOSPC,	"critical space allocation" },
	[BLK_STS_TRANSPORT]	= { -ENOLINK,	"recoverable transport" },
	[BLK_STS_TARGET]	= { -EREMOTEIO,	"critical target" },
	[BLK_STS_RESV_CONFLICT]	= { -EBADE,	"reservation conflict" },
	[BLK_STS_MEDIUM]	= { -ENODATA,	"critical medium" },
	[BLK_STS_PROTECTION]	= { -EILSEQ,	"protection" },
	[BLK_STS_RESOURCE]	= { -ENOMEM,	"kernel resource" },
	[BLK_STS_DEV_RESOURCE]	= { -EBUSY,	"device resource" },
	[BLK_STS_AGAIN]		= { -EAGAIN,	"nonblocking retry" },
	[BLK_STS_OFFLINE]	= { -ENODEV,	"device offline" },

	 
	[BLK_STS_DM_REQUEUE]	= { -EREMCHG, "dm internal retry" },

	 
	[BLK_STS_ZONE_OPEN_RESOURCE]	= { -ETOOMANYREFS, "open zones exceeded" },
	[BLK_STS_ZONE_ACTIVE_RESOURCE]	= { -EOVERFLOW, "active zones exceeded" },

	 
	[BLK_STS_DURATION_LIMIT]	= { -ETIME, "duration limit exceeded" },

	 
	[BLK_STS_IOERR]		= { -EIO,	"I/O" },
};

blk_status_t errno_to_blk_status(int errno)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(blk_errors); i++) {
		if (blk_errors[i].errno == errno)
			return (__force blk_status_t)i;
	}

	return BLK_STS_IOERR;
}
EXPORT_SYMBOL_GPL(errno_to_blk_status);

int blk_status_to_errno(blk_status_t status)
{
	int idx = (__force int)status;

	if (WARN_ON_ONCE(idx >= ARRAY_SIZE(blk_errors)))
		return -EIO;
	return blk_errors[idx].errno;
}
EXPORT_SYMBOL_GPL(blk_status_to_errno);

const char *blk_status_to_str(blk_status_t status)
{
	int idx = (__force int)status;

	if (WARN_ON_ONCE(idx >= ARRAY_SIZE(blk_errors)))
		return "<null>";
	return blk_errors[idx].name;
}
EXPORT_SYMBOL_GPL(blk_status_to_str);

 
void blk_sync_queue(struct request_queue *q)
{
	del_timer_sync(&q->timeout);
	cancel_work_sync(&q->timeout_work);
}
EXPORT_SYMBOL(blk_sync_queue);

 
void blk_set_pm_only(struct request_queue *q)
{
	atomic_inc(&q->pm_only);
}
EXPORT_SYMBOL_GPL(blk_set_pm_only);

void blk_clear_pm_only(struct request_queue *q)
{
	int pm_only;

	pm_only = atomic_dec_return(&q->pm_only);
	WARN_ON_ONCE(pm_only < 0);
	if (pm_only == 0)
		wake_up_all(&q->mq_freeze_wq);
}
EXPORT_SYMBOL_GPL(blk_clear_pm_only);

static void blk_free_queue_rcu(struct rcu_head *rcu_head)
{
	struct request_queue *q = container_of(rcu_head,
			struct request_queue, rcu_head);

	percpu_ref_exit(&q->q_usage_counter);
	kmem_cache_free(blk_requestq_cachep, q);
}

static void blk_free_queue(struct request_queue *q)
{
	blk_free_queue_stats(q->stats);
	if (queue_is_mq(q))
		blk_mq_release(q);

	ida_free(&blk_queue_ida, q->id);
	call_rcu(&q->rcu_head, blk_free_queue_rcu);
}

 
void blk_put_queue(struct request_queue *q)
{
	if (refcount_dec_and_test(&q->refs))
		blk_free_queue(q);
}
EXPORT_SYMBOL(blk_put_queue);

void blk_queue_start_drain(struct request_queue *q)
{
	 
	blk_freeze_queue_start(q);
	if (queue_is_mq(q))
		blk_mq_wake_waiters(q);
	 
	wake_up_all(&q->mq_freeze_wq);
}

 
int blk_queue_enter(struct request_queue *q, blk_mq_req_flags_t flags)
{
	const bool pm = flags & BLK_MQ_REQ_PM;

	while (!blk_try_enter_queue(q, pm)) {
		if (flags & BLK_MQ_REQ_NOWAIT)
			return -EAGAIN;

		 
		smp_rmb();
		wait_event(q->mq_freeze_wq,
			   (!q->mq_freeze_depth &&
			    blk_pm_resume_queue(pm, q)) ||
			   blk_queue_dying(q));
		if (blk_queue_dying(q))
			return -ENODEV;
	}

	return 0;
}

int __bio_queue_enter(struct request_queue *q, struct bio *bio)
{
	while (!blk_try_enter_queue(q, false)) {
		struct gendisk *disk = bio->bi_bdev->bd_disk;

		if (bio->bi_opf & REQ_NOWAIT) {
			if (test_bit(GD_DEAD, &disk->state))
				goto dead;
			bio_wouldblock_error(bio);
			return -EAGAIN;
		}

		 
		smp_rmb();
		wait_event(q->mq_freeze_wq,
			   (!q->mq_freeze_depth &&
			    blk_pm_resume_queue(false, q)) ||
			   test_bit(GD_DEAD, &disk->state));
		if (test_bit(GD_DEAD, &disk->state))
			goto dead;
	}

	return 0;
dead:
	bio_io_error(bio);
	return -ENODEV;
}

void blk_queue_exit(struct request_queue *q)
{
	percpu_ref_put(&q->q_usage_counter);
}

static void blk_queue_usage_counter_release(struct percpu_ref *ref)
{
	struct request_queue *q =
		container_of(ref, struct request_queue, q_usage_counter);

	wake_up_all(&q->mq_freeze_wq);
}

static void blk_rq_timed_out_timer(struct timer_list *t)
{
	struct request_queue *q = from_timer(q, t, timeout);

	kblockd_schedule_work(&q->timeout_work);
}

static void blk_timeout_work(struct work_struct *work)
{
}

struct request_queue *blk_alloc_queue(int node_id)
{
	struct request_queue *q;

	q = kmem_cache_alloc_node(blk_requestq_cachep, GFP_KERNEL | __GFP_ZERO,
				  node_id);
	if (!q)
		return NULL;

	q->last_merge = NULL;

	q->id = ida_alloc(&blk_queue_ida, GFP_KERNEL);
	if (q->id < 0)
		goto fail_q;

	q->stats = blk_alloc_queue_stats();
	if (!q->stats)
		goto fail_id;

	q->node = node_id;

	atomic_set(&q->nr_active_requests_shared_tags, 0);

	timer_setup(&q->timeout, blk_rq_timed_out_timer, 0);
	INIT_WORK(&q->timeout_work, blk_timeout_work);
	INIT_LIST_HEAD(&q->icq_list);

	refcount_set(&q->refs, 1);
	mutex_init(&q->debugfs_mutex);
	mutex_init(&q->sysfs_lock);
	mutex_init(&q->sysfs_dir_lock);
	mutex_init(&q->rq_qos_mutex);
	spin_lock_init(&q->queue_lock);

	init_waitqueue_head(&q->mq_freeze_wq);
	mutex_init(&q->mq_freeze_lock);

	 
	if (percpu_ref_init(&q->q_usage_counter,
				blk_queue_usage_counter_release,
				PERCPU_REF_INIT_ATOMIC, GFP_KERNEL))
		goto fail_stats;

	blk_set_default_limits(&q->limits);
	q->nr_requests = BLKDEV_DEFAULT_RQ;

	return q;

fail_stats:
	blk_free_queue_stats(q->stats);
fail_id:
	ida_free(&blk_queue_ida, q->id);
fail_q:
	kmem_cache_free(blk_requestq_cachep, q);
	return NULL;
}

 
bool blk_get_queue(struct request_queue *q)
{
	if (unlikely(blk_queue_dying(q)))
		return false;
	refcount_inc(&q->refs);
	return true;
}
EXPORT_SYMBOL(blk_get_queue);

#ifdef CONFIG_FAIL_MAKE_REQUEST

static DECLARE_FAULT_ATTR(fail_make_request);

static int __init setup_fail_make_request(char *str)
{
	return setup_fault_attr(&fail_make_request, str);
}
__setup("fail_make_request=", setup_fail_make_request);

bool should_fail_request(struct block_device *part, unsigned int bytes)
{
	return part->bd_make_it_fail && should_fail(&fail_make_request, bytes);
}

static int __init fail_make_request_debugfs(void)
{
	struct dentry *dir = fault_create_debugfs_attr("fail_make_request",
						NULL, &fail_make_request);

	return PTR_ERR_OR_ZERO(dir);
}

late_initcall(fail_make_request_debugfs);
#endif  

static inline void bio_check_ro(struct bio *bio)
{
	if (op_is_write(bio_op(bio)) && bdev_read_only(bio->bi_bdev)) {
		if (op_is_flush(bio->bi_opf) && !bio_sectors(bio))
			return;

		if (bio->bi_bdev->bd_ro_warned)
			return;

		bio->bi_bdev->bd_ro_warned = true;
		 
		pr_warn("Trying to write to read-only block-device %pg\n",
			bio->bi_bdev);
	}
}

static noinline int should_fail_bio(struct bio *bio)
{
	if (should_fail_request(bdev_whole(bio->bi_bdev), bio->bi_iter.bi_size))
		return -EIO;
	return 0;
}
ALLOW_ERROR_INJECTION(should_fail_bio, ERRNO);

 
static inline int bio_check_eod(struct bio *bio)
{
	sector_t maxsector = bdev_nr_sectors(bio->bi_bdev);
	unsigned int nr_sectors = bio_sectors(bio);

	if (nr_sectors &&
	    (nr_sectors > maxsector ||
	     bio->bi_iter.bi_sector > maxsector - nr_sectors)) {
		pr_info_ratelimited("%s: attempt to access beyond end of device\n"
				    "%pg: rw=%d, sector=%llu, nr_sectors = %u limit=%llu\n",
				    current->comm, bio->bi_bdev, bio->bi_opf,
				    bio->bi_iter.bi_sector, nr_sectors, maxsector);
		return -EIO;
	}
	return 0;
}

 
static int blk_partition_remap(struct bio *bio)
{
	struct block_device *p = bio->bi_bdev;

	if (unlikely(should_fail_request(p, bio->bi_iter.bi_size)))
		return -EIO;
	if (bio_sectors(bio)) {
		bio->bi_iter.bi_sector += p->bd_start_sect;
		trace_block_bio_remap(bio, p->bd_dev,
				      bio->bi_iter.bi_sector -
				      p->bd_start_sect);
	}
	bio_set_flag(bio, BIO_REMAPPED);
	return 0;
}

 
static inline blk_status_t blk_check_zone_append(struct request_queue *q,
						 struct bio *bio)
{
	int nr_sectors = bio_sectors(bio);

	 
	if (!bdev_is_zoned(bio->bi_bdev))
		return BLK_STS_NOTSUPP;

	 
	if (!bdev_is_zone_start(bio->bi_bdev, bio->bi_iter.bi_sector) ||
	    !bio_zone_is_seq(bio))
		return BLK_STS_IOERR;

	 
	if (nr_sectors > q->limits.chunk_sectors)
		return BLK_STS_IOERR;

	 
	if (nr_sectors > q->limits.max_zone_append_sectors)
		return BLK_STS_IOERR;

	bio->bi_opf |= REQ_NOMERGE;

	return BLK_STS_OK;
}

static void __submit_bio(struct bio *bio)
{
	if (unlikely(!blk_crypto_bio_prep(&bio)))
		return;

	if (!bio->bi_bdev->bd_has_submit_bio) {
		blk_mq_submit_bio(bio);
	} else if (likely(bio_queue_enter(bio) == 0)) {
		struct gendisk *disk = bio->bi_bdev->bd_disk;

		disk->fops->submit_bio(bio);
		blk_queue_exit(disk->queue);
	}
}

 
static void __submit_bio_noacct(struct bio *bio)
{
	struct bio_list bio_list_on_stack[2];

	BUG_ON(bio->bi_next);

	bio_list_init(&bio_list_on_stack[0]);
	current->bio_list = bio_list_on_stack;

	do {
		struct request_queue *q = bdev_get_queue(bio->bi_bdev);
		struct bio_list lower, same;

		 
		bio_list_on_stack[1] = bio_list_on_stack[0];
		bio_list_init(&bio_list_on_stack[0]);

		__submit_bio(bio);

		 
		bio_list_init(&lower);
		bio_list_init(&same);
		while ((bio = bio_list_pop(&bio_list_on_stack[0])) != NULL)
			if (q == bdev_get_queue(bio->bi_bdev))
				bio_list_add(&same, bio);
			else
				bio_list_add(&lower, bio);

		 
		bio_list_merge(&bio_list_on_stack[0], &lower);
		bio_list_merge(&bio_list_on_stack[0], &same);
		bio_list_merge(&bio_list_on_stack[0], &bio_list_on_stack[1]);
	} while ((bio = bio_list_pop(&bio_list_on_stack[0])));

	current->bio_list = NULL;
}

static void __submit_bio_noacct_mq(struct bio *bio)
{
	struct bio_list bio_list[2] = { };

	current->bio_list = bio_list;

	do {
		__submit_bio(bio);
	} while ((bio = bio_list_pop(&bio_list[0])));

	current->bio_list = NULL;
}

void submit_bio_noacct_nocheck(struct bio *bio)
{
	blk_cgroup_bio_start(bio);
	blkcg_bio_issue_init(bio);

	if (!bio_flagged(bio, BIO_TRACE_COMPLETION)) {
		trace_block_bio_queue(bio);
		 
		bio_set_flag(bio, BIO_TRACE_COMPLETION);
	}

	 
	if (current->bio_list)
		bio_list_add(&current->bio_list[0], bio);
	else if (!bio->bi_bdev->bd_has_submit_bio)
		__submit_bio_noacct_mq(bio);
	else
		__submit_bio_noacct(bio);
}

 
void submit_bio_noacct(struct bio *bio)
{
	struct block_device *bdev = bio->bi_bdev;
	struct request_queue *q = bdev_get_queue(bdev);
	blk_status_t status = BLK_STS_IOERR;

	might_sleep();

	 
	if ((bio->bi_opf & REQ_NOWAIT) && !bdev_nowait(bdev))
		goto not_supported;

	if (should_fail_bio(bio))
		goto end_io;
	bio_check_ro(bio);
	if (!bio_flagged(bio, BIO_REMAPPED)) {
		if (unlikely(bio_check_eod(bio)))
			goto end_io;
		if (bdev->bd_partno && unlikely(blk_partition_remap(bio)))
			goto end_io;
	}

	 
	if (op_is_flush(bio->bi_opf)) {
		if (WARN_ON_ONCE(bio_op(bio) != REQ_OP_WRITE &&
				 bio_op(bio) != REQ_OP_ZONE_APPEND))
			goto end_io;
		if (!test_bit(QUEUE_FLAG_WC, &q->queue_flags)) {
			bio->bi_opf &= ~(REQ_PREFLUSH | REQ_FUA);
			if (!bio_sectors(bio)) {
				status = BLK_STS_OK;
				goto end_io;
			}
		}
	}

	if (!test_bit(QUEUE_FLAG_POLL, &q->queue_flags))
		bio_clear_polled(bio);

	switch (bio_op(bio)) {
	case REQ_OP_DISCARD:
		if (!bdev_max_discard_sectors(bdev))
			goto not_supported;
		break;
	case REQ_OP_SECURE_ERASE:
		if (!bdev_max_secure_erase_sectors(bdev))
			goto not_supported;
		break;
	case REQ_OP_ZONE_APPEND:
		status = blk_check_zone_append(q, bio);
		if (status != BLK_STS_OK)
			goto end_io;
		break;
	case REQ_OP_ZONE_RESET:
	case REQ_OP_ZONE_OPEN:
	case REQ_OP_ZONE_CLOSE:
	case REQ_OP_ZONE_FINISH:
		if (!bdev_is_zoned(bio->bi_bdev))
			goto not_supported;
		break;
	case REQ_OP_ZONE_RESET_ALL:
		if (!bdev_is_zoned(bio->bi_bdev) || !blk_queue_zone_resetall(q))
			goto not_supported;
		break;
	case REQ_OP_WRITE_ZEROES:
		if (!q->limits.max_write_zeroes_sectors)
			goto not_supported;
		break;
	default:
		break;
	}

	if (blk_throtl_bio(bio))
		return;
	submit_bio_noacct_nocheck(bio);
	return;

not_supported:
	status = BLK_STS_NOTSUPP;
end_io:
	bio->bi_status = status;
	bio_endio(bio);
}
EXPORT_SYMBOL(submit_bio_noacct);

 
void submit_bio(struct bio *bio)
{
	if (bio_op(bio) == REQ_OP_READ) {
		task_io_account_read(bio->bi_iter.bi_size);
		count_vm_events(PGPGIN, bio_sectors(bio));
	} else if (bio_op(bio) == REQ_OP_WRITE) {
		count_vm_events(PGPGOUT, bio_sectors(bio));
	}

	submit_bio_noacct(bio);
}
EXPORT_SYMBOL(submit_bio);

 
int bio_poll(struct bio *bio, struct io_comp_batch *iob, unsigned int flags)
{
	blk_qc_t cookie = READ_ONCE(bio->bi_cookie);
	struct block_device *bdev;
	struct request_queue *q;
	int ret = 0;

	bdev = READ_ONCE(bio->bi_bdev);
	if (!bdev)
		return 0;

	q = bdev_get_queue(bdev);
	if (cookie == BLK_QC_T_NONE ||
	    !test_bit(QUEUE_FLAG_POLL, &q->queue_flags))
		return 0;

	 
	blk_flush_plug(current->plug, false);

	 
	if (!percpu_ref_tryget(&q->q_usage_counter))
		return 0;
	if (queue_is_mq(q)) {
		ret = blk_mq_poll(q, cookie, iob, flags);
	} else {
		struct gendisk *disk = q->disk;

		if (disk && disk->fops->poll_bio)
			ret = disk->fops->poll_bio(bio, iob, flags);
	}
	blk_queue_exit(q);
	return ret;
}
EXPORT_SYMBOL_GPL(bio_poll);

 
int iocb_bio_iopoll(struct kiocb *kiocb, struct io_comp_batch *iob,
		    unsigned int flags)
{
	struct bio *bio;
	int ret = 0;

	 
	rcu_read_lock();
	bio = READ_ONCE(kiocb->private);
	if (bio)
		ret = bio_poll(bio, iob, flags);
	rcu_read_unlock();

	return ret;
}
EXPORT_SYMBOL_GPL(iocb_bio_iopoll);

void update_io_ticks(struct block_device *part, unsigned long now, bool end)
{
	unsigned long stamp;
again:
	stamp = READ_ONCE(part->bd_stamp);
	if (unlikely(time_after(now, stamp))) {
		if (likely(try_cmpxchg(&part->bd_stamp, &stamp, now)))
			__part_stat_add(part, io_ticks, end ? now - stamp : 1);
	}
	if (part->bd_partno) {
		part = bdev_whole(part);
		goto again;
	}
}

unsigned long bdev_start_io_acct(struct block_device *bdev, enum req_op op,
				 unsigned long start_time)
{
	part_stat_lock();
	update_io_ticks(bdev, start_time, false);
	part_stat_local_inc(bdev, in_flight[op_is_write(op)]);
	part_stat_unlock();

	return start_time;
}
EXPORT_SYMBOL(bdev_start_io_acct);

 
unsigned long bio_start_io_acct(struct bio *bio)
{
	return bdev_start_io_acct(bio->bi_bdev, bio_op(bio), jiffies);
}
EXPORT_SYMBOL_GPL(bio_start_io_acct);

void bdev_end_io_acct(struct block_device *bdev, enum req_op op,
		      unsigned int sectors, unsigned long start_time)
{
	const int sgrp = op_stat_group(op);
	unsigned long now = READ_ONCE(jiffies);
	unsigned long duration = now - start_time;

	part_stat_lock();
	update_io_ticks(bdev, now, true);
	part_stat_inc(bdev, ios[sgrp]);
	part_stat_add(bdev, sectors[sgrp], sectors);
	part_stat_add(bdev, nsecs[sgrp], jiffies_to_nsecs(duration));
	part_stat_local_dec(bdev, in_flight[op_is_write(op)]);
	part_stat_unlock();
}
EXPORT_SYMBOL(bdev_end_io_acct);

void bio_end_io_acct_remapped(struct bio *bio, unsigned long start_time,
			      struct block_device *orig_bdev)
{
	bdev_end_io_acct(orig_bdev, bio_op(bio), bio_sectors(bio), start_time);
}
EXPORT_SYMBOL_GPL(bio_end_io_acct_remapped);

 
int blk_lld_busy(struct request_queue *q)
{
	if (queue_is_mq(q) && q->mq_ops->busy)
		return q->mq_ops->busy(q);

	return 0;
}
EXPORT_SYMBOL_GPL(blk_lld_busy);

int kblockd_schedule_work(struct work_struct *work)
{
	return queue_work(kblockd_workqueue, work);
}
EXPORT_SYMBOL(kblockd_schedule_work);

int kblockd_mod_delayed_work_on(int cpu, struct delayed_work *dwork,
				unsigned long delay)
{
	return mod_delayed_work_on(cpu, kblockd_workqueue, dwork, delay);
}
EXPORT_SYMBOL(kblockd_mod_delayed_work_on);

void blk_start_plug_nr_ios(struct blk_plug *plug, unsigned short nr_ios)
{
	struct task_struct *tsk = current;

	 
	if (tsk->plug)
		return;

	plug->mq_list = NULL;
	plug->cached_rq = NULL;
	plug->nr_ios = min_t(unsigned short, nr_ios, BLK_MAX_REQUEST_COUNT);
	plug->rq_count = 0;
	plug->multiple_queues = false;
	plug->has_elevator = false;
	INIT_LIST_HEAD(&plug->cb_list);

	 
	tsk->plug = plug;
}

 
void blk_start_plug(struct blk_plug *plug)
{
	blk_start_plug_nr_ios(plug, 1);
}
EXPORT_SYMBOL(blk_start_plug);

static void flush_plug_callbacks(struct blk_plug *plug, bool from_schedule)
{
	LIST_HEAD(callbacks);

	while (!list_empty(&plug->cb_list)) {
		list_splice_init(&plug->cb_list, &callbacks);

		while (!list_empty(&callbacks)) {
			struct blk_plug_cb *cb = list_first_entry(&callbacks,
							  struct blk_plug_cb,
							  list);
			list_del(&cb->list);
			cb->callback(cb, from_schedule);
		}
	}
}

struct blk_plug_cb *blk_check_plugged(blk_plug_cb_fn unplug, void *data,
				      int size)
{
	struct blk_plug *plug = current->plug;
	struct blk_plug_cb *cb;

	if (!plug)
		return NULL;

	list_for_each_entry(cb, &plug->cb_list, list)
		if (cb->callback == unplug && cb->data == data)
			return cb;

	 
	BUG_ON(size < sizeof(*cb));
	cb = kzalloc(size, GFP_ATOMIC);
	if (cb) {
		cb->data = data;
		cb->callback = unplug;
		list_add(&cb->list, &plug->cb_list);
	}
	return cb;
}
EXPORT_SYMBOL(blk_check_plugged);

void __blk_flush_plug(struct blk_plug *plug, bool from_schedule)
{
	if (!list_empty(&plug->cb_list))
		flush_plug_callbacks(plug, from_schedule);
	blk_mq_flush_plug_list(plug, from_schedule);
	 
	if (unlikely(!rq_list_empty(plug->cached_rq)))
		blk_mq_free_plug_rqs(plug);
}

 
void blk_finish_plug(struct blk_plug *plug)
{
	if (plug == current->plug) {
		__blk_flush_plug(plug, false);
		current->plug = NULL;
	}
}
EXPORT_SYMBOL(blk_finish_plug);

void blk_io_schedule(void)
{
	 
	unsigned long timeout = sysctl_hung_task_timeout_secs * HZ / 2;

	if (timeout)
		io_schedule_timeout(timeout);
	else
		io_schedule();
}
EXPORT_SYMBOL_GPL(blk_io_schedule);

int __init blk_dev_init(void)
{
	BUILD_BUG_ON((__force u32)REQ_OP_LAST >= (1 << REQ_OP_BITS));
	BUILD_BUG_ON(REQ_OP_BITS + REQ_FLAG_BITS > 8 *
			sizeof_field(struct request, cmd_flags));
	BUILD_BUG_ON(REQ_OP_BITS + REQ_FLAG_BITS > 8 *
			sizeof_field(struct bio, bi_opf));

	 
	kblockd_workqueue = alloc_workqueue("kblockd",
					    WQ_MEM_RECLAIM | WQ_HIGHPRI, 0);
	if (!kblockd_workqueue)
		panic("Failed to create kblockd\n");

	blk_requestq_cachep = kmem_cache_create("request_queue",
			sizeof(struct request_queue), 0, SLAB_PANIC, NULL);

	blk_debugfs_root = debugfs_create_dir("block", NULL);

	return 0;
}
