 

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/timer.h>
#include <linux/time64.h>
#include <linux/parser.h>
#include <linux/sched/signal.h>
#include <asm/local.h>
#include <asm/local64.h>
#include "blk-rq-qos.h"
#include "blk-stat.h"
#include "blk-wbt.h"
#include "blk-cgroup.h"

#ifdef CONFIG_TRACEPOINTS

 
#define TRACE_IOCG_PATH_LEN 1024
static DEFINE_SPINLOCK(trace_iocg_path_lock);
static char trace_iocg_path[TRACE_IOCG_PATH_LEN];

#define TRACE_IOCG_PATH(type, iocg, ...)					\
	do {									\
		unsigned long flags;						\
		if (trace_iocost_##type##_enabled()) {				\
			spin_lock_irqsave(&trace_iocg_path_lock, flags);	\
			cgroup_path(iocg_to_blkg(iocg)->blkcg->css.cgroup,	\
				    trace_iocg_path, TRACE_IOCG_PATH_LEN);	\
			trace_iocost_##type(iocg, trace_iocg_path,		\
					      ##__VA_ARGS__);			\
			spin_unlock_irqrestore(&trace_iocg_path_lock, flags);	\
		}								\
	} while (0)

#else	 
#define TRACE_IOCG_PATH(type, iocg, ...)	do { } while (0)
#endif	 

enum {
	MILLION			= 1000000,

	 
	MIN_PERIOD		= USEC_PER_MSEC,
	MAX_PERIOD		= USEC_PER_SEC,

	 
	MARGIN_MIN_PCT		= 10,
	MARGIN_LOW_PCT		= 20,
	MARGIN_TARGET_PCT	= 50,

	INUSE_ADJ_STEP_PCT	= 25,

	 
	TIMER_SLACK_PCT		= 1,

	 
	WEIGHT_ONE		= 1 << 16,
};

enum {
	 
	VTIME_PER_SEC_SHIFT	= 37,
	VTIME_PER_SEC		= 1LLU << VTIME_PER_SEC_SHIFT,
	VTIME_PER_USEC		= VTIME_PER_SEC / USEC_PER_SEC,
	VTIME_PER_NSEC		= VTIME_PER_SEC / NSEC_PER_SEC,

	 
	VRATE_MIN_PPM		= 10000,	 
	VRATE_MAX_PPM		= 100000000,	 

	VRATE_MIN		= VTIME_PER_USEC * VRATE_MIN_PPM / MILLION,
	VRATE_CLAMP_ADJ_PCT	= 4,

	 
	AUTOP_CYCLE_NSEC	= 10LLU * NSEC_PER_SEC,
};

enum {
	 
	RQ_WAIT_BUSY_PCT	= 5,

	 
	UNBUSY_THR_PCT		= 75,

	 
	MIN_DELAY_THR_PCT	= 500,
	MAX_DELAY_THR_PCT	= 25000,
	MIN_DELAY		= 250,
	MAX_DELAY		= 250 * USEC_PER_MSEC,

	 
	DFGV_USAGE_PCT		= 50,
	DFGV_PERIOD		= 100 * USEC_PER_MSEC,

	 
	MAX_LAGGING_PERIODS	= 10,

	 
	IOC_PAGE_SHIFT		= 12,
	IOC_PAGE_SIZE		= 1 << IOC_PAGE_SHIFT,
	IOC_SECT_TO_PAGE_SHIFT	= IOC_PAGE_SHIFT - SECTOR_SHIFT,

	 
	LCOEF_RANDIO_PAGES	= 4096,
};

enum ioc_running {
	IOC_IDLE,
	IOC_RUNNING,
	IOC_STOP,
};

 
enum {
	QOS_ENABLE,
	QOS_CTRL,
	NR_QOS_CTRL_PARAMS,
};

 
enum {
	QOS_RPPM,
	QOS_RLAT,
	QOS_WPPM,
	QOS_WLAT,
	QOS_MIN,
	QOS_MAX,
	NR_QOS_PARAMS,
};

 
enum {
	COST_CTRL,
	COST_MODEL,
	NR_COST_CTRL_PARAMS,
};

 
enum {
	I_LCOEF_RBPS,
	I_LCOEF_RSEQIOPS,
	I_LCOEF_RRANDIOPS,
	I_LCOEF_WBPS,
	I_LCOEF_WSEQIOPS,
	I_LCOEF_WRANDIOPS,
	NR_I_LCOEFS,
};

enum {
	LCOEF_RPAGE,
	LCOEF_RSEQIO,
	LCOEF_RRANDIO,
	LCOEF_WPAGE,
	LCOEF_WSEQIO,
	LCOEF_WRANDIO,
	NR_LCOEFS,
};

enum {
	AUTOP_INVALID,
	AUTOP_HDD,
	AUTOP_SSD_QD1,
	AUTOP_SSD_DFL,
	AUTOP_SSD_FAST,
};

struct ioc_params {
	u32				qos[NR_QOS_PARAMS];
	u64				i_lcoefs[NR_I_LCOEFS];
	u64				lcoefs[NR_LCOEFS];
	u32				too_fast_vrate_pct;
	u32				too_slow_vrate_pct;
};

struct ioc_margins {
	s64				min;
	s64				low;
	s64				target;
};

struct ioc_missed {
	local_t				nr_met;
	local_t				nr_missed;
	u32				last_met;
	u32				last_missed;
};

struct ioc_pcpu_stat {
	struct ioc_missed		missed[2];

	local64_t			rq_wait_ns;
	u64				last_rq_wait_ns;
};

 
struct ioc {
	struct rq_qos			rqos;

	bool				enabled;

	struct ioc_params		params;
	struct ioc_margins		margins;
	u32				period_us;
	u32				timer_slack_ns;
	u64				vrate_min;
	u64				vrate_max;

	spinlock_t			lock;
	struct timer_list		timer;
	struct list_head		active_iocgs;	 
	struct ioc_pcpu_stat __percpu	*pcpu_stat;

	enum ioc_running		running;
	atomic64_t			vtime_rate;
	u64				vtime_base_rate;
	s64				vtime_err;

	seqcount_spinlock_t		period_seqcount;
	u64				period_at;	 
	u64				period_at_vtime;  

	atomic64_t			cur_period;	 
	int				busy_level;	 

	bool				weights_updated;
	atomic_t			hweight_gen;	 

	 
	u64				dfgv_period_at;
	u64				dfgv_period_rem;
	u64				dfgv_usage_us_sum;

	u64				autop_too_fast_at;
	u64				autop_too_slow_at;
	int				autop_idx;
	bool				user_qos_params:1;
	bool				user_cost_model:1;
};

struct iocg_pcpu_stat {
	local64_t			abs_vusage;
};

struct iocg_stat {
	u64				usage_us;
	u64				wait_us;
	u64				indebt_us;
	u64				indelay_us;
};

 
struct ioc_gq {
	struct blkg_policy_data		pd;
	struct ioc			*ioc;

	 
	u32				cfg_weight;
	u32				weight;
	u32				active;
	u32				inuse;

	u32				last_inuse;
	s64				saved_margin;

	sector_t			cursor;		 

	 
	atomic64_t			vtime;
	atomic64_t			done_vtime;
	u64				abs_vdebt;

	 
	u64				delay;
	u64				delay_at;

	 
	atomic64_t			active_period;
	struct list_head		active_list;

	 
	u64				child_active_sum;
	u64				child_inuse_sum;
	u64				child_adjusted_sum;
	int				hweight_gen;
	u32				hweight_active;
	u32				hweight_inuse;
	u32				hweight_donating;
	u32				hweight_after_donation;

	struct list_head		walk_list;
	struct list_head		surplus_list;

	struct wait_queue_head		waitq;
	struct hrtimer			waitq_timer;

	 
	u64				activated_at;

	 
	struct iocg_pcpu_stat __percpu	*pcpu_stat;
	struct iocg_stat		stat;
	struct iocg_stat		last_stat;
	u64				last_stat_abs_vusage;
	u64				usage_delta_us;
	u64				wait_since;
	u64				indebt_since;
	u64				indelay_since;

	 
	int				level;
	struct ioc_gq			*ancestors[];
};

 
struct ioc_cgrp {
	struct blkcg_policy_data	cpd;
	unsigned int			dfl_weight;
};

struct ioc_now {
	u64				now_ns;
	u64				now;
	u64				vnow;
};

struct iocg_wait {
	struct wait_queue_entry		wait;
	struct bio			*bio;
	u64				abs_cost;
	bool				committed;
};

struct iocg_wake_ctx {
	struct ioc_gq			*iocg;
	u32				hw_inuse;
	s64				vbudget;
};

static const struct ioc_params autop[] = {
	[AUTOP_HDD] = {
		.qos				= {
			[QOS_RLAT]		=        250000,  
			[QOS_WLAT]		=        250000,
			[QOS_MIN]		= VRATE_MIN_PPM,
			[QOS_MAX]		= VRATE_MAX_PPM,
		},
		.i_lcoefs			= {
			[I_LCOEF_RBPS]		=     174019176,
			[I_LCOEF_RSEQIOPS]	=         41708,
			[I_LCOEF_RRANDIOPS]	=           370,
			[I_LCOEF_WBPS]		=     178075866,
			[I_LCOEF_WSEQIOPS]	=         42705,
			[I_LCOEF_WRANDIOPS]	=           378,
		},
	},
	[AUTOP_SSD_QD1] = {
		.qos				= {
			[QOS_RLAT]		=         25000,  
			[QOS_WLAT]		=         25000,
			[QOS_MIN]		= VRATE_MIN_PPM,
			[QOS_MAX]		= VRATE_MAX_PPM,
		},
		.i_lcoefs			= {
			[I_LCOEF_RBPS]		=     245855193,
			[I_LCOEF_RSEQIOPS]	=         61575,
			[I_LCOEF_RRANDIOPS]	=          6946,
			[I_LCOEF_WBPS]		=     141365009,
			[I_LCOEF_WSEQIOPS]	=         33716,
			[I_LCOEF_WRANDIOPS]	=         26796,
		},
	},
	[AUTOP_SSD_DFL] = {
		.qos				= {
			[QOS_RLAT]		=         25000,  
			[QOS_WLAT]		=         25000,
			[QOS_MIN]		= VRATE_MIN_PPM,
			[QOS_MAX]		= VRATE_MAX_PPM,
		},
		.i_lcoefs			= {
			[I_LCOEF_RBPS]		=     488636629,
			[I_LCOEF_RSEQIOPS]	=          8932,
			[I_LCOEF_RRANDIOPS]	=          8518,
			[I_LCOEF_WBPS]		=     427891549,
			[I_LCOEF_WSEQIOPS]	=         28755,
			[I_LCOEF_WRANDIOPS]	=         21940,
		},
		.too_fast_vrate_pct		=           500,
	},
	[AUTOP_SSD_FAST] = {
		.qos				= {
			[QOS_RLAT]		=          5000,  
			[QOS_WLAT]		=          5000,
			[QOS_MIN]		= VRATE_MIN_PPM,
			[QOS_MAX]		= VRATE_MAX_PPM,
		},
		.i_lcoefs			= {
			[I_LCOEF_RBPS]		=    3102524156LLU,
			[I_LCOEF_RSEQIOPS]	=        724816,
			[I_LCOEF_RRANDIOPS]	=        778122,
			[I_LCOEF_WBPS]		=    1742780862LLU,
			[I_LCOEF_WSEQIOPS]	=        425702,
			[I_LCOEF_WRANDIOPS]	=	 443193,
		},
		.too_slow_vrate_pct		=            10,
	},
};

 
static u32 vrate_adj_pct[] =
	{ 0, 0, 0, 0,
	  1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
	  2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,
	  4, 4, 4, 4, 4, 4, 4, 4, 8, 8, 8, 8, 8, 8, 8, 8, 16 };

static struct blkcg_policy blkcg_policy_iocost;

 
static struct ioc *rqos_to_ioc(struct rq_qos *rqos)
{
	return container_of(rqos, struct ioc, rqos);
}

static struct ioc *q_to_ioc(struct request_queue *q)
{
	return rqos_to_ioc(rq_qos_id(q, RQ_QOS_COST));
}

static const char __maybe_unused *ioc_name(struct ioc *ioc)
{
	struct gendisk *disk = ioc->rqos.disk;

	if (!disk)
		return "<unknown>";
	return disk->disk_name;
}

static struct ioc_gq *pd_to_iocg(struct blkg_policy_data *pd)
{
	return pd ? container_of(pd, struct ioc_gq, pd) : NULL;
}

static struct ioc_gq *blkg_to_iocg(struct blkcg_gq *blkg)
{
	return pd_to_iocg(blkg_to_pd(blkg, &blkcg_policy_iocost));
}

static struct blkcg_gq *iocg_to_blkg(struct ioc_gq *iocg)
{
	return pd_to_blkg(&iocg->pd);
}

static struct ioc_cgrp *blkcg_to_iocc(struct blkcg *blkcg)
{
	return container_of(blkcg_to_cpd(blkcg, &blkcg_policy_iocost),
			    struct ioc_cgrp, cpd);
}

 
static u64 abs_cost_to_cost(u64 abs_cost, u32 hw_inuse)
{
	return DIV64_U64_ROUND_UP(abs_cost * WEIGHT_ONE, hw_inuse);
}

 
static u64 cost_to_abs_cost(u64 cost, u32 hw_inuse)
{
	return DIV64_U64_ROUND_UP(cost * hw_inuse, WEIGHT_ONE);
}

static void iocg_commit_bio(struct ioc_gq *iocg, struct bio *bio,
			    u64 abs_cost, u64 cost)
{
	struct iocg_pcpu_stat *gcs;

	bio->bi_iocost_cost = cost;
	atomic64_add(cost, &iocg->vtime);

	gcs = get_cpu_ptr(iocg->pcpu_stat);
	local64_add(abs_cost, &gcs->abs_vusage);
	put_cpu_ptr(gcs);
}

static void iocg_lock(struct ioc_gq *iocg, bool lock_ioc, unsigned long *flags)
{
	if (lock_ioc) {
		spin_lock_irqsave(&iocg->ioc->lock, *flags);
		spin_lock(&iocg->waitq.lock);
	} else {
		spin_lock_irqsave(&iocg->waitq.lock, *flags);
	}
}

static void iocg_unlock(struct ioc_gq *iocg, bool unlock_ioc, unsigned long *flags)
{
	if (unlock_ioc) {
		spin_unlock(&iocg->waitq.lock);
		spin_unlock_irqrestore(&iocg->ioc->lock, *flags);
	} else {
		spin_unlock_irqrestore(&iocg->waitq.lock, *flags);
	}
}

#define CREATE_TRACE_POINTS
#include <trace/events/iocost.h>

static void ioc_refresh_margins(struct ioc *ioc)
{
	struct ioc_margins *margins = &ioc->margins;
	u32 period_us = ioc->period_us;
	u64 vrate = ioc->vtime_base_rate;

	margins->min = (period_us * MARGIN_MIN_PCT / 100) * vrate;
	margins->low = (period_us * MARGIN_LOW_PCT / 100) * vrate;
	margins->target = (period_us * MARGIN_TARGET_PCT / 100) * vrate;
}

 
static void ioc_refresh_period_us(struct ioc *ioc)
{
	u32 ppm, lat, multi, period_us;

	lockdep_assert_held(&ioc->lock);

	 
	if (ioc->params.qos[QOS_RLAT] >= ioc->params.qos[QOS_WLAT]) {
		ppm = ioc->params.qos[QOS_RPPM];
		lat = ioc->params.qos[QOS_RLAT];
	} else {
		ppm = ioc->params.qos[QOS_WPPM];
		lat = ioc->params.qos[QOS_WLAT];
	}

	 
	if (ppm)
		multi = max_t(u32, (MILLION - ppm) / 50000, 2);
	else
		multi = 2;
	period_us = multi * lat;
	period_us = clamp_t(u32, period_us, MIN_PERIOD, MAX_PERIOD);

	 
	ioc->period_us = period_us;
	ioc->timer_slack_ns = div64_u64(
		(u64)period_us * NSEC_PER_USEC * TIMER_SLACK_PCT,
		100);
	ioc_refresh_margins(ioc);
}

 
static int ioc_autop_idx(struct ioc *ioc, struct gendisk *disk)
{
	int idx = ioc->autop_idx;
	const struct ioc_params *p = &autop[idx];
	u32 vrate_pct;
	u64 now_ns;

	 
	if (!blk_queue_nonrot(disk->queue))
		return AUTOP_HDD;

	 
	if (blk_queue_depth(disk->queue) == 1)
		return AUTOP_SSD_QD1;

	 
	if (idx < AUTOP_SSD_DFL)
		return AUTOP_SSD_DFL;

	 
	if (ioc->user_qos_params || ioc->user_cost_model)
		return idx;

	 
	vrate_pct = div64_u64(ioc->vtime_base_rate * 100, VTIME_PER_USEC);
	now_ns = ktime_get_ns();

	if (p->too_fast_vrate_pct && p->too_fast_vrate_pct <= vrate_pct) {
		if (!ioc->autop_too_fast_at)
			ioc->autop_too_fast_at = now_ns;
		if (now_ns - ioc->autop_too_fast_at >= AUTOP_CYCLE_NSEC)
			return idx + 1;
	} else {
		ioc->autop_too_fast_at = 0;
	}

	if (p->too_slow_vrate_pct && p->too_slow_vrate_pct >= vrate_pct) {
		if (!ioc->autop_too_slow_at)
			ioc->autop_too_slow_at = now_ns;
		if (now_ns - ioc->autop_too_slow_at >= AUTOP_CYCLE_NSEC)
			return idx - 1;
	} else {
		ioc->autop_too_slow_at = 0;
	}

	return idx;
}

 
static void calc_lcoefs(u64 bps, u64 seqiops, u64 randiops,
			u64 *page, u64 *seqio, u64 *randio)
{
	u64 v;

	*page = *seqio = *randio = 0;

	if (bps) {
		u64 bps_pages = DIV_ROUND_UP_ULL(bps, IOC_PAGE_SIZE);

		if (bps_pages)
			*page = DIV64_U64_ROUND_UP(VTIME_PER_SEC, bps_pages);
		else
			*page = 1;
	}

	if (seqiops) {
		v = DIV64_U64_ROUND_UP(VTIME_PER_SEC, seqiops);
		if (v > *page)
			*seqio = v - *page;
	}

	if (randiops) {
		v = DIV64_U64_ROUND_UP(VTIME_PER_SEC, randiops);
		if (v > *page)
			*randio = v - *page;
	}
}

static void ioc_refresh_lcoefs(struct ioc *ioc)
{
	u64 *u = ioc->params.i_lcoefs;
	u64 *c = ioc->params.lcoefs;

	calc_lcoefs(u[I_LCOEF_RBPS], u[I_LCOEF_RSEQIOPS], u[I_LCOEF_RRANDIOPS],
		    &c[LCOEF_RPAGE], &c[LCOEF_RSEQIO], &c[LCOEF_RRANDIO]);
	calc_lcoefs(u[I_LCOEF_WBPS], u[I_LCOEF_WSEQIOPS], u[I_LCOEF_WRANDIOPS],
		    &c[LCOEF_WPAGE], &c[LCOEF_WSEQIO], &c[LCOEF_WRANDIO]);
}

 
static bool ioc_refresh_params_disk(struct ioc *ioc, bool force,
				    struct gendisk *disk)
{
	const struct ioc_params *p;
	int idx;

	lockdep_assert_held(&ioc->lock);

	idx = ioc_autop_idx(ioc, disk);
	p = &autop[idx];

	if (idx == ioc->autop_idx && !force)
		return false;

	if (idx != ioc->autop_idx) {
		atomic64_set(&ioc->vtime_rate, VTIME_PER_USEC);
		ioc->vtime_base_rate = VTIME_PER_USEC;
	}

	ioc->autop_idx = idx;
	ioc->autop_too_fast_at = 0;
	ioc->autop_too_slow_at = 0;

	if (!ioc->user_qos_params)
		memcpy(ioc->params.qos, p->qos, sizeof(p->qos));
	if (!ioc->user_cost_model)
		memcpy(ioc->params.i_lcoefs, p->i_lcoefs, sizeof(p->i_lcoefs));

	ioc_refresh_period_us(ioc);
	ioc_refresh_lcoefs(ioc);

	ioc->vrate_min = DIV64_U64_ROUND_UP((u64)ioc->params.qos[QOS_MIN] *
					    VTIME_PER_USEC, MILLION);
	ioc->vrate_max = DIV64_U64_ROUND_UP((u64)ioc->params.qos[QOS_MAX] *
					    VTIME_PER_USEC, MILLION);

	return true;
}

static bool ioc_refresh_params(struct ioc *ioc, bool force)
{
	return ioc_refresh_params_disk(ioc, force, ioc->rqos.disk);
}

 
static void ioc_refresh_vrate(struct ioc *ioc, struct ioc_now *now)
{
	s64 pleft = ioc->period_at + ioc->period_us - now->now;
	s64 vperiod = ioc->period_us * ioc->vtime_base_rate;
	s64 vcomp, vcomp_min, vcomp_max;

	lockdep_assert_held(&ioc->lock);

	 
	if (pleft <= 0)
		goto done;

	 
	vcomp = -div64_s64(ioc->vtime_err, pleft);
	vcomp_min = -(ioc->vtime_base_rate >> 1);
	vcomp_max = ioc->vtime_base_rate;
	vcomp = clamp(vcomp, vcomp_min, vcomp_max);

	ioc->vtime_err += vcomp * pleft;

	atomic64_set(&ioc->vtime_rate, ioc->vtime_base_rate + vcomp);
done:
	 
	ioc->vtime_err = clamp(ioc->vtime_err, -vperiod, vperiod);
}

static void ioc_adjust_base_vrate(struct ioc *ioc, u32 rq_wait_pct,
				  int nr_lagging, int nr_shortages,
				  int prev_busy_level, u32 *missed_ppm)
{
	u64 vrate = ioc->vtime_base_rate;
	u64 vrate_min = ioc->vrate_min, vrate_max = ioc->vrate_max;

	if (!ioc->busy_level || (ioc->busy_level < 0 && nr_lagging)) {
		if (ioc->busy_level != prev_busy_level || nr_lagging)
			trace_iocost_ioc_vrate_adj(ioc, vrate,
						   missed_ppm, rq_wait_pct,
						   nr_lagging, nr_shortages);

		return;
	}

	 
	if (vrate < vrate_min) {
		vrate = div64_u64(vrate * (100 + VRATE_CLAMP_ADJ_PCT), 100);
		vrate = min(vrate, vrate_min);
	} else if (vrate > vrate_max) {
		vrate = div64_u64(vrate * (100 - VRATE_CLAMP_ADJ_PCT), 100);
		vrate = max(vrate, vrate_max);
	} else {
		int idx = min_t(int, abs(ioc->busy_level),
				ARRAY_SIZE(vrate_adj_pct) - 1);
		u32 adj_pct = vrate_adj_pct[idx];

		if (ioc->busy_level > 0)
			adj_pct = 100 - adj_pct;
		else
			adj_pct = 100 + adj_pct;

		vrate = clamp(DIV64_U64_ROUND_UP(vrate * adj_pct, 100),
			      vrate_min, vrate_max);
	}

	trace_iocost_ioc_vrate_adj(ioc, vrate, missed_ppm, rq_wait_pct,
				   nr_lagging, nr_shortages);

	ioc->vtime_base_rate = vrate;
	ioc_refresh_margins(ioc);
}

 
static void ioc_now(struct ioc *ioc, struct ioc_now *now)
{
	unsigned seq;
	u64 vrate;

	now->now_ns = ktime_get();
	now->now = ktime_to_us(now->now_ns);
	vrate = atomic64_read(&ioc->vtime_rate);

	 
	do {
		seq = read_seqcount_begin(&ioc->period_seqcount);
		now->vnow = ioc->period_at_vtime +
			(now->now - ioc->period_at) * vrate;
	} while (read_seqcount_retry(&ioc->period_seqcount, seq));
}

static void ioc_start_period(struct ioc *ioc, struct ioc_now *now)
{
	WARN_ON_ONCE(ioc->running != IOC_RUNNING);

	write_seqcount_begin(&ioc->period_seqcount);
	ioc->period_at = now->now;
	ioc->period_at_vtime = now->vnow;
	write_seqcount_end(&ioc->period_seqcount);

	ioc->timer.expires = jiffies + usecs_to_jiffies(ioc->period_us);
	add_timer(&ioc->timer);
}

 
static void __propagate_weights(struct ioc_gq *iocg, u32 active, u32 inuse,
				bool save, struct ioc_now *now)
{
	struct ioc *ioc = iocg->ioc;
	int lvl;

	lockdep_assert_held(&ioc->lock);

	 
	if (list_empty(&iocg->active_list) && iocg->child_active_sum) {
		inuse = DIV64_U64_ROUND_UP(active * iocg->child_inuse_sum,
					   iocg->child_active_sum);
	} else {
		inuse = clamp_t(u32, inuse, 1, active);
	}

	iocg->last_inuse = iocg->inuse;
	if (save)
		iocg->saved_margin = now->vnow - atomic64_read(&iocg->vtime);

	if (active == iocg->active && inuse == iocg->inuse)
		return;

	for (lvl = iocg->level - 1; lvl >= 0; lvl--) {
		struct ioc_gq *parent = iocg->ancestors[lvl];
		struct ioc_gq *child = iocg->ancestors[lvl + 1];
		u32 parent_active = 0, parent_inuse = 0;

		 
		parent->child_active_sum += (s32)(active - child->active);
		parent->child_inuse_sum += (s32)(inuse - child->inuse);
		 
		child->active = active;
		child->inuse = inuse;

		 
		if (parent->child_active_sum) {
			parent_active = parent->weight;
			parent_inuse = DIV64_U64_ROUND_UP(
				parent_active * parent->child_inuse_sum,
				parent->child_active_sum);
		}

		 
		if (parent_active == parent->active &&
		    parent_inuse == parent->inuse)
			break;

		active = parent_active;
		inuse = parent_inuse;
	}

	ioc->weights_updated = true;
}

static void commit_weights(struct ioc *ioc)
{
	lockdep_assert_held(&ioc->lock);

	if (ioc->weights_updated) {
		 
		smp_wmb();
		atomic_inc(&ioc->hweight_gen);
		ioc->weights_updated = false;
	}
}

static void propagate_weights(struct ioc_gq *iocg, u32 active, u32 inuse,
			      bool save, struct ioc_now *now)
{
	__propagate_weights(iocg, active, inuse, save, now);
	commit_weights(iocg->ioc);
}

static void current_hweight(struct ioc_gq *iocg, u32 *hw_activep, u32 *hw_inusep)
{
	struct ioc *ioc = iocg->ioc;
	int lvl;
	u32 hwa, hwi;
	int ioc_gen;

	 
	ioc_gen = atomic_read(&ioc->hweight_gen);
	if (ioc_gen == iocg->hweight_gen)
		goto out;

	 
	smp_rmb();

	hwa = hwi = WEIGHT_ONE;
	for (lvl = 0; lvl <= iocg->level - 1; lvl++) {
		struct ioc_gq *parent = iocg->ancestors[lvl];
		struct ioc_gq *child = iocg->ancestors[lvl + 1];
		u64 active_sum = READ_ONCE(parent->child_active_sum);
		u64 inuse_sum = READ_ONCE(parent->child_inuse_sum);
		u32 active = READ_ONCE(child->active);
		u32 inuse = READ_ONCE(child->inuse);

		 
		if (!active_sum || !inuse_sum)
			continue;

		active_sum = max_t(u64, active, active_sum);
		hwa = div64_u64((u64)hwa * active, active_sum);

		inuse_sum = max_t(u64, inuse, inuse_sum);
		hwi = div64_u64((u64)hwi * inuse, inuse_sum);
	}

	iocg->hweight_active = max_t(u32, hwa, 1);
	iocg->hweight_inuse = max_t(u32, hwi, 1);
	iocg->hweight_gen = ioc_gen;
out:
	if (hw_activep)
		*hw_activep = iocg->hweight_active;
	if (hw_inusep)
		*hw_inusep = iocg->hweight_inuse;
}

 
static u32 current_hweight_max(struct ioc_gq *iocg)
{
	u32 hwm = WEIGHT_ONE;
	u32 inuse = iocg->active;
	u64 child_inuse_sum;
	int lvl;

	lockdep_assert_held(&iocg->ioc->lock);

	for (lvl = iocg->level - 1; lvl >= 0; lvl--) {
		struct ioc_gq *parent = iocg->ancestors[lvl];
		struct ioc_gq *child = iocg->ancestors[lvl + 1];

		child_inuse_sum = parent->child_inuse_sum + inuse - child->inuse;
		hwm = div64_u64((u64)hwm * inuse, child_inuse_sum);
		inuse = DIV64_U64_ROUND_UP(parent->active * child_inuse_sum,
					   parent->child_active_sum);
	}

	return max_t(u32, hwm, 1);
}

static void weight_updated(struct ioc_gq *iocg, struct ioc_now *now)
{
	struct ioc *ioc = iocg->ioc;
	struct blkcg_gq *blkg = iocg_to_blkg(iocg);
	struct ioc_cgrp *iocc = blkcg_to_iocc(blkg->blkcg);
	u32 weight;

	lockdep_assert_held(&ioc->lock);

	weight = iocg->cfg_weight ?: iocc->dfl_weight;
	if (weight != iocg->weight && iocg->active)
		propagate_weights(iocg, weight, iocg->inuse, true, now);
	iocg->weight = weight;
}

static bool iocg_activate(struct ioc_gq *iocg, struct ioc_now *now)
{
	struct ioc *ioc = iocg->ioc;
	u64 last_period, cur_period;
	u64 vtime, vtarget;
	int i;

	 
	if (!list_empty(&iocg->active_list)) {
		ioc_now(ioc, now);
		cur_period = atomic64_read(&ioc->cur_period);
		if (atomic64_read(&iocg->active_period) != cur_period)
			atomic64_set(&iocg->active_period, cur_period);
		return true;
	}

	 
	if (iocg->child_active_sum)
		return false;

	spin_lock_irq(&ioc->lock);

	ioc_now(ioc, now);

	 
	cur_period = atomic64_read(&ioc->cur_period);
	last_period = atomic64_read(&iocg->active_period);
	atomic64_set(&iocg->active_period, cur_period);

	 
	if (!list_empty(&iocg->active_list))
		goto succeed_unlock;
	for (i = iocg->level - 1; i > 0; i--)
		if (!list_empty(&iocg->ancestors[i]->active_list))
			goto fail_unlock;

	if (iocg->child_active_sum)
		goto fail_unlock;

	 
	vtarget = now->vnow - ioc->margins.target;
	vtime = atomic64_read(&iocg->vtime);

	atomic64_add(vtarget - vtime, &iocg->vtime);
	atomic64_add(vtarget - vtime, &iocg->done_vtime);
	vtime = vtarget;

	 
	iocg->hweight_gen = atomic_read(&ioc->hweight_gen) - 1;
	list_add(&iocg->active_list, &ioc->active_iocgs);

	propagate_weights(iocg, iocg->weight,
			  iocg->last_inuse ?: iocg->weight, true, now);

	TRACE_IOCG_PATH(iocg_activate, iocg, now,
			last_period, cur_period, vtime);

	iocg->activated_at = now->now;

	if (ioc->running == IOC_IDLE) {
		ioc->running = IOC_RUNNING;
		ioc->dfgv_period_at = now->now;
		ioc->dfgv_period_rem = 0;
		ioc_start_period(ioc, now);
	}

succeed_unlock:
	spin_unlock_irq(&ioc->lock);
	return true;

fail_unlock:
	spin_unlock_irq(&ioc->lock);
	return false;
}

static bool iocg_kick_delay(struct ioc_gq *iocg, struct ioc_now *now)
{
	struct ioc *ioc = iocg->ioc;
	struct blkcg_gq *blkg = iocg_to_blkg(iocg);
	u64 tdelta, delay, new_delay;
	s64 vover, vover_pct;
	u32 hwa;

	lockdep_assert_held(&iocg->waitq.lock);

	 
	tdelta = now->now - iocg->delay_at;
	if (iocg->delay)
		delay = iocg->delay >> div64_u64(tdelta, USEC_PER_SEC);
	else
		delay = 0;

	 
	current_hweight(iocg, &hwa, NULL);
	vover = atomic64_read(&iocg->vtime) +
		abs_cost_to_cost(iocg->abs_vdebt, hwa) - now->vnow;
	vover_pct = div64_s64(100 * vover,
			      ioc->period_us * ioc->vtime_base_rate);

	if (vover_pct <= MIN_DELAY_THR_PCT)
		new_delay = 0;
	else if (vover_pct >= MAX_DELAY_THR_PCT)
		new_delay = MAX_DELAY;
	else
		new_delay = MIN_DELAY +
			div_u64((MAX_DELAY - MIN_DELAY) *
				(vover_pct - MIN_DELAY_THR_PCT),
				MAX_DELAY_THR_PCT - MIN_DELAY_THR_PCT);

	 
	if (new_delay > delay) {
		iocg->delay = new_delay;
		iocg->delay_at = now->now;
		delay = new_delay;
	}

	if (delay >= MIN_DELAY) {
		if (!iocg->indelay_since)
			iocg->indelay_since = now->now;
		blkcg_set_delay(blkg, delay * NSEC_PER_USEC);
		return true;
	} else {
		if (iocg->indelay_since) {
			iocg->stat.indelay_us += now->now - iocg->indelay_since;
			iocg->indelay_since = 0;
		}
		iocg->delay = 0;
		blkcg_clear_delay(blkg);
		return false;
	}
}

static void iocg_incur_debt(struct ioc_gq *iocg, u64 abs_cost,
			    struct ioc_now *now)
{
	struct iocg_pcpu_stat *gcs;

	lockdep_assert_held(&iocg->ioc->lock);
	lockdep_assert_held(&iocg->waitq.lock);
	WARN_ON_ONCE(list_empty(&iocg->active_list));

	 
	if (!iocg->abs_vdebt && abs_cost) {
		iocg->indebt_since = now->now;
		propagate_weights(iocg, iocg->active, 0, false, now);
	}

	iocg->abs_vdebt += abs_cost;

	gcs = get_cpu_ptr(iocg->pcpu_stat);
	local64_add(abs_cost, &gcs->abs_vusage);
	put_cpu_ptr(gcs);
}

static void iocg_pay_debt(struct ioc_gq *iocg, u64 abs_vpay,
			  struct ioc_now *now)
{
	lockdep_assert_held(&iocg->ioc->lock);
	lockdep_assert_held(&iocg->waitq.lock);

	 
	WARN_ON_ONCE(list_empty(&iocg->active_list));
	WARN_ON_ONCE(iocg->inuse > 1);

	iocg->abs_vdebt -= min(abs_vpay, iocg->abs_vdebt);

	 
	if (!iocg->abs_vdebt) {
		iocg->stat.indebt_us += now->now - iocg->indebt_since;
		iocg->indebt_since = 0;

		propagate_weights(iocg, iocg->active, iocg->last_inuse,
				  false, now);
	}
}

static int iocg_wake_fn(struct wait_queue_entry *wq_entry, unsigned mode,
			int flags, void *key)
{
	struct iocg_wait *wait = container_of(wq_entry, struct iocg_wait, wait);
	struct iocg_wake_ctx *ctx = key;
	u64 cost = abs_cost_to_cost(wait->abs_cost, ctx->hw_inuse);

	ctx->vbudget -= cost;

	if (ctx->vbudget < 0)
		return -1;

	iocg_commit_bio(ctx->iocg, wait->bio, wait->abs_cost, cost);
	wait->committed = true;

	 
	default_wake_function(wq_entry, mode, flags, key);
	list_del_init_careful(&wq_entry->entry);
	return 0;
}

 
static void iocg_kick_waitq(struct ioc_gq *iocg, bool pay_debt,
			    struct ioc_now *now)
{
	struct ioc *ioc = iocg->ioc;
	struct iocg_wake_ctx ctx = { .iocg = iocg };
	u64 vshortage, expires, oexpires;
	s64 vbudget;
	u32 hwa;

	lockdep_assert_held(&iocg->waitq.lock);

	current_hweight(iocg, &hwa, NULL);
	vbudget = now->vnow - atomic64_read(&iocg->vtime);

	 
	if (pay_debt && iocg->abs_vdebt && vbudget > 0) {
		u64 abs_vbudget = cost_to_abs_cost(vbudget, hwa);
		u64 abs_vpay = min_t(u64, abs_vbudget, iocg->abs_vdebt);
		u64 vpay = abs_cost_to_cost(abs_vpay, hwa);

		lockdep_assert_held(&ioc->lock);

		atomic64_add(vpay, &iocg->vtime);
		atomic64_add(vpay, &iocg->done_vtime);
		iocg_pay_debt(iocg, abs_vpay, now);
		vbudget -= vpay;
	}

	if (iocg->abs_vdebt || iocg->delay)
		iocg_kick_delay(iocg, now);

	 
	if (iocg->abs_vdebt) {
		s64 vdebt = abs_cost_to_cost(iocg->abs_vdebt, hwa);
		vbudget = min_t(s64, 0, vbudget - vdebt);
	}

	 
	ctx.vbudget = vbudget;
	current_hweight(iocg, NULL, &ctx.hw_inuse);

	__wake_up_locked_key(&iocg->waitq, TASK_NORMAL, &ctx);

	if (!waitqueue_active(&iocg->waitq)) {
		if (iocg->wait_since) {
			iocg->stat.wait_us += now->now - iocg->wait_since;
			iocg->wait_since = 0;
		}
		return;
	}

	if (!iocg->wait_since)
		iocg->wait_since = now->now;

	if (WARN_ON_ONCE(ctx.vbudget >= 0))
		return;

	 
	vshortage = -ctx.vbudget;
	expires = now->now_ns +
		DIV64_U64_ROUND_UP(vshortage, ioc->vtime_base_rate) *
		NSEC_PER_USEC;
	expires += ioc->timer_slack_ns;

	 
	oexpires = ktime_to_ns(hrtimer_get_softexpires(&iocg->waitq_timer));
	if (hrtimer_is_queued(&iocg->waitq_timer) &&
	    abs(oexpires - expires) <= ioc->timer_slack_ns)
		return;

	hrtimer_start_range_ns(&iocg->waitq_timer, ns_to_ktime(expires),
			       ioc->timer_slack_ns, HRTIMER_MODE_ABS);
}

static enum hrtimer_restart iocg_waitq_timer_fn(struct hrtimer *timer)
{
	struct ioc_gq *iocg = container_of(timer, struct ioc_gq, waitq_timer);
	bool pay_debt = READ_ONCE(iocg->abs_vdebt);
	struct ioc_now now;
	unsigned long flags;

	ioc_now(iocg->ioc, &now);

	iocg_lock(iocg, pay_debt, &flags);
	iocg_kick_waitq(iocg, pay_debt, &now);
	iocg_unlock(iocg, pay_debt, &flags);

	return HRTIMER_NORESTART;
}

static void ioc_lat_stat(struct ioc *ioc, u32 *missed_ppm_ar, u32 *rq_wait_pct_p)
{
	u32 nr_met[2] = { };
	u32 nr_missed[2] = { };
	u64 rq_wait_ns = 0;
	int cpu, rw;

	for_each_online_cpu(cpu) {
		struct ioc_pcpu_stat *stat = per_cpu_ptr(ioc->pcpu_stat, cpu);
		u64 this_rq_wait_ns;

		for (rw = READ; rw <= WRITE; rw++) {
			u32 this_met = local_read(&stat->missed[rw].nr_met);
			u32 this_missed = local_read(&stat->missed[rw].nr_missed);

			nr_met[rw] += this_met - stat->missed[rw].last_met;
			nr_missed[rw] += this_missed - stat->missed[rw].last_missed;
			stat->missed[rw].last_met = this_met;
			stat->missed[rw].last_missed = this_missed;
		}

		this_rq_wait_ns = local64_read(&stat->rq_wait_ns);
		rq_wait_ns += this_rq_wait_ns - stat->last_rq_wait_ns;
		stat->last_rq_wait_ns = this_rq_wait_ns;
	}

	for (rw = READ; rw <= WRITE; rw++) {
		if (nr_met[rw] + nr_missed[rw])
			missed_ppm_ar[rw] =
				DIV64_U64_ROUND_UP((u64)nr_missed[rw] * MILLION,
						   nr_met[rw] + nr_missed[rw]);
		else
			missed_ppm_ar[rw] = 0;
	}

	*rq_wait_pct_p = div64_u64(rq_wait_ns * 100,
				   ioc->period_us * NSEC_PER_USEC);
}

 
static bool iocg_is_idle(struct ioc_gq *iocg)
{
	struct ioc *ioc = iocg->ioc;

	 
	if (atomic64_read(&iocg->active_period) ==
	    atomic64_read(&ioc->cur_period))
		return false;

	 
	if (atomic64_read(&iocg->done_vtime) != atomic64_read(&iocg->vtime))
		return false;

	return true;
}

 
static void iocg_build_inner_walk(struct ioc_gq *iocg,
				  struct list_head *inner_walk)
{
	int lvl;

	WARN_ON_ONCE(!list_empty(&iocg->walk_list));

	 
	for (lvl = iocg->level - 1; lvl >= 0; lvl--) {
		if (!list_empty(&iocg->ancestors[lvl]->walk_list))
			break;
	}

	 
	while (++lvl <= iocg->level - 1) {
		struct ioc_gq *inner = iocg->ancestors[lvl];

		 
		list_add_tail(&inner->walk_list, inner_walk);
	}
}

 
static void iocg_flush_stat_upward(struct ioc_gq *iocg)
{
	if (iocg->level > 0) {
		struct iocg_stat *parent_stat =
			&iocg->ancestors[iocg->level - 1]->stat;

		parent_stat->usage_us +=
			iocg->stat.usage_us - iocg->last_stat.usage_us;
		parent_stat->wait_us +=
			iocg->stat.wait_us - iocg->last_stat.wait_us;
		parent_stat->indebt_us +=
			iocg->stat.indebt_us - iocg->last_stat.indebt_us;
		parent_stat->indelay_us +=
			iocg->stat.indelay_us - iocg->last_stat.indelay_us;
	}

	iocg->last_stat = iocg->stat;
}

 
static void iocg_flush_stat_leaf(struct ioc_gq *iocg, struct ioc_now *now)
{
	struct ioc *ioc = iocg->ioc;
	u64 abs_vusage = 0;
	u64 vusage_delta;
	int cpu;

	lockdep_assert_held(&iocg->ioc->lock);

	 
	for_each_possible_cpu(cpu) {
		abs_vusage += local64_read(
				per_cpu_ptr(&iocg->pcpu_stat->abs_vusage, cpu));
	}
	vusage_delta = abs_vusage - iocg->last_stat_abs_vusage;
	iocg->last_stat_abs_vusage = abs_vusage;

	iocg->usage_delta_us = div64_u64(vusage_delta, ioc->vtime_base_rate);
	iocg->stat.usage_us += iocg->usage_delta_us;

	iocg_flush_stat_upward(iocg);
}

 
static void iocg_flush_stat(struct list_head *target_iocgs, struct ioc_now *now)
{
	LIST_HEAD(inner_walk);
	struct ioc_gq *iocg, *tiocg;

	 
	list_for_each_entry(iocg, target_iocgs, active_list) {
		iocg_flush_stat_leaf(iocg, now);
		iocg_build_inner_walk(iocg, &inner_walk);
	}

	 
	list_for_each_entry_safe_reverse(iocg, tiocg, &inner_walk, walk_list) {
		iocg_flush_stat_upward(iocg);
		list_del_init(&iocg->walk_list);
	}
}

 
static u32 hweight_after_donation(struct ioc_gq *iocg, u32 old_hwi, u32 hwm,
				  u32 usage, struct ioc_now *now)
{
	struct ioc *ioc = iocg->ioc;
	u64 vtime = atomic64_read(&iocg->vtime);
	s64 excess, delta, target, new_hwi;

	 
	if (iocg->abs_vdebt)
		return 1;

	 
	if (waitqueue_active(&iocg->waitq) ||
	    time_after64(vtime, now->vnow - ioc->margins.min))
		return hwm;

	 
	excess = now->vnow - vtime - ioc->margins.target;
	if (excess > 0) {
		atomic64_add(excess, &iocg->vtime);
		atomic64_add(excess, &iocg->done_vtime);
		vtime += excess;
		ioc->vtime_err -= div64_u64(excess * old_hwi, WEIGHT_ONE);
	}

	 
	delta = div64_s64(WEIGHT_ONE * (now->vnow - vtime),
			  now->vnow - ioc->period_at_vtime);
	target = WEIGHT_ONE * MARGIN_TARGET_PCT / 100;
	new_hwi = div64_s64(WEIGHT_ONE * usage, WEIGHT_ONE - target + delta);

	return clamp_t(s64, new_hwi, 1, hwm);
}

 
static void transfer_surpluses(struct list_head *surpluses, struct ioc_now *now)
{
	LIST_HEAD(over_hwa);
	LIST_HEAD(inner_walk);
	struct ioc_gq *iocg, *tiocg, *root_iocg;
	u32 after_sum, over_sum, over_target, gamma;

	 
	after_sum = 0;
	over_sum = 0;
	list_for_each_entry(iocg, surpluses, surplus_list) {
		u32 hwa;

		current_hweight(iocg, &hwa, NULL);
		after_sum += iocg->hweight_after_donation;

		if (iocg->hweight_after_donation > hwa) {
			over_sum += iocg->hweight_after_donation;
			list_add(&iocg->walk_list, &over_hwa);
		}
	}

	if (after_sum >= WEIGHT_ONE) {
		 
		u32 over_delta = after_sum - (WEIGHT_ONE - 1);
		WARN_ON_ONCE(over_sum <= over_delta);
		over_target = over_sum - over_delta;
	} else {
		over_target = 0;
	}

	list_for_each_entry_safe(iocg, tiocg, &over_hwa, walk_list) {
		if (over_target)
			iocg->hweight_after_donation =
				div_u64((u64)iocg->hweight_after_donation *
					over_target, over_sum);
		list_del_init(&iocg->walk_list);
	}

	 
	list_for_each_entry(iocg, surpluses, surplus_list) {
		iocg_build_inner_walk(iocg, &inner_walk);
	}

	root_iocg = list_first_entry(&inner_walk, struct ioc_gq, walk_list);
	WARN_ON_ONCE(root_iocg->level > 0);

	list_for_each_entry(iocg, &inner_walk, walk_list) {
		iocg->child_adjusted_sum = 0;
		iocg->hweight_donating = 0;
		iocg->hweight_after_donation = 0;
	}

	 
	list_for_each_entry(iocg, surpluses, surplus_list) {
		struct ioc_gq *parent = iocg->ancestors[iocg->level - 1];

		parent->hweight_donating += iocg->hweight_donating;
		parent->hweight_after_donation += iocg->hweight_after_donation;
	}

	list_for_each_entry_reverse(iocg, &inner_walk, walk_list) {
		if (iocg->level > 0) {
			struct ioc_gq *parent = iocg->ancestors[iocg->level - 1];

			parent->hweight_donating += iocg->hweight_donating;
			parent->hweight_after_donation += iocg->hweight_after_donation;
		}
	}

	 
	list_for_each_entry(iocg, &inner_walk, walk_list) {
		if (iocg->level) {
			struct ioc_gq *parent = iocg->ancestors[iocg->level - 1];

			iocg->hweight_active = DIV64_U64_ROUND_UP(
				(u64)parent->hweight_active * iocg->active,
				parent->child_active_sum);

		}

		iocg->hweight_donating = min(iocg->hweight_donating,
					     iocg->hweight_active);
		iocg->hweight_after_donation = min(iocg->hweight_after_donation,
						   iocg->hweight_donating - 1);
		if (WARN_ON_ONCE(iocg->hweight_active <= 1 ||
				 iocg->hweight_donating <= 1 ||
				 iocg->hweight_after_donation == 0)) {
			pr_warn("iocg: invalid donation weights in ");
			pr_cont_cgroup_path(iocg_to_blkg(iocg)->blkcg->css.cgroup);
			pr_cont(": active=%u donating=%u after=%u\n",
				iocg->hweight_active, iocg->hweight_donating,
				iocg->hweight_after_donation);
		}
	}

	 
	gamma = DIV_ROUND_UP(
		(WEIGHT_ONE - root_iocg->hweight_after_donation) * WEIGHT_ONE,
		WEIGHT_ONE - min_t(u32, root_iocg->hweight_donating, WEIGHT_ONE - 1));

	 
	list_for_each_entry(iocg, &inner_walk, walk_list) {
		struct ioc_gq *parent;
		u32 inuse, wpt, wptp;
		u64 st, sf;

		if (iocg->level == 0) {
			 
			iocg->child_adjusted_sum = DIV64_U64_ROUND_UP(
				iocg->child_active_sum * (WEIGHT_ONE - iocg->hweight_donating),
				WEIGHT_ONE - iocg->hweight_after_donation);
			continue;
		}

		parent = iocg->ancestors[iocg->level - 1];

		 
		iocg->hweight_inuse = DIV64_U64_ROUND_UP(
			(u64)gamma * (iocg->hweight_active - iocg->hweight_donating),
			WEIGHT_ONE) + iocg->hweight_after_donation;

		 
		inuse = DIV64_U64_ROUND_UP(
			(u64)parent->child_adjusted_sum * iocg->hweight_inuse,
			parent->hweight_inuse);

		 
		st = DIV64_U64_ROUND_UP(
			iocg->child_active_sum * iocg->hweight_donating,
			iocg->hweight_active);
		sf = iocg->child_active_sum - st;
		wpt = DIV64_U64_ROUND_UP(
			(u64)iocg->active * iocg->hweight_donating,
			iocg->hweight_active);
		wptp = DIV64_U64_ROUND_UP(
			(u64)inuse * iocg->hweight_after_donation,
			iocg->hweight_inuse);

		iocg->child_adjusted_sum = sf + DIV64_U64_ROUND_UP(st * wptp, wpt);
	}

	 
	list_for_each_entry(iocg, surpluses, surplus_list) {
		struct ioc_gq *parent = iocg->ancestors[iocg->level - 1];
		u32 inuse;

		 
		if (iocg->abs_vdebt) {
			WARN_ON_ONCE(iocg->inuse > 1);
			continue;
		}

		 
		inuse = DIV64_U64_ROUND_UP(
			parent->child_adjusted_sum * iocg->hweight_after_donation,
			parent->hweight_inuse);

		TRACE_IOCG_PATH(inuse_transfer, iocg, now,
				iocg->inuse, inuse,
				iocg->hweight_inuse,
				iocg->hweight_after_donation);

		__propagate_weights(iocg, iocg->active, inuse, true, now);
	}

	 
	list_for_each_entry_safe(iocg, tiocg, &inner_walk, walk_list)
		list_del_init(&iocg->walk_list);
}

 
static void ioc_forgive_debts(struct ioc *ioc, u64 usage_us_sum, int nr_debtors,
			      struct ioc_now *now)
{
	struct ioc_gq *iocg;
	u64 dur, usage_pct, nr_cycles;

	 
	if (!nr_debtors) {
		ioc->dfgv_period_at = now->now;
		ioc->dfgv_period_rem = 0;
		ioc->dfgv_usage_us_sum = 0;
		return;
	}

	 
	if (ioc->busy_level > 0)
		usage_us_sum = max_t(u64, usage_us_sum, ioc->period_us);

	ioc->dfgv_usage_us_sum += usage_us_sum;
	if (time_before64(now->now, ioc->dfgv_period_at + DFGV_PERIOD))
		return;

	 
	dur = now->now - ioc->dfgv_period_at;
	usage_pct = div64_u64(100 * ioc->dfgv_usage_us_sum, dur);

	ioc->dfgv_period_at = now->now;
	ioc->dfgv_usage_us_sum = 0;

	 
	if (usage_pct > DFGV_USAGE_PCT) {
		ioc->dfgv_period_rem = 0;
		return;
	}

	 
	nr_cycles = dur + ioc->dfgv_period_rem;
	ioc->dfgv_period_rem = do_div(nr_cycles, DFGV_PERIOD);

	list_for_each_entry(iocg, &ioc->active_iocgs, active_list) {
		u64 __maybe_unused old_debt, __maybe_unused old_delay;

		if (!iocg->abs_vdebt && !iocg->delay)
			continue;

		spin_lock(&iocg->waitq.lock);

		old_debt = iocg->abs_vdebt;
		old_delay = iocg->delay;

		if (iocg->abs_vdebt)
			iocg->abs_vdebt = iocg->abs_vdebt >> nr_cycles ?: 1;
		if (iocg->delay)
			iocg->delay = iocg->delay >> nr_cycles ?: 1;

		iocg_kick_waitq(iocg, true, now);

		TRACE_IOCG_PATH(iocg_forgive_debt, iocg, now, usage_pct,
				old_debt, iocg->abs_vdebt,
				old_delay, iocg->delay);

		spin_unlock(&iocg->waitq.lock);
	}
}

 
static int ioc_check_iocgs(struct ioc *ioc, struct ioc_now *now)
{
	int nr_debtors = 0;
	struct ioc_gq *iocg, *tiocg;

	list_for_each_entry_safe(iocg, tiocg, &ioc->active_iocgs, active_list) {
		if (!waitqueue_active(&iocg->waitq) && !iocg->abs_vdebt &&
		    !iocg->delay && !iocg_is_idle(iocg))
			continue;

		spin_lock(&iocg->waitq.lock);

		 
		if (iocg->wait_since) {
			iocg->stat.wait_us += now->now - iocg->wait_since;
			iocg->wait_since = now->now;
		}
		if (iocg->indebt_since) {
			iocg->stat.indebt_us +=
				now->now - iocg->indebt_since;
			iocg->indebt_since = now->now;
		}
		if (iocg->indelay_since) {
			iocg->stat.indelay_us +=
				now->now - iocg->indelay_since;
			iocg->indelay_since = now->now;
		}

		if (waitqueue_active(&iocg->waitq) || iocg->abs_vdebt ||
		    iocg->delay) {
			 
			iocg_kick_waitq(iocg, true, now);
			if (iocg->abs_vdebt || iocg->delay)
				nr_debtors++;
		} else if (iocg_is_idle(iocg)) {
			 
			u64 vtime = atomic64_read(&iocg->vtime);
			s64 excess;

			 
			excess = now->vnow - vtime - ioc->margins.target;
			if (excess > 0) {
				u32 old_hwi;

				current_hweight(iocg, NULL, &old_hwi);
				ioc->vtime_err -= div64_u64(excess * old_hwi,
							    WEIGHT_ONE);
			}

			TRACE_IOCG_PATH(iocg_idle, iocg, now,
					atomic64_read(&iocg->active_period),
					atomic64_read(&ioc->cur_period), vtime);
			__propagate_weights(iocg, 0, 0, false, now);
			list_del_init(&iocg->active_list);
		}

		spin_unlock(&iocg->waitq.lock);
	}

	commit_weights(ioc);
	return nr_debtors;
}

static void ioc_timer_fn(struct timer_list *timer)
{
	struct ioc *ioc = container_of(timer, struct ioc, timer);
	struct ioc_gq *iocg, *tiocg;
	struct ioc_now now;
	LIST_HEAD(surpluses);
	int nr_debtors, nr_shortages = 0, nr_lagging = 0;
	u64 usage_us_sum = 0;
	u32 ppm_rthr;
	u32 ppm_wthr;
	u32 missed_ppm[2], rq_wait_pct;
	u64 period_vtime;
	int prev_busy_level;

	 
	ioc_lat_stat(ioc, missed_ppm, &rq_wait_pct);

	 
	spin_lock_irq(&ioc->lock);

	ppm_rthr = MILLION - ioc->params.qos[QOS_RPPM];
	ppm_wthr = MILLION - ioc->params.qos[QOS_WPPM];
	ioc_now(ioc, &now);

	period_vtime = now.vnow - ioc->period_at_vtime;
	if (WARN_ON_ONCE(!period_vtime)) {
		spin_unlock_irq(&ioc->lock);
		return;
	}

	nr_debtors = ioc_check_iocgs(ioc, &now);

	 
	iocg_flush_stat(&ioc->active_iocgs, &now);

	 
	list_for_each_entry(iocg, &ioc->active_iocgs, active_list) {
		u64 vdone, vtime, usage_us;
		u32 hw_active, hw_inuse;

		 
		vdone = atomic64_read(&iocg->done_vtime);
		vtime = atomic64_read(&iocg->vtime);
		current_hweight(iocg, &hw_active, &hw_inuse);

		 
		if ((ppm_rthr != MILLION || ppm_wthr != MILLION) &&
		    !atomic_read(&iocg_to_blkg(iocg)->use_delay) &&
		    time_after64(vtime, vdone) &&
		    time_after64(vtime, now.vnow -
				 MAX_LAGGING_PERIODS * period_vtime) &&
		    time_before64(vdone, now.vnow - period_vtime))
			nr_lagging++;

		 
		usage_us = iocg->usage_delta_us;
		usage_us_sum += usage_us;

		 
		WARN_ON_ONCE(!list_empty(&iocg->surplus_list));
		if (hw_inuse < hw_active ||
		    (!waitqueue_active(&iocg->waitq) &&
		     time_before64(vtime, now.vnow - ioc->margins.low))) {
			u32 hwa, old_hwi, hwm, new_hwi, usage;
			u64 usage_dur;

			if (vdone != vtime) {
				u64 inflight_us = DIV64_U64_ROUND_UP(
					cost_to_abs_cost(vtime - vdone, hw_inuse),
					ioc->vtime_base_rate);

				usage_us = max(usage_us, inflight_us);
			}

			 
			if (time_after64(iocg->activated_at, ioc->period_at))
				usage_dur = max_t(u64, now.now - iocg->activated_at, 1);
			else
				usage_dur = max_t(u64, now.now - ioc->period_at, 1);

			usage = clamp_t(u32,
				DIV64_U64_ROUND_UP(usage_us * WEIGHT_ONE,
						   usage_dur),
				1, WEIGHT_ONE);

			 
			current_hweight(iocg, &hwa, &old_hwi);
			hwm = current_hweight_max(iocg);
			new_hwi = hweight_after_donation(iocg, old_hwi, hwm,
							 usage, &now);
			 
			if (new_hwi < hwm && hwa >= 2) {
				iocg->hweight_donating = hwa;
				iocg->hweight_after_donation = new_hwi;
				list_add(&iocg->surplus_list, &surpluses);
			} else if (!iocg->abs_vdebt) {
				 
				TRACE_IOCG_PATH(inuse_shortage, iocg, &now,
						iocg->inuse, iocg->active,
						iocg->hweight_inuse, new_hwi);

				__propagate_weights(iocg, iocg->active,
						    iocg->active, true, &now);
				nr_shortages++;
			}
		} else {
			 
			nr_shortages++;
		}
	}

	if (!list_empty(&surpluses) && nr_shortages)
		transfer_surpluses(&surpluses, &now);

	commit_weights(ioc);

	 
	list_for_each_entry_safe(iocg, tiocg, &surpluses, surplus_list)
		list_del_init(&iocg->surplus_list);

	 
	prev_busy_level = ioc->busy_level;
	if (rq_wait_pct > RQ_WAIT_BUSY_PCT ||
	    missed_ppm[READ] > ppm_rthr ||
	    missed_ppm[WRITE] > ppm_wthr) {
		 
		ioc->busy_level = max(ioc->busy_level, 0);
		ioc->busy_level++;
	} else if (rq_wait_pct <= RQ_WAIT_BUSY_PCT * UNBUSY_THR_PCT / 100 &&
		   missed_ppm[READ] <= ppm_rthr * UNBUSY_THR_PCT / 100 &&
		   missed_ppm[WRITE] <= ppm_wthr * UNBUSY_THR_PCT / 100) {
		 
		if (nr_shortages) {
			 
			ioc->busy_level = min(ioc->busy_level, 0);

			 
			if (!nr_lagging)
				ioc->busy_level--;
		} else {
			 
			ioc->busy_level = 0;
		}
	} else {
		 
		ioc->busy_level = 0;
	}

	ioc->busy_level = clamp(ioc->busy_level, -1000, 1000);

	ioc_adjust_base_vrate(ioc, rq_wait_pct, nr_lagging, nr_shortages,
			      prev_busy_level, missed_ppm);

	ioc_refresh_params(ioc, false);

	ioc_forgive_debts(ioc, usage_us_sum, nr_debtors, &now);

	 
	atomic64_inc(&ioc->cur_period);

	if (ioc->running != IOC_STOP) {
		if (!list_empty(&ioc->active_iocgs)) {
			ioc_start_period(ioc, &now);
		} else {
			ioc->busy_level = 0;
			ioc->vtime_err = 0;
			ioc->running = IOC_IDLE;
		}

		ioc_refresh_vrate(ioc, &now);
	}

	spin_unlock_irq(&ioc->lock);
}

static u64 adjust_inuse_and_calc_cost(struct ioc_gq *iocg, u64 vtime,
				      u64 abs_cost, struct ioc_now *now)
{
	struct ioc *ioc = iocg->ioc;
	struct ioc_margins *margins = &ioc->margins;
	u32 __maybe_unused old_inuse = iocg->inuse, __maybe_unused old_hwi;
	u32 hwi, adj_step;
	s64 margin;
	u64 cost, new_inuse;
	unsigned long flags;

	current_hweight(iocg, NULL, &hwi);
	old_hwi = hwi;
	cost = abs_cost_to_cost(abs_cost, hwi);
	margin = now->vnow - vtime - cost;

	 
	if (iocg->abs_vdebt)
		return cost;

	 
	if (margin >= iocg->saved_margin || margin >= margins->low ||
	    iocg->inuse == iocg->active)
		return cost;

	spin_lock_irqsave(&ioc->lock, flags);

	 
	if (iocg->abs_vdebt || list_empty(&iocg->active_list)) {
		spin_unlock_irqrestore(&ioc->lock, flags);
		return cost;
	}

	 
	new_inuse = iocg->inuse;
	adj_step = DIV_ROUND_UP(iocg->active * INUSE_ADJ_STEP_PCT, 100);
	do {
		new_inuse = new_inuse + adj_step;
		propagate_weights(iocg, iocg->active, new_inuse, true, now);
		current_hweight(iocg, NULL, &hwi);
		cost = abs_cost_to_cost(abs_cost, hwi);
	} while (time_after64(vtime + cost, now->vnow) &&
		 iocg->inuse != iocg->active);

	spin_unlock_irqrestore(&ioc->lock, flags);

	TRACE_IOCG_PATH(inuse_adjust, iocg, now,
			old_inuse, iocg->inuse, old_hwi, hwi);

	return cost;
}

static void calc_vtime_cost_builtin(struct bio *bio, struct ioc_gq *iocg,
				    bool is_merge, u64 *costp)
{
	struct ioc *ioc = iocg->ioc;
	u64 coef_seqio, coef_randio, coef_page;
	u64 pages = max_t(u64, bio_sectors(bio) >> IOC_SECT_TO_PAGE_SHIFT, 1);
	u64 seek_pages = 0;
	u64 cost = 0;

	 
	if (!bio->bi_iter.bi_size)
		goto out;

	switch (bio_op(bio)) {
	case REQ_OP_READ:
		coef_seqio	= ioc->params.lcoefs[LCOEF_RSEQIO];
		coef_randio	= ioc->params.lcoefs[LCOEF_RRANDIO];
		coef_page	= ioc->params.lcoefs[LCOEF_RPAGE];
		break;
	case REQ_OP_WRITE:
		coef_seqio	= ioc->params.lcoefs[LCOEF_WSEQIO];
		coef_randio	= ioc->params.lcoefs[LCOEF_WRANDIO];
		coef_page	= ioc->params.lcoefs[LCOEF_WPAGE];
		break;
	default:
		goto out;
	}

	if (iocg->cursor) {
		seek_pages = abs(bio->bi_iter.bi_sector - iocg->cursor);
		seek_pages >>= IOC_SECT_TO_PAGE_SHIFT;
	}

	if (!is_merge) {
		if (seek_pages > LCOEF_RANDIO_PAGES) {
			cost += coef_randio;
		} else {
			cost += coef_seqio;
		}
	}
	cost += pages * coef_page;
out:
	*costp = cost;
}

static u64 calc_vtime_cost(struct bio *bio, struct ioc_gq *iocg, bool is_merge)
{
	u64 cost;

	calc_vtime_cost_builtin(bio, iocg, is_merge, &cost);
	return cost;
}

static void calc_size_vtime_cost_builtin(struct request *rq, struct ioc *ioc,
					 u64 *costp)
{
	unsigned int pages = blk_rq_stats_sectors(rq) >> IOC_SECT_TO_PAGE_SHIFT;

	switch (req_op(rq)) {
	case REQ_OP_READ:
		*costp = pages * ioc->params.lcoefs[LCOEF_RPAGE];
		break;
	case REQ_OP_WRITE:
		*costp = pages * ioc->params.lcoefs[LCOEF_WPAGE];
		break;
	default:
		*costp = 0;
	}
}

static u64 calc_size_vtime_cost(struct request *rq, struct ioc *ioc)
{
	u64 cost;

	calc_size_vtime_cost_builtin(rq, ioc, &cost);
	return cost;
}

static void ioc_rqos_throttle(struct rq_qos *rqos, struct bio *bio)
{
	struct blkcg_gq *blkg = bio->bi_blkg;
	struct ioc *ioc = rqos_to_ioc(rqos);
	struct ioc_gq *iocg = blkg_to_iocg(blkg);
	struct ioc_now now;
	struct iocg_wait wait;
	u64 abs_cost, cost, vtime;
	bool use_debt, ioc_locked;
	unsigned long flags;

	 
	if (!ioc->enabled || !iocg || !iocg->level)
		return;

	 
	abs_cost = calc_vtime_cost(bio, iocg, false);
	if (!abs_cost)
		return;

	if (!iocg_activate(iocg, &now))
		return;

	iocg->cursor = bio_end_sector(bio);
	vtime = atomic64_read(&iocg->vtime);
	cost = adjust_inuse_and_calc_cost(iocg, vtime, abs_cost, &now);

	 
	if (!waitqueue_active(&iocg->waitq) && !iocg->abs_vdebt &&
	    time_before_eq64(vtime + cost, now.vnow)) {
		iocg_commit_bio(iocg, bio, abs_cost, cost);
		return;
	}

	 
	use_debt = bio_issue_as_root_blkg(bio) || fatal_signal_pending(current);
	ioc_locked = use_debt || READ_ONCE(iocg->abs_vdebt);
retry_lock:
	iocg_lock(iocg, ioc_locked, &flags);

	 
	if (unlikely(list_empty(&iocg->active_list))) {
		iocg_unlock(iocg, ioc_locked, &flags);
		iocg_commit_bio(iocg, bio, abs_cost, cost);
		return;
	}

	 
	if (use_debt) {
		iocg_incur_debt(iocg, abs_cost, &now);
		if (iocg_kick_delay(iocg, &now))
			blkcg_schedule_throttle(rqos->disk,
					(bio->bi_opf & REQ_SWAP) == REQ_SWAP);
		iocg_unlock(iocg, ioc_locked, &flags);
		return;
	}

	 
	if (!iocg->abs_vdebt && iocg->inuse != iocg->active) {
		if (!ioc_locked) {
			iocg_unlock(iocg, false, &flags);
			ioc_locked = true;
			goto retry_lock;
		}
		propagate_weights(iocg, iocg->active, iocg->active, true,
				  &now);
	}

	 
	init_waitqueue_func_entry(&wait.wait, iocg_wake_fn);
	wait.wait.private = current;
	wait.bio = bio;
	wait.abs_cost = abs_cost;
	wait.committed = false;	 

	__add_wait_queue_entry_tail(&iocg->waitq, &wait.wait);
	iocg_kick_waitq(iocg, ioc_locked, &now);

	iocg_unlock(iocg, ioc_locked, &flags);

	while (true) {
		set_current_state(TASK_UNINTERRUPTIBLE);
		if (wait.committed)
			break;
		io_schedule();
	}

	 
	finish_wait(&iocg->waitq, &wait.wait);
}

static void ioc_rqos_merge(struct rq_qos *rqos, struct request *rq,
			   struct bio *bio)
{
	struct ioc_gq *iocg = blkg_to_iocg(bio->bi_blkg);
	struct ioc *ioc = rqos_to_ioc(rqos);
	sector_t bio_end = bio_end_sector(bio);
	struct ioc_now now;
	u64 vtime, abs_cost, cost;
	unsigned long flags;

	 
	if (!ioc->enabled || !iocg || !iocg->level)
		return;

	abs_cost = calc_vtime_cost(bio, iocg, true);
	if (!abs_cost)
		return;

	ioc_now(ioc, &now);

	vtime = atomic64_read(&iocg->vtime);
	cost = adjust_inuse_and_calc_cost(iocg, vtime, abs_cost, &now);

	 
	if (blk_rq_pos(rq) < bio_end &&
	    blk_rq_pos(rq) + blk_rq_sectors(rq) == iocg->cursor)
		iocg->cursor = bio_end;

	 
	if (rq->bio && rq->bio->bi_iocost_cost &&
	    time_before_eq64(atomic64_read(&iocg->vtime) + cost, now.vnow)) {
		iocg_commit_bio(iocg, bio, abs_cost, cost);
		return;
	}

	 
	spin_lock_irqsave(&ioc->lock, flags);
	spin_lock(&iocg->waitq.lock);

	if (likely(!list_empty(&iocg->active_list))) {
		iocg_incur_debt(iocg, abs_cost, &now);
		if (iocg_kick_delay(iocg, &now))
			blkcg_schedule_throttle(rqos->disk,
					(bio->bi_opf & REQ_SWAP) == REQ_SWAP);
	} else {
		iocg_commit_bio(iocg, bio, abs_cost, cost);
	}

	spin_unlock(&iocg->waitq.lock);
	spin_unlock_irqrestore(&ioc->lock, flags);
}

static void ioc_rqos_done_bio(struct rq_qos *rqos, struct bio *bio)
{
	struct ioc_gq *iocg = blkg_to_iocg(bio->bi_blkg);

	if (iocg && bio->bi_iocost_cost)
		atomic64_add(bio->bi_iocost_cost, &iocg->done_vtime);
}

static void ioc_rqos_done(struct rq_qos *rqos, struct request *rq)
{
	struct ioc *ioc = rqos_to_ioc(rqos);
	struct ioc_pcpu_stat *ccs;
	u64 on_q_ns, rq_wait_ns, size_nsec;
	int pidx, rw;

	if (!ioc->enabled || !rq->alloc_time_ns || !rq->start_time_ns)
		return;

	switch (req_op(rq)) {
	case REQ_OP_READ:
		pidx = QOS_RLAT;
		rw = READ;
		break;
	case REQ_OP_WRITE:
		pidx = QOS_WLAT;
		rw = WRITE;
		break;
	default:
		return;
	}

	on_q_ns = ktime_get_ns() - rq->alloc_time_ns;
	rq_wait_ns = rq->start_time_ns - rq->alloc_time_ns;
	size_nsec = div64_u64(calc_size_vtime_cost(rq, ioc), VTIME_PER_NSEC);

	ccs = get_cpu_ptr(ioc->pcpu_stat);

	if (on_q_ns <= size_nsec ||
	    on_q_ns - size_nsec <= ioc->params.qos[pidx] * NSEC_PER_USEC)
		local_inc(&ccs->missed[rw].nr_met);
	else
		local_inc(&ccs->missed[rw].nr_missed);

	local64_add(rq_wait_ns, &ccs->rq_wait_ns);

	put_cpu_ptr(ccs);
}

static void ioc_rqos_queue_depth_changed(struct rq_qos *rqos)
{
	struct ioc *ioc = rqos_to_ioc(rqos);

	spin_lock_irq(&ioc->lock);
	ioc_refresh_params(ioc, false);
	spin_unlock_irq(&ioc->lock);
}

static void ioc_rqos_exit(struct rq_qos *rqos)
{
	struct ioc *ioc = rqos_to_ioc(rqos);

	blkcg_deactivate_policy(rqos->disk, &blkcg_policy_iocost);

	spin_lock_irq(&ioc->lock);
	ioc->running = IOC_STOP;
	spin_unlock_irq(&ioc->lock);

	timer_shutdown_sync(&ioc->timer);
	free_percpu(ioc->pcpu_stat);
	kfree(ioc);
}

static const struct rq_qos_ops ioc_rqos_ops = {
	.throttle = ioc_rqos_throttle,
	.merge = ioc_rqos_merge,
	.done_bio = ioc_rqos_done_bio,
	.done = ioc_rqos_done,
	.queue_depth_changed = ioc_rqos_queue_depth_changed,
	.exit = ioc_rqos_exit,
};

static int blk_iocost_init(struct gendisk *disk)
{
	struct ioc *ioc;
	int i, cpu, ret;

	ioc = kzalloc(sizeof(*ioc), GFP_KERNEL);
	if (!ioc)
		return -ENOMEM;

	ioc->pcpu_stat = alloc_percpu(struct ioc_pcpu_stat);
	if (!ioc->pcpu_stat) {
		kfree(ioc);
		return -ENOMEM;
	}

	for_each_possible_cpu(cpu) {
		struct ioc_pcpu_stat *ccs = per_cpu_ptr(ioc->pcpu_stat, cpu);

		for (i = 0; i < ARRAY_SIZE(ccs->missed); i++) {
			local_set(&ccs->missed[i].nr_met, 0);
			local_set(&ccs->missed[i].nr_missed, 0);
		}
		local64_set(&ccs->rq_wait_ns, 0);
	}

	spin_lock_init(&ioc->lock);
	timer_setup(&ioc->timer, ioc_timer_fn, 0);
	INIT_LIST_HEAD(&ioc->active_iocgs);

	ioc->running = IOC_IDLE;
	ioc->vtime_base_rate = VTIME_PER_USEC;
	atomic64_set(&ioc->vtime_rate, VTIME_PER_USEC);
	seqcount_spinlock_init(&ioc->period_seqcount, &ioc->lock);
	ioc->period_at = ktime_to_us(ktime_get());
	atomic64_set(&ioc->cur_period, 0);
	atomic_set(&ioc->hweight_gen, 0);

	spin_lock_irq(&ioc->lock);
	ioc->autop_idx = AUTOP_INVALID;
	ioc_refresh_params_disk(ioc, true, disk);
	spin_unlock_irq(&ioc->lock);

	 
	ret = rq_qos_add(&ioc->rqos, disk, RQ_QOS_COST, &ioc_rqos_ops);
	if (ret)
		goto err_free_ioc;

	ret = blkcg_activate_policy(disk, &blkcg_policy_iocost);
	if (ret)
		goto err_del_qos;
	return 0;

err_del_qos:
	rq_qos_del(&ioc->rqos);
err_free_ioc:
	free_percpu(ioc->pcpu_stat);
	kfree(ioc);
	return ret;
}

static struct blkcg_policy_data *ioc_cpd_alloc(gfp_t gfp)
{
	struct ioc_cgrp *iocc;

	iocc = kzalloc(sizeof(struct ioc_cgrp), gfp);
	if (!iocc)
		return NULL;

	iocc->dfl_weight = CGROUP_WEIGHT_DFL * WEIGHT_ONE;
	return &iocc->cpd;
}

static void ioc_cpd_free(struct blkcg_policy_data *cpd)
{
	kfree(container_of(cpd, struct ioc_cgrp, cpd));
}

static struct blkg_policy_data *ioc_pd_alloc(struct gendisk *disk,
		struct blkcg *blkcg, gfp_t gfp)
{
	int levels = blkcg->css.cgroup->level + 1;
	struct ioc_gq *iocg;

	iocg = kzalloc_node(struct_size(iocg, ancestors, levels), gfp,
			    disk->node_id);
	if (!iocg)
		return NULL;

	iocg->pcpu_stat = alloc_percpu_gfp(struct iocg_pcpu_stat, gfp);
	if (!iocg->pcpu_stat) {
		kfree(iocg);
		return NULL;
	}

	return &iocg->pd;
}

static void ioc_pd_init(struct blkg_policy_data *pd)
{
	struct ioc_gq *iocg = pd_to_iocg(pd);
	struct blkcg_gq *blkg = pd_to_blkg(&iocg->pd);
	struct ioc *ioc = q_to_ioc(blkg->q);
	struct ioc_now now;
	struct blkcg_gq *tblkg;
	unsigned long flags;

	ioc_now(ioc, &now);

	iocg->ioc = ioc;
	atomic64_set(&iocg->vtime, now.vnow);
	atomic64_set(&iocg->done_vtime, now.vnow);
	atomic64_set(&iocg->active_period, atomic64_read(&ioc->cur_period));
	INIT_LIST_HEAD(&iocg->active_list);
	INIT_LIST_HEAD(&iocg->walk_list);
	INIT_LIST_HEAD(&iocg->surplus_list);
	iocg->hweight_active = WEIGHT_ONE;
	iocg->hweight_inuse = WEIGHT_ONE;

	init_waitqueue_head(&iocg->waitq);
	hrtimer_init(&iocg->waitq_timer, CLOCK_MONOTONIC, HRTIMER_MODE_ABS);
	iocg->waitq_timer.function = iocg_waitq_timer_fn;

	iocg->level = blkg->blkcg->css.cgroup->level;

	for (tblkg = blkg; tblkg; tblkg = tblkg->parent) {
		struct ioc_gq *tiocg = blkg_to_iocg(tblkg);
		iocg->ancestors[tiocg->level] = tiocg;
	}

	spin_lock_irqsave(&ioc->lock, flags);
	weight_updated(iocg, &now);
	spin_unlock_irqrestore(&ioc->lock, flags);
}

static void ioc_pd_free(struct blkg_policy_data *pd)
{
	struct ioc_gq *iocg = pd_to_iocg(pd);
	struct ioc *ioc = iocg->ioc;
	unsigned long flags;

	if (ioc) {
		spin_lock_irqsave(&ioc->lock, flags);

		if (!list_empty(&iocg->active_list)) {
			struct ioc_now now;

			ioc_now(ioc, &now);
			propagate_weights(iocg, 0, 0, false, &now);
			list_del_init(&iocg->active_list);
		}

		WARN_ON_ONCE(!list_empty(&iocg->walk_list));
		WARN_ON_ONCE(!list_empty(&iocg->surplus_list));

		spin_unlock_irqrestore(&ioc->lock, flags);

		hrtimer_cancel(&iocg->waitq_timer);
	}
	free_percpu(iocg->pcpu_stat);
	kfree(iocg);
}

static void ioc_pd_stat(struct blkg_policy_data *pd, struct seq_file *s)
{
	struct ioc_gq *iocg = pd_to_iocg(pd);
	struct ioc *ioc = iocg->ioc;

	if (!ioc->enabled)
		return;

	if (iocg->level == 0) {
		unsigned vp10k = DIV64_U64_ROUND_CLOSEST(
			ioc->vtime_base_rate * 10000,
			VTIME_PER_USEC);
		seq_printf(s, " cost.vrate=%u.%02u", vp10k / 100, vp10k % 100);
	}

	seq_printf(s, " cost.usage=%llu", iocg->last_stat.usage_us);

	if (blkcg_debug_stats)
		seq_printf(s, " cost.wait=%llu cost.indebt=%llu cost.indelay=%llu",
			iocg->last_stat.wait_us,
			iocg->last_stat.indebt_us,
			iocg->last_stat.indelay_us);
}

static u64 ioc_weight_prfill(struct seq_file *sf, struct blkg_policy_data *pd,
			     int off)
{
	const char *dname = blkg_dev_name(pd->blkg);
	struct ioc_gq *iocg = pd_to_iocg(pd);

	if (dname && iocg->cfg_weight)
		seq_printf(sf, "%s %u\n", dname, iocg->cfg_weight / WEIGHT_ONE);
	return 0;
}


static int ioc_weight_show(struct seq_file *sf, void *v)
{
	struct blkcg *blkcg = css_to_blkcg(seq_css(sf));
	struct ioc_cgrp *iocc = blkcg_to_iocc(blkcg);

	seq_printf(sf, "default %u\n", iocc->dfl_weight / WEIGHT_ONE);
	blkcg_print_blkgs(sf, blkcg, ioc_weight_prfill,
			  &blkcg_policy_iocost, seq_cft(sf)->private, false);
	return 0;
}

static ssize_t ioc_weight_write(struct kernfs_open_file *of, char *buf,
				size_t nbytes, loff_t off)
{
	struct blkcg *blkcg = css_to_blkcg(of_css(of));
	struct ioc_cgrp *iocc = blkcg_to_iocc(blkcg);
	struct blkg_conf_ctx ctx;
	struct ioc_now now;
	struct ioc_gq *iocg;
	u32 v;
	int ret;

	if (!strchr(buf, ':')) {
		struct blkcg_gq *blkg;

		if (!sscanf(buf, "default %u", &v) && !sscanf(buf, "%u", &v))
			return -EINVAL;

		if (v < CGROUP_WEIGHT_MIN || v > CGROUP_WEIGHT_MAX)
			return -EINVAL;

		spin_lock_irq(&blkcg->lock);
		iocc->dfl_weight = v * WEIGHT_ONE;
		hlist_for_each_entry(blkg, &blkcg->blkg_list, blkcg_node) {
			struct ioc_gq *iocg = blkg_to_iocg(blkg);

			if (iocg) {
				spin_lock(&iocg->ioc->lock);
				ioc_now(iocg->ioc, &now);
				weight_updated(iocg, &now);
				spin_unlock(&iocg->ioc->lock);
			}
		}
		spin_unlock_irq(&blkcg->lock);

		return nbytes;
	}

	blkg_conf_init(&ctx, buf);

	ret = blkg_conf_prep(blkcg, &blkcg_policy_iocost, &ctx);
	if (ret)
		goto err;

	iocg = blkg_to_iocg(ctx.blkg);

	if (!strncmp(ctx.body, "default", 7)) {
		v = 0;
	} else {
		if (!sscanf(ctx.body, "%u", &v))
			goto einval;
		if (v < CGROUP_WEIGHT_MIN || v > CGROUP_WEIGHT_MAX)
			goto einval;
	}

	spin_lock(&iocg->ioc->lock);
	iocg->cfg_weight = v * WEIGHT_ONE;
	ioc_now(iocg->ioc, &now);
	weight_updated(iocg, &now);
	spin_unlock(&iocg->ioc->lock);

	blkg_conf_exit(&ctx);
	return nbytes;

einval:
	ret = -EINVAL;
err:
	blkg_conf_exit(&ctx);
	return ret;
}

static u64 ioc_qos_prfill(struct seq_file *sf, struct blkg_policy_data *pd,
			  int off)
{
	const char *dname = blkg_dev_name(pd->blkg);
	struct ioc *ioc = pd_to_iocg(pd)->ioc;

	if (!dname)
		return 0;

	spin_lock_irq(&ioc->lock);
	seq_printf(sf, "%s enable=%d ctrl=%s rpct=%u.%02u rlat=%u wpct=%u.%02u wlat=%u min=%u.%02u max=%u.%02u\n",
		   dname, ioc->enabled, ioc->user_qos_params ? "user" : "auto",
		   ioc->params.qos[QOS_RPPM] / 10000,
		   ioc->params.qos[QOS_RPPM] % 10000 / 100,
		   ioc->params.qos[QOS_RLAT],
		   ioc->params.qos[QOS_WPPM] / 10000,
		   ioc->params.qos[QOS_WPPM] % 10000 / 100,
		   ioc->params.qos[QOS_WLAT],
		   ioc->params.qos[QOS_MIN] / 10000,
		   ioc->params.qos[QOS_MIN] % 10000 / 100,
		   ioc->params.qos[QOS_MAX] / 10000,
		   ioc->params.qos[QOS_MAX] % 10000 / 100);
	spin_unlock_irq(&ioc->lock);
	return 0;
}

static int ioc_qos_show(struct seq_file *sf, void *v)
{
	struct blkcg *blkcg = css_to_blkcg(seq_css(sf));

	blkcg_print_blkgs(sf, blkcg, ioc_qos_prfill,
			  &blkcg_policy_iocost, seq_cft(sf)->private, false);
	return 0;
}

static const match_table_t qos_ctrl_tokens = {
	{ QOS_ENABLE,		"enable=%u"	},
	{ QOS_CTRL,		"ctrl=%s"	},
	{ NR_QOS_CTRL_PARAMS,	NULL		},
};

static const match_table_t qos_tokens = {
	{ QOS_RPPM,		"rpct=%s"	},
	{ QOS_RLAT,		"rlat=%u"	},
	{ QOS_WPPM,		"wpct=%s"	},
	{ QOS_WLAT,		"wlat=%u"	},
	{ QOS_MIN,		"min=%s"	},
	{ QOS_MAX,		"max=%s"	},
	{ NR_QOS_PARAMS,	NULL		},
};

static ssize_t ioc_qos_write(struct kernfs_open_file *of, char *input,
			     size_t nbytes, loff_t off)
{
	struct blkg_conf_ctx ctx;
	struct gendisk *disk;
	struct ioc *ioc;
	u32 qos[NR_QOS_PARAMS];
	bool enable, user;
	char *body, *p;
	int ret;

	blkg_conf_init(&ctx, input);

	ret = blkg_conf_open_bdev(&ctx);
	if (ret)
		goto err;

	body = ctx.body;
	disk = ctx.bdev->bd_disk;
	if (!queue_is_mq(disk->queue)) {
		ret = -EOPNOTSUPP;
		goto err;
	}

	ioc = q_to_ioc(disk->queue);
	if (!ioc) {
		ret = blk_iocost_init(disk);
		if (ret)
			goto err;
		ioc = q_to_ioc(disk->queue);
	}

	blk_mq_freeze_queue(disk->queue);
	blk_mq_quiesce_queue(disk->queue);

	spin_lock_irq(&ioc->lock);
	memcpy(qos, ioc->params.qos, sizeof(qos));
	enable = ioc->enabled;
	user = ioc->user_qos_params;

	while ((p = strsep(&body, " \t\n"))) {
		substring_t args[MAX_OPT_ARGS];
		char buf[32];
		int tok;
		s64 v;

		if (!*p)
			continue;

		switch (match_token(p, qos_ctrl_tokens, args)) {
		case QOS_ENABLE:
			if (match_u64(&args[0], &v))
				goto einval;
			enable = v;
			continue;
		case QOS_CTRL:
			match_strlcpy(buf, &args[0], sizeof(buf));
			if (!strcmp(buf, "auto"))
				user = false;
			else if (!strcmp(buf, "user"))
				user = true;
			else
				goto einval;
			continue;
		}

		tok = match_token(p, qos_tokens, args);
		switch (tok) {
		case QOS_RPPM:
		case QOS_WPPM:
			if (match_strlcpy(buf, &args[0], sizeof(buf)) >=
			    sizeof(buf))
				goto einval;
			if (cgroup_parse_float(buf, 2, &v))
				goto einval;
			if (v < 0 || v > 10000)
				goto einval;
			qos[tok] = v * 100;
			break;
		case QOS_RLAT:
		case QOS_WLAT:
			if (match_u64(&args[0], &v))
				goto einval;
			qos[tok] = v;
			break;
		case QOS_MIN:
		case QOS_MAX:
			if (match_strlcpy(buf, &args[0], sizeof(buf)) >=
			    sizeof(buf))
				goto einval;
			if (cgroup_parse_float(buf, 2, &v))
				goto einval;
			if (v < 0)
				goto einval;
			qos[tok] = clamp_t(s64, v * 100,
					   VRATE_MIN_PPM, VRATE_MAX_PPM);
			break;
		default:
			goto einval;
		}
		user = true;
	}

	if (qos[QOS_MIN] > qos[QOS_MAX])
		goto einval;

	if (enable && !ioc->enabled) {
		blk_stat_enable_accounting(disk->queue);
		blk_queue_flag_set(QUEUE_FLAG_RQ_ALLOC_TIME, disk->queue);
		ioc->enabled = true;
	} else if (!enable && ioc->enabled) {
		blk_stat_disable_accounting(disk->queue);
		blk_queue_flag_clear(QUEUE_FLAG_RQ_ALLOC_TIME, disk->queue);
		ioc->enabled = false;
	}

	if (user) {
		memcpy(ioc->params.qos, qos, sizeof(qos));
		ioc->user_qos_params = true;
	} else {
		ioc->user_qos_params = false;
	}

	ioc_refresh_params(ioc, true);
	spin_unlock_irq(&ioc->lock);

	if (enable)
		wbt_disable_default(disk);
	else
		wbt_enable_default(disk);

	blk_mq_unquiesce_queue(disk->queue);
	blk_mq_unfreeze_queue(disk->queue);

	blkg_conf_exit(&ctx);
	return nbytes;
einval:
	spin_unlock_irq(&ioc->lock);

	blk_mq_unquiesce_queue(disk->queue);
	blk_mq_unfreeze_queue(disk->queue);

	ret = -EINVAL;
err:
	blkg_conf_exit(&ctx);
	return ret;
}

static u64 ioc_cost_model_prfill(struct seq_file *sf,
				 struct blkg_policy_data *pd, int off)
{
	const char *dname = blkg_dev_name(pd->blkg);
	struct ioc *ioc = pd_to_iocg(pd)->ioc;
	u64 *u = ioc->params.i_lcoefs;

	if (!dname)
		return 0;

	spin_lock_irq(&ioc->lock);
	seq_printf(sf, "%s ctrl=%s model=linear "
		   "rbps=%llu rseqiops=%llu rrandiops=%llu "
		   "wbps=%llu wseqiops=%llu wrandiops=%llu\n",
		   dname, ioc->user_cost_model ? "user" : "auto",
		   u[I_LCOEF_RBPS], u[I_LCOEF_RSEQIOPS], u[I_LCOEF_RRANDIOPS],
		   u[I_LCOEF_WBPS], u[I_LCOEF_WSEQIOPS], u[I_LCOEF_WRANDIOPS]);
	spin_unlock_irq(&ioc->lock);
	return 0;
}

static int ioc_cost_model_show(struct seq_file *sf, void *v)
{
	struct blkcg *blkcg = css_to_blkcg(seq_css(sf));

	blkcg_print_blkgs(sf, blkcg, ioc_cost_model_prfill,
			  &blkcg_policy_iocost, seq_cft(sf)->private, false);
	return 0;
}

static const match_table_t cost_ctrl_tokens = {
	{ COST_CTRL,		"ctrl=%s"	},
	{ COST_MODEL,		"model=%s"	},
	{ NR_COST_CTRL_PARAMS,	NULL		},
};

static const match_table_t i_lcoef_tokens = {
	{ I_LCOEF_RBPS,		"rbps=%u"	},
	{ I_LCOEF_RSEQIOPS,	"rseqiops=%u"	},
	{ I_LCOEF_RRANDIOPS,	"rrandiops=%u"	},
	{ I_LCOEF_WBPS,		"wbps=%u"	},
	{ I_LCOEF_WSEQIOPS,	"wseqiops=%u"	},
	{ I_LCOEF_WRANDIOPS,	"wrandiops=%u"	},
	{ NR_I_LCOEFS,		NULL		},
};

static ssize_t ioc_cost_model_write(struct kernfs_open_file *of, char *input,
				    size_t nbytes, loff_t off)
{
	struct blkg_conf_ctx ctx;
	struct request_queue *q;
	struct ioc *ioc;
	u64 u[NR_I_LCOEFS];
	bool user;
	char *body, *p;
	int ret;

	blkg_conf_init(&ctx, input);

	ret = blkg_conf_open_bdev(&ctx);
	if (ret)
		goto err;

	body = ctx.body;
	q = bdev_get_queue(ctx.bdev);
	if (!queue_is_mq(q)) {
		ret = -EOPNOTSUPP;
		goto err;
	}

	ioc = q_to_ioc(q);
	if (!ioc) {
		ret = blk_iocost_init(ctx.bdev->bd_disk);
		if (ret)
			goto err;
		ioc = q_to_ioc(q);
	}

	blk_mq_freeze_queue(q);
	blk_mq_quiesce_queue(q);

	spin_lock_irq(&ioc->lock);
	memcpy(u, ioc->params.i_lcoefs, sizeof(u));
	user = ioc->user_cost_model;

	while ((p = strsep(&body, " \t\n"))) {
		substring_t args[MAX_OPT_ARGS];
		char buf[32];
		int tok;
		u64 v;

		if (!*p)
			continue;

		switch (match_token(p, cost_ctrl_tokens, args)) {
		case COST_CTRL:
			match_strlcpy(buf, &args[0], sizeof(buf));
			if (!strcmp(buf, "auto"))
				user = false;
			else if (!strcmp(buf, "user"))
				user = true;
			else
				goto einval;
			continue;
		case COST_MODEL:
			match_strlcpy(buf, &args[0], sizeof(buf));
			if (strcmp(buf, "linear"))
				goto einval;
			continue;
		}

		tok = match_token(p, i_lcoef_tokens, args);
		if (tok == NR_I_LCOEFS)
			goto einval;
		if (match_u64(&args[0], &v))
			goto einval;
		u[tok] = v;
		user = true;
	}

	if (user) {
		memcpy(ioc->params.i_lcoefs, u, sizeof(u));
		ioc->user_cost_model = true;
	} else {
		ioc->user_cost_model = false;
	}
	ioc_refresh_params(ioc, true);
	spin_unlock_irq(&ioc->lock);

	blk_mq_unquiesce_queue(q);
	blk_mq_unfreeze_queue(q);

	blkg_conf_exit(&ctx);
	return nbytes;

einval:
	spin_unlock_irq(&ioc->lock);

	blk_mq_unquiesce_queue(q);
	blk_mq_unfreeze_queue(q);

	ret = -EINVAL;
err:
	blkg_conf_exit(&ctx);
	return ret;
}

static struct cftype ioc_files[] = {
	{
		.name = "weight",
		.flags = CFTYPE_NOT_ON_ROOT,
		.seq_show = ioc_weight_show,
		.write = ioc_weight_write,
	},
	{
		.name = "cost.qos",
		.flags = CFTYPE_ONLY_ON_ROOT,
		.seq_show = ioc_qos_show,
		.write = ioc_qos_write,
	},
	{
		.name = "cost.model",
		.flags = CFTYPE_ONLY_ON_ROOT,
		.seq_show = ioc_cost_model_show,
		.write = ioc_cost_model_write,
	},
	{}
};

static struct blkcg_policy blkcg_policy_iocost = {
	.dfl_cftypes	= ioc_files,
	.cpd_alloc_fn	= ioc_cpd_alloc,
	.cpd_free_fn	= ioc_cpd_free,
	.pd_alloc_fn	= ioc_pd_alloc,
	.pd_init_fn	= ioc_pd_init,
	.pd_free_fn	= ioc_pd_free,
	.pd_stat_fn	= ioc_pd_stat,
};

static int __init ioc_init(void)
{
	return blkcg_policy_register(&blkcg_policy_iocost);
}

static void __exit ioc_exit(void)
{
	blkcg_policy_unregister(&blkcg_policy_iocost);
}

module_init(ioc_init);
module_exit(ioc_exit);
