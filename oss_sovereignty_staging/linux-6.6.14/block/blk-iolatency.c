
 
#include <linux/kernel.h>
#include <linux/blk_types.h>
#include <linux/backing-dev.h>
#include <linux/module.h>
#include <linux/timer.h>
#include <linux/memcontrol.h>
#include <linux/sched/loadavg.h>
#include <linux/sched/signal.h>
#include <trace/events/block.h>
#include <linux/blk-mq.h>
#include "blk-rq-qos.h"
#include "blk-stat.h"
#include "blk-cgroup.h"
#include "blk.h"

#define DEFAULT_SCALE_COOKIE 1000000U

static struct blkcg_policy blkcg_policy_iolatency;
struct iolatency_grp;

struct blk_iolatency {
	struct rq_qos rqos;
	struct timer_list timer;

	 
	bool enabled;
	atomic_t enable_cnt;
	struct work_struct enable_work;
};

static inline struct blk_iolatency *BLKIOLATENCY(struct rq_qos *rqos)
{
	return container_of(rqos, struct blk_iolatency, rqos);
}

struct child_latency_info {
	spinlock_t lock;

	 
	u64 last_scale_event;

	 
	u64 scale_lat;

	 
	u64 nr_samples;

	 
	struct iolatency_grp *scale_grp;

	 
	atomic_t scale_cookie;
};

struct percentile_stats {
	u64 total;
	u64 missed;
};

struct latency_stat {
	union {
		struct percentile_stats ps;
		struct blk_rq_stat rqs;
	};
};

struct iolatency_grp {
	struct blkg_policy_data pd;
	struct latency_stat __percpu *stats;
	struct latency_stat cur_stat;
	struct blk_iolatency *blkiolat;
	unsigned int max_depth;
	struct rq_wait rq_wait;
	atomic64_t window_start;
	atomic_t scale_cookie;
	u64 min_lat_nsec;
	u64 cur_win_nsec;

	 
	u64 lat_avg;

	 
	u64 nr_samples;

	bool ssd;
	struct child_latency_info child_lat;
};

#define BLKIOLATENCY_MIN_WIN_SIZE (100 * NSEC_PER_MSEC)
#define BLKIOLATENCY_MAX_WIN_SIZE NSEC_PER_SEC
 
#define BLKIOLATENCY_NR_EXP_FACTORS 5
#define BLKIOLATENCY_EXP_BUCKET_SIZE (BLKIOLATENCY_MAX_WIN_SIZE / \
				      (BLKIOLATENCY_NR_EXP_FACTORS - 1))
static const u64 iolatency_exp_factors[BLKIOLATENCY_NR_EXP_FACTORS] = {
	2045, 
	2039, 
	2031, 
	2023, 
	2014, 
};

static inline struct iolatency_grp *pd_to_lat(struct blkg_policy_data *pd)
{
	return pd ? container_of(pd, struct iolatency_grp, pd) : NULL;
}

static inline struct iolatency_grp *blkg_to_lat(struct blkcg_gq *blkg)
{
	return pd_to_lat(blkg_to_pd(blkg, &blkcg_policy_iolatency));
}

static inline struct blkcg_gq *lat_to_blkg(struct iolatency_grp *iolat)
{
	return pd_to_blkg(&iolat->pd);
}

static inline void latency_stat_init(struct iolatency_grp *iolat,
				     struct latency_stat *stat)
{
	if (iolat->ssd) {
		stat->ps.total = 0;
		stat->ps.missed = 0;
	} else
		blk_rq_stat_init(&stat->rqs);
}

static inline void latency_stat_sum(struct iolatency_grp *iolat,
				    struct latency_stat *sum,
				    struct latency_stat *stat)
{
	if (iolat->ssd) {
		sum->ps.total += stat->ps.total;
		sum->ps.missed += stat->ps.missed;
	} else
		blk_rq_stat_sum(&sum->rqs, &stat->rqs);
}

static inline void latency_stat_record_time(struct iolatency_grp *iolat,
					    u64 req_time)
{
	struct latency_stat *stat = get_cpu_ptr(iolat->stats);
	if (iolat->ssd) {
		if (req_time >= iolat->min_lat_nsec)
			stat->ps.missed++;
		stat->ps.total++;
	} else
		blk_rq_stat_add(&stat->rqs, req_time);
	put_cpu_ptr(stat);
}

static inline bool latency_sum_ok(struct iolatency_grp *iolat,
				  struct latency_stat *stat)
{
	if (iolat->ssd) {
		u64 thresh = div64_u64(stat->ps.total, 10);
		thresh = max(thresh, 1ULL);
		return stat->ps.missed < thresh;
	}
	return stat->rqs.mean <= iolat->min_lat_nsec;
}

static inline u64 latency_stat_samples(struct iolatency_grp *iolat,
				       struct latency_stat *stat)
{
	if (iolat->ssd)
		return stat->ps.total;
	return stat->rqs.nr_samples;
}

static inline void iolat_update_total_lat_avg(struct iolatency_grp *iolat,
					      struct latency_stat *stat)
{
	int exp_idx;

	if (iolat->ssd)
		return;

	 
	exp_idx = min_t(int, BLKIOLATENCY_NR_EXP_FACTORS - 1,
			div64_u64(iolat->cur_win_nsec,
				  BLKIOLATENCY_EXP_BUCKET_SIZE));
	iolat->lat_avg = calc_load(iolat->lat_avg,
				   iolatency_exp_factors[exp_idx],
				   stat->rqs.mean);
}

static void iolat_cleanup_cb(struct rq_wait *rqw, void *private_data)
{
	atomic_dec(&rqw->inflight);
	wake_up(&rqw->wait);
}

static bool iolat_acquire_inflight(struct rq_wait *rqw, void *private_data)
{
	struct iolatency_grp *iolat = private_data;
	return rq_wait_inc_below(rqw, iolat->max_depth);
}

static void __blkcg_iolatency_throttle(struct rq_qos *rqos,
				       struct iolatency_grp *iolat,
				       bool issue_as_root,
				       bool use_memdelay)
{
	struct rq_wait *rqw = &iolat->rq_wait;
	unsigned use_delay = atomic_read(&lat_to_blkg(iolat)->use_delay);

	if (use_delay)
		blkcg_schedule_throttle(rqos->disk, use_memdelay);

	 
	if (issue_as_root || fatal_signal_pending(current)) {
		atomic_inc(&rqw->inflight);
		return;
	}

	rq_qos_wait(rqw, iolat, iolat_acquire_inflight, iolat_cleanup_cb);
}

#define SCALE_DOWN_FACTOR 2
#define SCALE_UP_FACTOR 4

static inline unsigned long scale_amount(unsigned long qd, bool up)
{
	return max(up ? qd >> SCALE_UP_FACTOR : qd >> SCALE_DOWN_FACTOR, 1UL);
}

 
static void scale_cookie_change(struct blk_iolatency *blkiolat,
				struct child_latency_info *lat_info,
				bool up)
{
	unsigned long qd = blkiolat->rqos.disk->queue->nr_requests;
	unsigned long scale = scale_amount(qd, up);
	unsigned long old = atomic_read(&lat_info->scale_cookie);
	unsigned long max_scale = qd << 1;
	unsigned long diff = 0;

	if (old < DEFAULT_SCALE_COOKIE)
		diff = DEFAULT_SCALE_COOKIE - old;

	if (up) {
		if (scale + old > DEFAULT_SCALE_COOKIE)
			atomic_set(&lat_info->scale_cookie,
				   DEFAULT_SCALE_COOKIE);
		else if (diff > qd)
			atomic_inc(&lat_info->scale_cookie);
		else
			atomic_add(scale, &lat_info->scale_cookie);
	} else {
		 
		if (diff > qd) {
			if (diff < max_scale)
				atomic_dec(&lat_info->scale_cookie);
		} else {
			atomic_sub(scale, &lat_info->scale_cookie);
		}
	}
}

 
static void scale_change(struct iolatency_grp *iolat, bool up)
{
	unsigned long qd = iolat->blkiolat->rqos.disk->queue->nr_requests;
	unsigned long scale = scale_amount(qd, up);
	unsigned long old = iolat->max_depth;

	if (old > qd)
		old = qd;

	if (up) {
		if (old == 1 && blkcg_unuse_delay(lat_to_blkg(iolat)))
			return;

		if (old < qd) {
			old += scale;
			old = min(old, qd);
			iolat->max_depth = old;
			wake_up_all(&iolat->rq_wait.wait);
		}
	} else {
		old >>= 1;
		iolat->max_depth = max(old, 1UL);
	}
}

 
static void check_scale_change(struct iolatency_grp *iolat)
{
	struct iolatency_grp *parent;
	struct child_latency_info *lat_info;
	unsigned int cur_cookie;
	unsigned int our_cookie = atomic_read(&iolat->scale_cookie);
	u64 scale_lat;
	int direction = 0;

	parent = blkg_to_lat(lat_to_blkg(iolat)->parent);
	if (!parent)
		return;

	lat_info = &parent->child_lat;
	cur_cookie = atomic_read(&lat_info->scale_cookie);
	scale_lat = READ_ONCE(lat_info->scale_lat);

	if (cur_cookie < our_cookie)
		direction = -1;
	else if (cur_cookie > our_cookie)
		direction = 1;
	else
		return;

	if (!atomic_try_cmpxchg(&iolat->scale_cookie, &our_cookie, cur_cookie)) {
		 
		return;
	}

	if (direction < 0 && iolat->min_lat_nsec) {
		u64 samples_thresh;

		if (!scale_lat || iolat->min_lat_nsec <= scale_lat)
			return;

		 
		samples_thresh = lat_info->nr_samples * 5;
		samples_thresh = max(1ULL, div64_u64(samples_thresh, 100));
		if (iolat->nr_samples <= samples_thresh)
			return;
	}

	 
	if (iolat->max_depth == 1 && direction < 0) {
		blkcg_use_delay(lat_to_blkg(iolat));
		return;
	}

	 
	if (cur_cookie == DEFAULT_SCALE_COOKIE) {
		blkcg_clear_delay(lat_to_blkg(iolat));
		iolat->max_depth = UINT_MAX;
		wake_up_all(&iolat->rq_wait.wait);
		return;
	}

	scale_change(iolat, direction > 0);
}

static void blkcg_iolatency_throttle(struct rq_qos *rqos, struct bio *bio)
{
	struct blk_iolatency *blkiolat = BLKIOLATENCY(rqos);
	struct blkcg_gq *blkg = bio->bi_blkg;
	bool issue_as_root = bio_issue_as_root_blkg(bio);

	if (!blkiolat->enabled)
		return;

	while (blkg && blkg->parent) {
		struct iolatency_grp *iolat = blkg_to_lat(blkg);
		if (!iolat) {
			blkg = blkg->parent;
			continue;
		}

		check_scale_change(iolat);
		__blkcg_iolatency_throttle(rqos, iolat, issue_as_root,
				     (bio->bi_opf & REQ_SWAP) == REQ_SWAP);
		blkg = blkg->parent;
	}
	if (!timer_pending(&blkiolat->timer))
		mod_timer(&blkiolat->timer, jiffies + HZ);
}

static void iolatency_record_time(struct iolatency_grp *iolat,
				  struct bio_issue *issue, u64 now,
				  bool issue_as_root)
{
	u64 start = bio_issue_time(issue);
	u64 req_time;

	 
	now = __bio_issue_time(now);

	if (now <= start)
		return;

	req_time = now - start;

	 
	if (unlikely(issue_as_root && iolat->max_depth != UINT_MAX)) {
		u64 sub = iolat->min_lat_nsec;
		if (req_time < sub)
			blkcg_add_delay(lat_to_blkg(iolat), now, sub - req_time);
		return;
	}

	latency_stat_record_time(iolat, req_time);
}

#define BLKIOLATENCY_MIN_ADJUST_TIME (500 * NSEC_PER_MSEC)
#define BLKIOLATENCY_MIN_GOOD_SAMPLES 5

static void iolatency_check_latencies(struct iolatency_grp *iolat, u64 now)
{
	struct blkcg_gq *blkg = lat_to_blkg(iolat);
	struct iolatency_grp *parent;
	struct child_latency_info *lat_info;
	struct latency_stat stat;
	unsigned long flags;
	int cpu;

	latency_stat_init(iolat, &stat);
	preempt_disable();
	for_each_online_cpu(cpu) {
		struct latency_stat *s;
		s = per_cpu_ptr(iolat->stats, cpu);
		latency_stat_sum(iolat, &stat, s);
		latency_stat_init(iolat, s);
	}
	preempt_enable();

	parent = blkg_to_lat(blkg->parent);
	if (!parent)
		return;

	lat_info = &parent->child_lat;

	iolat_update_total_lat_avg(iolat, &stat);

	 
	if (latency_sum_ok(iolat, &stat) &&
	    atomic_read(&lat_info->scale_cookie) == DEFAULT_SCALE_COOKIE)
		return;

	 
	spin_lock_irqsave(&lat_info->lock, flags);

	latency_stat_sum(iolat, &iolat->cur_stat, &stat);
	lat_info->nr_samples -= iolat->nr_samples;
	lat_info->nr_samples += latency_stat_samples(iolat, &iolat->cur_stat);
	iolat->nr_samples = latency_stat_samples(iolat, &iolat->cur_stat);

	if ((lat_info->last_scale_event >= now ||
	    now - lat_info->last_scale_event < BLKIOLATENCY_MIN_ADJUST_TIME))
		goto out;

	if (latency_sum_ok(iolat, &iolat->cur_stat) &&
	    latency_sum_ok(iolat, &stat)) {
		if (latency_stat_samples(iolat, &iolat->cur_stat) <
		    BLKIOLATENCY_MIN_GOOD_SAMPLES)
			goto out;
		if (lat_info->scale_grp == iolat) {
			lat_info->last_scale_event = now;
			scale_cookie_change(iolat->blkiolat, lat_info, true);
		}
	} else if (lat_info->scale_lat == 0 ||
		   lat_info->scale_lat >= iolat->min_lat_nsec) {
		lat_info->last_scale_event = now;
		if (!lat_info->scale_grp ||
		    lat_info->scale_lat > iolat->min_lat_nsec) {
			WRITE_ONCE(lat_info->scale_lat, iolat->min_lat_nsec);
			lat_info->scale_grp = iolat;
		}
		scale_cookie_change(iolat->blkiolat, lat_info, false);
	}
	latency_stat_init(iolat, &iolat->cur_stat);
out:
	spin_unlock_irqrestore(&lat_info->lock, flags);
}

static void blkcg_iolatency_done_bio(struct rq_qos *rqos, struct bio *bio)
{
	struct blkcg_gq *blkg;
	struct rq_wait *rqw;
	struct iolatency_grp *iolat;
	u64 window_start;
	u64 now;
	bool issue_as_root = bio_issue_as_root_blkg(bio);
	int inflight = 0;

	blkg = bio->bi_blkg;
	if (!blkg || !bio_flagged(bio, BIO_QOS_THROTTLED))
		return;

	iolat = blkg_to_lat(bio->bi_blkg);
	if (!iolat)
		return;

	if (!iolat->blkiolat->enabled)
		return;

	now = ktime_to_ns(ktime_get());
	while (blkg && blkg->parent) {
		iolat = blkg_to_lat(blkg);
		if (!iolat) {
			blkg = blkg->parent;
			continue;
		}
		rqw = &iolat->rq_wait;

		inflight = atomic_dec_return(&rqw->inflight);
		WARN_ON_ONCE(inflight < 0);
		 
		if (iolat->min_lat_nsec && bio->bi_status != BLK_STS_AGAIN) {
			iolatency_record_time(iolat, &bio->bi_issue, now,
					      issue_as_root);
			window_start = atomic64_read(&iolat->window_start);
			if (now > window_start &&
			    (now - window_start) >= iolat->cur_win_nsec) {
				if (atomic64_try_cmpxchg(&iolat->window_start,
							 &window_start, now))
					iolatency_check_latencies(iolat, now);
			}
		}
		wake_up(&rqw->wait);
		blkg = blkg->parent;
	}
}

static void blkcg_iolatency_exit(struct rq_qos *rqos)
{
	struct blk_iolatency *blkiolat = BLKIOLATENCY(rqos);

	timer_shutdown_sync(&blkiolat->timer);
	flush_work(&blkiolat->enable_work);
	blkcg_deactivate_policy(rqos->disk, &blkcg_policy_iolatency);
	kfree(blkiolat);
}

static const struct rq_qos_ops blkcg_iolatency_ops = {
	.throttle = blkcg_iolatency_throttle,
	.done_bio = blkcg_iolatency_done_bio,
	.exit = blkcg_iolatency_exit,
};

static void blkiolatency_timer_fn(struct timer_list *t)
{
	struct blk_iolatency *blkiolat = from_timer(blkiolat, t, timer);
	struct blkcg_gq *blkg;
	struct cgroup_subsys_state *pos_css;
	u64 now = ktime_to_ns(ktime_get());

	rcu_read_lock();
	blkg_for_each_descendant_pre(blkg, pos_css,
				     blkiolat->rqos.disk->queue->root_blkg) {
		struct iolatency_grp *iolat;
		struct child_latency_info *lat_info;
		unsigned long flags;
		u64 cookie;

		 
		if (!blkg_tryget(blkg))
			continue;

		iolat = blkg_to_lat(blkg);
		if (!iolat)
			goto next;

		lat_info = &iolat->child_lat;
		cookie = atomic_read(&lat_info->scale_cookie);

		if (cookie >= DEFAULT_SCALE_COOKIE)
			goto next;

		spin_lock_irqsave(&lat_info->lock, flags);
		if (lat_info->last_scale_event >= now)
			goto next_lock;

		 
		if (lat_info->scale_grp == NULL) {
			scale_cookie_change(iolat->blkiolat, lat_info, true);
			goto next_lock;
		}

		 
		if (now - lat_info->last_scale_event >=
		    ((u64)NSEC_PER_SEC * 5))
			lat_info->scale_grp = NULL;
next_lock:
		spin_unlock_irqrestore(&lat_info->lock, flags);
next:
		blkg_put(blkg);
	}
	rcu_read_unlock();
}

 
static void blkiolatency_enable_work_fn(struct work_struct *work)
{
	struct blk_iolatency *blkiolat = container_of(work, struct blk_iolatency,
						      enable_work);
	bool enabled;

	 
	enabled = atomic_read(&blkiolat->enable_cnt);
	if (enabled != blkiolat->enabled) {
		blk_mq_freeze_queue(blkiolat->rqos.disk->queue);
		blkiolat->enabled = enabled;
		blk_mq_unfreeze_queue(blkiolat->rqos.disk->queue);
	}
}

static int blk_iolatency_init(struct gendisk *disk)
{
	struct blk_iolatency *blkiolat;
	int ret;

	blkiolat = kzalloc(sizeof(*blkiolat), GFP_KERNEL);
	if (!blkiolat)
		return -ENOMEM;

	ret = rq_qos_add(&blkiolat->rqos, disk, RQ_QOS_LATENCY,
			 &blkcg_iolatency_ops);
	if (ret)
		goto err_free;
	ret = blkcg_activate_policy(disk, &blkcg_policy_iolatency);
	if (ret)
		goto err_qos_del;

	timer_setup(&blkiolat->timer, blkiolatency_timer_fn, 0);
	INIT_WORK(&blkiolat->enable_work, blkiolatency_enable_work_fn);

	return 0;

err_qos_del:
	rq_qos_del(&blkiolat->rqos);
err_free:
	kfree(blkiolat);
	return ret;
}

static void iolatency_set_min_lat_nsec(struct blkcg_gq *blkg, u64 val)
{
	struct iolatency_grp *iolat = blkg_to_lat(blkg);
	struct blk_iolatency *blkiolat = iolat->blkiolat;
	u64 oldval = iolat->min_lat_nsec;

	iolat->min_lat_nsec = val;
	iolat->cur_win_nsec = max_t(u64, val << 4, BLKIOLATENCY_MIN_WIN_SIZE);
	iolat->cur_win_nsec = min_t(u64, iolat->cur_win_nsec,
				    BLKIOLATENCY_MAX_WIN_SIZE);

	if (!oldval && val) {
		if (atomic_inc_return(&blkiolat->enable_cnt) == 1)
			schedule_work(&blkiolat->enable_work);
	}
	if (oldval && !val) {
		blkcg_clear_delay(blkg);
		if (atomic_dec_return(&blkiolat->enable_cnt) == 0)
			schedule_work(&blkiolat->enable_work);
	}
}

static void iolatency_clear_scaling(struct blkcg_gq *blkg)
{
	if (blkg->parent) {
		struct iolatency_grp *iolat = blkg_to_lat(blkg->parent);
		struct child_latency_info *lat_info;
		if (!iolat)
			return;

		lat_info = &iolat->child_lat;
		spin_lock(&lat_info->lock);
		atomic_set(&lat_info->scale_cookie, DEFAULT_SCALE_COOKIE);
		lat_info->last_scale_event = 0;
		lat_info->scale_grp = NULL;
		lat_info->scale_lat = 0;
		spin_unlock(&lat_info->lock);
	}
}

static ssize_t iolatency_set_limit(struct kernfs_open_file *of, char *buf,
			     size_t nbytes, loff_t off)
{
	struct blkcg *blkcg = css_to_blkcg(of_css(of));
	struct blkcg_gq *blkg;
	struct blkg_conf_ctx ctx;
	struct iolatency_grp *iolat;
	char *p, *tok;
	u64 lat_val = 0;
	u64 oldval;
	int ret;

	blkg_conf_init(&ctx, buf);

	ret = blkg_conf_open_bdev(&ctx);
	if (ret)
		goto out;

	 
	lockdep_assert_held(&ctx.bdev->bd_queue->rq_qos_mutex);
	if (!iolat_rq_qos(ctx.bdev->bd_queue))
		ret = blk_iolatency_init(ctx.bdev->bd_disk);
	if (ret)
		goto out;

	ret = blkg_conf_prep(blkcg, &blkcg_policy_iolatency, &ctx);
	if (ret)
		goto out;

	iolat = blkg_to_lat(ctx.blkg);
	p = ctx.body;

	ret = -EINVAL;
	while ((tok = strsep(&p, " "))) {
		char key[16];
		char val[21];	 

		if (sscanf(tok, "%15[^=]=%20s", key, val) != 2)
			goto out;

		if (!strcmp(key, "target")) {
			u64 v;

			if (!strcmp(val, "max"))
				lat_val = 0;
			else if (sscanf(val, "%llu", &v) == 1)
				lat_val = v * NSEC_PER_USEC;
			else
				goto out;
		} else {
			goto out;
		}
	}

	 
	blkg = ctx.blkg;
	oldval = iolat->min_lat_nsec;

	iolatency_set_min_lat_nsec(blkg, lat_val);
	if (oldval != iolat->min_lat_nsec)
		iolatency_clear_scaling(blkg);
	ret = 0;
out:
	blkg_conf_exit(&ctx);
	return ret ?: nbytes;
}

static u64 iolatency_prfill_limit(struct seq_file *sf,
				  struct blkg_policy_data *pd, int off)
{
	struct iolatency_grp *iolat = pd_to_lat(pd);
	const char *dname = blkg_dev_name(pd->blkg);

	if (!dname || !iolat->min_lat_nsec)
		return 0;
	seq_printf(sf, "%s target=%llu\n",
		   dname, div_u64(iolat->min_lat_nsec, NSEC_PER_USEC));
	return 0;
}

static int iolatency_print_limit(struct seq_file *sf, void *v)
{
	blkcg_print_blkgs(sf, css_to_blkcg(seq_css(sf)),
			  iolatency_prfill_limit,
			  &blkcg_policy_iolatency, seq_cft(sf)->private, false);
	return 0;
}

static void iolatency_ssd_stat(struct iolatency_grp *iolat, struct seq_file *s)
{
	struct latency_stat stat;
	int cpu;

	latency_stat_init(iolat, &stat);
	preempt_disable();
	for_each_online_cpu(cpu) {
		struct latency_stat *s;
		s = per_cpu_ptr(iolat->stats, cpu);
		latency_stat_sum(iolat, &stat, s);
	}
	preempt_enable();

	if (iolat->max_depth == UINT_MAX)
		seq_printf(s, " missed=%llu total=%llu depth=max",
			(unsigned long long)stat.ps.missed,
			(unsigned long long)stat.ps.total);
	else
		seq_printf(s, " missed=%llu total=%llu depth=%u",
			(unsigned long long)stat.ps.missed,
			(unsigned long long)stat.ps.total,
			iolat->max_depth);
}

static void iolatency_pd_stat(struct blkg_policy_data *pd, struct seq_file *s)
{
	struct iolatency_grp *iolat = pd_to_lat(pd);
	unsigned long long avg_lat;
	unsigned long long cur_win;

	if (!blkcg_debug_stats)
		return;

	if (iolat->ssd)
		return iolatency_ssd_stat(iolat, s);

	avg_lat = div64_u64(iolat->lat_avg, NSEC_PER_USEC);
	cur_win = div64_u64(iolat->cur_win_nsec, NSEC_PER_MSEC);
	if (iolat->max_depth == UINT_MAX)
		seq_printf(s, " depth=max avg_lat=%llu win=%llu",
			avg_lat, cur_win);
	else
		seq_printf(s, " depth=%u avg_lat=%llu win=%llu",
			iolat->max_depth, avg_lat, cur_win);
}

static struct blkg_policy_data *iolatency_pd_alloc(struct gendisk *disk,
		struct blkcg *blkcg, gfp_t gfp)
{
	struct iolatency_grp *iolat;

	iolat = kzalloc_node(sizeof(*iolat), gfp, disk->node_id);
	if (!iolat)
		return NULL;
	iolat->stats = __alloc_percpu_gfp(sizeof(struct latency_stat),
				       __alignof__(struct latency_stat), gfp);
	if (!iolat->stats) {
		kfree(iolat);
		return NULL;
	}
	return &iolat->pd;
}

static void iolatency_pd_init(struct blkg_policy_data *pd)
{
	struct iolatency_grp *iolat = pd_to_lat(pd);
	struct blkcg_gq *blkg = lat_to_blkg(iolat);
	struct rq_qos *rqos = iolat_rq_qos(blkg->q);
	struct blk_iolatency *blkiolat = BLKIOLATENCY(rqos);
	u64 now = ktime_to_ns(ktime_get());
	int cpu;

	if (blk_queue_nonrot(blkg->q))
		iolat->ssd = true;
	else
		iolat->ssd = false;

	for_each_possible_cpu(cpu) {
		struct latency_stat *stat;
		stat = per_cpu_ptr(iolat->stats, cpu);
		latency_stat_init(iolat, stat);
	}

	latency_stat_init(iolat, &iolat->cur_stat);
	rq_wait_init(&iolat->rq_wait);
	spin_lock_init(&iolat->child_lat.lock);
	iolat->max_depth = UINT_MAX;
	iolat->blkiolat = blkiolat;
	iolat->cur_win_nsec = 100 * NSEC_PER_MSEC;
	atomic64_set(&iolat->window_start, now);

	 
	if (blkg->parent && blkg_to_pd(blkg->parent, &blkcg_policy_iolatency)) {
		struct iolatency_grp *parent = blkg_to_lat(blkg->parent);
		atomic_set(&iolat->scale_cookie,
			   atomic_read(&parent->child_lat.scale_cookie));
	} else {
		atomic_set(&iolat->scale_cookie, DEFAULT_SCALE_COOKIE);
	}

	atomic_set(&iolat->child_lat.scale_cookie, DEFAULT_SCALE_COOKIE);
}

static void iolatency_pd_offline(struct blkg_policy_data *pd)
{
	struct iolatency_grp *iolat = pd_to_lat(pd);
	struct blkcg_gq *blkg = lat_to_blkg(iolat);

	iolatency_set_min_lat_nsec(blkg, 0);
	iolatency_clear_scaling(blkg);
}

static void iolatency_pd_free(struct blkg_policy_data *pd)
{
	struct iolatency_grp *iolat = pd_to_lat(pd);
	free_percpu(iolat->stats);
	kfree(iolat);
}

static struct cftype iolatency_files[] = {
	{
		.name = "latency",
		.flags = CFTYPE_NOT_ON_ROOT,
		.seq_show = iolatency_print_limit,
		.write = iolatency_set_limit,
	},
	{}
};

static struct blkcg_policy blkcg_policy_iolatency = {
	.dfl_cftypes	= iolatency_files,
	.pd_alloc_fn	= iolatency_pd_alloc,
	.pd_init_fn	= iolatency_pd_init,
	.pd_offline_fn	= iolatency_pd_offline,
	.pd_free_fn	= iolatency_pd_free,
	.pd_stat_fn	= iolatency_pd_stat,
};

static int __init iolatency_init(void)
{
	return blkcg_policy_register(&blkcg_policy_iolatency);
}

static void __exit iolatency_exit(void)
{
	blkcg_policy_unregister(&blkcg_policy_iolatency);
}

module_init(iolatency_init);
module_exit(iolatency_exit);
