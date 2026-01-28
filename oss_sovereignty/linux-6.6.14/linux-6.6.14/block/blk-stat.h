#ifndef BLK_STAT_H
#define BLK_STAT_H
#include <linux/kernel.h>
#include <linux/blkdev.h>
#include <linux/ktime.h>
#include <linux/rcupdate.h>
#include <linux/timer.h>
struct blk_stat_callback {
	struct list_head list;
	struct timer_list timer;
	struct blk_rq_stat __percpu *cpu_stat;
	int (*bucket_fn)(const struct request *);
	unsigned int buckets;
	struct blk_rq_stat *stat;
	void (*timer_fn)(struct blk_stat_callback *);
	void *data;
	struct rcu_head rcu;
};
struct blk_queue_stats *blk_alloc_queue_stats(void);
void blk_free_queue_stats(struct blk_queue_stats *);
bool blk_stats_alloc_enable(struct request_queue *q);
void blk_stat_add(struct request *rq, u64 now);
void blk_stat_enable_accounting(struct request_queue *q);
void blk_stat_disable_accounting(struct request_queue *q);
struct blk_stat_callback *
blk_stat_alloc_callback(void (*timer_fn)(struct blk_stat_callback *),
			int (*bucket_fn)(const struct request *),
			unsigned int buckets, void *data);
void blk_stat_add_callback(struct request_queue *q,
			   struct blk_stat_callback *cb);
void blk_stat_remove_callback(struct request_queue *q,
			      struct blk_stat_callback *cb);
void blk_stat_free_callback(struct blk_stat_callback *cb);
static inline bool blk_stat_is_active(struct blk_stat_callback *cb)
{
	return timer_pending(&cb->timer);
}
static inline void blk_stat_activate_nsecs(struct blk_stat_callback *cb,
					   u64 nsecs)
{
	mod_timer(&cb->timer, jiffies + nsecs_to_jiffies(nsecs));
}
static inline void blk_stat_deactivate(struct blk_stat_callback *cb)
{
	del_timer_sync(&cb->timer);
}
static inline void blk_stat_activate_msecs(struct blk_stat_callback *cb,
					   unsigned int msecs)
{
	mod_timer(&cb->timer, jiffies + msecs_to_jiffies(msecs));
}
void blk_rq_stat_add(struct blk_rq_stat *, u64);
void blk_rq_stat_sum(struct blk_rq_stat *, struct blk_rq_stat *);
void blk_rq_stat_init(struct blk_rq_stat *);
#endif
