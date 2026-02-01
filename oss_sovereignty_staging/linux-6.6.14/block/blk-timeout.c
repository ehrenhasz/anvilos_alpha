
 
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/blkdev.h>
#include <linux/fault-inject.h>

#include "blk.h"
#include "blk-mq.h"

#ifdef CONFIG_FAIL_IO_TIMEOUT

static DECLARE_FAULT_ATTR(fail_io_timeout);

static int __init setup_fail_io_timeout(char *str)
{
	return setup_fault_attr(&fail_io_timeout, str);
}
__setup("fail_io_timeout=", setup_fail_io_timeout);

bool __blk_should_fake_timeout(struct request_queue *q)
{
	return should_fail(&fail_io_timeout, 1);
}
EXPORT_SYMBOL_GPL(__blk_should_fake_timeout);

static int __init fail_io_timeout_debugfs(void)
{
	struct dentry *dir = fault_create_debugfs_attr("fail_io_timeout",
						NULL, &fail_io_timeout);

	return PTR_ERR_OR_ZERO(dir);
}

late_initcall(fail_io_timeout_debugfs);

ssize_t part_timeout_show(struct device *dev, struct device_attribute *attr,
			  char *buf)
{
	struct gendisk *disk = dev_to_disk(dev);
	int set = test_bit(QUEUE_FLAG_FAIL_IO, &disk->queue->queue_flags);

	return sprintf(buf, "%d\n", set != 0);
}

ssize_t part_timeout_store(struct device *dev, struct device_attribute *attr,
			   const char *buf, size_t count)
{
	struct gendisk *disk = dev_to_disk(dev);
	int val;

	if (count) {
		struct request_queue *q = disk->queue;
		char *p = (char *) buf;

		val = simple_strtoul(p, &p, 10);
		if (val)
			blk_queue_flag_set(QUEUE_FLAG_FAIL_IO, q);
		else
			blk_queue_flag_clear(QUEUE_FLAG_FAIL_IO, q);
	}

	return count;
}

#endif  

 
void blk_abort_request(struct request *req)
{
	 
	WRITE_ONCE(req->deadline, jiffies);
	kblockd_schedule_work(&req->q->timeout_work);
}
EXPORT_SYMBOL_GPL(blk_abort_request);

static unsigned long blk_timeout_mask __read_mostly;

static int __init blk_timeout_init(void)
{
	blk_timeout_mask = roundup_pow_of_two(HZ) - 1;
	return 0;
}

late_initcall(blk_timeout_init);

 
static inline unsigned long blk_round_jiffies(unsigned long j)
{
	return (j + blk_timeout_mask) + 1;
}

unsigned long blk_rq_timeout(unsigned long timeout)
{
	unsigned long maxt;

	maxt = blk_round_jiffies(jiffies + BLK_MAX_TIMEOUT);
	if (time_after(timeout, maxt))
		timeout = maxt;

	return timeout;
}

 
void blk_add_timer(struct request *req)
{
	struct request_queue *q = req->q;
	unsigned long expiry;

	 
	if (!req->timeout)
		req->timeout = q->rq_timeout;

	req->rq_flags &= ~RQF_TIMED_OUT;

	expiry = jiffies + req->timeout;
	WRITE_ONCE(req->deadline, expiry);

	 
	expiry = blk_rq_timeout(blk_round_jiffies(expiry));

	if (!timer_pending(&q->timeout) ||
	    time_before(expiry, q->timeout.expires)) {
		unsigned long diff = q->timeout.expires - expiry;

		 
		if (!timer_pending(&q->timeout) || (diff >= HZ / 2))
			mod_timer(&q->timeout, expiry);
	}

}
