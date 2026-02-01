
 

#include <linux/kernel.h>
#include <linux/math64.h>
#include <linux/export.h>
#include <linux/spinlock.h>
#include <linux/fs.h>
#include <linux/mm.h>
#include <linux/swap.h>
#include <linux/slab.h>
#include <linux/pagemap.h>
#include <linux/writeback.h>
#include <linux/init.h>
#include <linux/backing-dev.h>
#include <linux/task_io_accounting_ops.h>
#include <linux/blkdev.h>
#include <linux/mpage.h>
#include <linux/rmap.h>
#include <linux/percpu.h>
#include <linux/smp.h>
#include <linux/sysctl.h>
#include <linux/cpu.h>
#include <linux/syscalls.h>
#include <linux/pagevec.h>
#include <linux/timer.h>
#include <linux/sched/rt.h>
#include <linux/sched/signal.h>
#include <linux/mm_inline.h>
#include <trace/events/writeback.h>

#include "internal.h"

 
#define MAX_PAUSE		max(HZ/5, 1)

 
#define DIRTY_POLL_THRESH	(128 >> (PAGE_SHIFT - 10))

 
#define BANDWIDTH_INTERVAL	max(HZ/5, 1)

#define RATELIMIT_CALC_SHIFT	10

 
static long ratelimit_pages = 32;

 

 
static int dirty_background_ratio = 10;

 
static unsigned long dirty_background_bytes;

 
static int vm_highmem_is_dirtyable;

 
static int vm_dirty_ratio = 20;

 
static unsigned long vm_dirty_bytes;

 
unsigned int dirty_writeback_interval = 5 * 100;  

EXPORT_SYMBOL_GPL(dirty_writeback_interval);

 
unsigned int dirty_expire_interval = 30 * 100;  

 
int laptop_mode;

EXPORT_SYMBOL(laptop_mode);

 

struct wb_domain global_wb_domain;

 
struct dirty_throttle_control {
#ifdef CONFIG_CGROUP_WRITEBACK
	struct wb_domain	*dom;
	struct dirty_throttle_control *gdtc;	 
#endif
	struct bdi_writeback	*wb;
	struct fprop_local_percpu *wb_completions;

	unsigned long		avail;		 
	unsigned long		dirty;		 
	unsigned long		thresh;		 
	unsigned long		bg_thresh;	 

	unsigned long		wb_dirty;	 
	unsigned long		wb_thresh;
	unsigned long		wb_bg_thresh;

	unsigned long		pos_ratio;
};

 
#define VM_COMPLETIONS_PERIOD_LEN (3*HZ)

#ifdef CONFIG_CGROUP_WRITEBACK

#define GDTC_INIT(__wb)		.wb = (__wb),				\
				.dom = &global_wb_domain,		\
				.wb_completions = &(__wb)->completions

#define GDTC_INIT_NO_WB		.dom = &global_wb_domain

#define MDTC_INIT(__wb, __gdtc)	.wb = (__wb),				\
				.dom = mem_cgroup_wb_domain(__wb),	\
				.wb_completions = &(__wb)->memcg_completions, \
				.gdtc = __gdtc

static bool mdtc_valid(struct dirty_throttle_control *dtc)
{
	return dtc->dom;
}

static struct wb_domain *dtc_dom(struct dirty_throttle_control *dtc)
{
	return dtc->dom;
}

static struct dirty_throttle_control *mdtc_gdtc(struct dirty_throttle_control *mdtc)
{
	return mdtc->gdtc;
}

static struct fprop_local_percpu *wb_memcg_completions(struct bdi_writeback *wb)
{
	return &wb->memcg_completions;
}

static void wb_min_max_ratio(struct bdi_writeback *wb,
			     unsigned long *minp, unsigned long *maxp)
{
	unsigned long this_bw = READ_ONCE(wb->avg_write_bandwidth);
	unsigned long tot_bw = atomic_long_read(&wb->bdi->tot_write_bandwidth);
	unsigned long long min = wb->bdi->min_ratio;
	unsigned long long max = wb->bdi->max_ratio;

	 
	if (this_bw < tot_bw) {
		if (min) {
			min *= this_bw;
			min = div64_ul(min, tot_bw);
		}
		if (max < 100 * BDI_RATIO_SCALE) {
			max *= this_bw;
			max = div64_ul(max, tot_bw);
		}
	}

	*minp = min;
	*maxp = max;
}

#else	 

#define GDTC_INIT(__wb)		.wb = (__wb),                           \
				.wb_completions = &(__wb)->completions
#define GDTC_INIT_NO_WB
#define MDTC_INIT(__wb, __gdtc)

static bool mdtc_valid(struct dirty_throttle_control *dtc)
{
	return false;
}

static struct wb_domain *dtc_dom(struct dirty_throttle_control *dtc)
{
	return &global_wb_domain;
}

static struct dirty_throttle_control *mdtc_gdtc(struct dirty_throttle_control *mdtc)
{
	return NULL;
}

static struct fprop_local_percpu *wb_memcg_completions(struct bdi_writeback *wb)
{
	return NULL;
}

static void wb_min_max_ratio(struct bdi_writeback *wb,
			     unsigned long *minp, unsigned long *maxp)
{
	*minp = wb->bdi->min_ratio;
	*maxp = wb->bdi->max_ratio;
}

#endif	 

 

 
static unsigned long node_dirtyable_memory(struct pglist_data *pgdat)
{
	unsigned long nr_pages = 0;
	int z;

	for (z = 0; z < MAX_NR_ZONES; z++) {
		struct zone *zone = pgdat->node_zones + z;

		if (!populated_zone(zone))
			continue;

		nr_pages += zone_page_state(zone, NR_FREE_PAGES);
	}

	 
	nr_pages -= min(nr_pages, pgdat->totalreserve_pages);

	nr_pages += node_page_state(pgdat, NR_INACTIVE_FILE);
	nr_pages += node_page_state(pgdat, NR_ACTIVE_FILE);

	return nr_pages;
}

static unsigned long highmem_dirtyable_memory(unsigned long total)
{
#ifdef CONFIG_HIGHMEM
	int node;
	unsigned long x = 0;
	int i;

	for_each_node_state(node, N_HIGH_MEMORY) {
		for (i = ZONE_NORMAL + 1; i < MAX_NR_ZONES; i++) {
			struct zone *z;
			unsigned long nr_pages;

			if (!is_highmem_idx(i))
				continue;

			z = &NODE_DATA(node)->node_zones[i];
			if (!populated_zone(z))
				continue;

			nr_pages = zone_page_state(z, NR_FREE_PAGES);
			 
			nr_pages -= min(nr_pages, high_wmark_pages(z));
			nr_pages += zone_page_state(z, NR_ZONE_INACTIVE_FILE);
			nr_pages += zone_page_state(z, NR_ZONE_ACTIVE_FILE);
			x += nr_pages;
		}
	}

	 
	return min(x, total);
#else
	return 0;
#endif
}

 
static unsigned long global_dirtyable_memory(void)
{
	unsigned long x;

	x = global_zone_page_state(NR_FREE_PAGES);
	 
	x -= min(x, totalreserve_pages);

	x += global_node_page_state(NR_INACTIVE_FILE);
	x += global_node_page_state(NR_ACTIVE_FILE);

	if (!vm_highmem_is_dirtyable)
		x -= highmem_dirtyable_memory(x);

	return x + 1;	 
}

 
static void domain_dirty_limits(struct dirty_throttle_control *dtc)
{
	const unsigned long available_memory = dtc->avail;
	struct dirty_throttle_control *gdtc = mdtc_gdtc(dtc);
	unsigned long bytes = vm_dirty_bytes;
	unsigned long bg_bytes = dirty_background_bytes;
	 
	unsigned long ratio = (vm_dirty_ratio * PAGE_SIZE) / 100;
	unsigned long bg_ratio = (dirty_background_ratio * PAGE_SIZE) / 100;
	unsigned long thresh;
	unsigned long bg_thresh;
	struct task_struct *tsk;

	 
	if (gdtc) {
		unsigned long global_avail = gdtc->avail;

		 
		if (bytes)
			ratio = min(DIV_ROUND_UP(bytes, global_avail),
				    PAGE_SIZE);
		if (bg_bytes)
			bg_ratio = min(DIV_ROUND_UP(bg_bytes, global_avail),
				       PAGE_SIZE);
		bytes = bg_bytes = 0;
	}

	if (bytes)
		thresh = DIV_ROUND_UP(bytes, PAGE_SIZE);
	else
		thresh = (ratio * available_memory) / PAGE_SIZE;

	if (bg_bytes)
		bg_thresh = DIV_ROUND_UP(bg_bytes, PAGE_SIZE);
	else
		bg_thresh = (bg_ratio * available_memory) / PAGE_SIZE;

	if (bg_thresh >= thresh)
		bg_thresh = thresh / 2;
	tsk = current;
	if (rt_task(tsk)) {
		bg_thresh += bg_thresh / 4 + global_wb_domain.dirty_limit / 32;
		thresh += thresh / 4 + global_wb_domain.dirty_limit / 32;
	}
	dtc->thresh = thresh;
	dtc->bg_thresh = bg_thresh;

	 
	if (!gdtc)
		trace_global_dirty_state(bg_thresh, thresh);
}

 
void global_dirty_limits(unsigned long *pbackground, unsigned long *pdirty)
{
	struct dirty_throttle_control gdtc = { GDTC_INIT_NO_WB };

	gdtc.avail = global_dirtyable_memory();
	domain_dirty_limits(&gdtc);

	*pbackground = gdtc.bg_thresh;
	*pdirty = gdtc.thresh;
}

 
static unsigned long node_dirty_limit(struct pglist_data *pgdat)
{
	unsigned long node_memory = node_dirtyable_memory(pgdat);
	struct task_struct *tsk = current;
	unsigned long dirty;

	if (vm_dirty_bytes)
		dirty = DIV_ROUND_UP(vm_dirty_bytes, PAGE_SIZE) *
			node_memory / global_dirtyable_memory();
	else
		dirty = vm_dirty_ratio * node_memory / 100;

	if (rt_task(tsk))
		dirty += dirty / 4;

	return dirty;
}

 
bool node_dirty_ok(struct pglist_data *pgdat)
{
	unsigned long limit = node_dirty_limit(pgdat);
	unsigned long nr_pages = 0;

	nr_pages += node_page_state(pgdat, NR_FILE_DIRTY);
	nr_pages += node_page_state(pgdat, NR_WRITEBACK);

	return nr_pages <= limit;
}

#ifdef CONFIG_SYSCTL
static int dirty_background_ratio_handler(struct ctl_table *table, int write,
		void *buffer, size_t *lenp, loff_t *ppos)
{
	int ret;

	ret = proc_dointvec_minmax(table, write, buffer, lenp, ppos);
	if (ret == 0 && write)
		dirty_background_bytes = 0;
	return ret;
}

static int dirty_background_bytes_handler(struct ctl_table *table, int write,
		void *buffer, size_t *lenp, loff_t *ppos)
{
	int ret;

	ret = proc_doulongvec_minmax(table, write, buffer, lenp, ppos);
	if (ret == 0 && write)
		dirty_background_ratio = 0;
	return ret;
}

static int dirty_ratio_handler(struct ctl_table *table, int write, void *buffer,
		size_t *lenp, loff_t *ppos)
{
	int old_ratio = vm_dirty_ratio;
	int ret;

	ret = proc_dointvec_minmax(table, write, buffer, lenp, ppos);
	if (ret == 0 && write && vm_dirty_ratio != old_ratio) {
		writeback_set_ratelimit();
		vm_dirty_bytes = 0;
	}
	return ret;
}

static int dirty_bytes_handler(struct ctl_table *table, int write,
		void *buffer, size_t *lenp, loff_t *ppos)
{
	unsigned long old_bytes = vm_dirty_bytes;
	int ret;

	ret = proc_doulongvec_minmax(table, write, buffer, lenp, ppos);
	if (ret == 0 && write && vm_dirty_bytes != old_bytes) {
		writeback_set_ratelimit();
		vm_dirty_ratio = 0;
	}
	return ret;
}
#endif

static unsigned long wp_next_time(unsigned long cur_time)
{
	cur_time += VM_COMPLETIONS_PERIOD_LEN;
	 
	if (!cur_time)
		return 1;
	return cur_time;
}

static void wb_domain_writeout_add(struct wb_domain *dom,
				   struct fprop_local_percpu *completions,
				   unsigned int max_prop_frac, long nr)
{
	__fprop_add_percpu_max(&dom->completions, completions,
			       max_prop_frac, nr);
	 
	if (unlikely(!dom->period_time)) {
		 
		dom->period_time = wp_next_time(jiffies);
		mod_timer(&dom->period_timer, dom->period_time);
	}
}

 
static inline void __wb_writeout_add(struct bdi_writeback *wb, long nr)
{
	struct wb_domain *cgdom;

	wb_stat_mod(wb, WB_WRITTEN, nr);
	wb_domain_writeout_add(&global_wb_domain, &wb->completions,
			       wb->bdi->max_prop_frac, nr);

	cgdom = mem_cgroup_wb_domain(wb);
	if (cgdom)
		wb_domain_writeout_add(cgdom, wb_memcg_completions(wb),
				       wb->bdi->max_prop_frac, nr);
}

void wb_writeout_inc(struct bdi_writeback *wb)
{
	unsigned long flags;

	local_irq_save(flags);
	__wb_writeout_add(wb, 1);
	local_irq_restore(flags);
}
EXPORT_SYMBOL_GPL(wb_writeout_inc);

 
static void writeout_period(struct timer_list *t)
{
	struct wb_domain *dom = from_timer(dom, t, period_timer);
	int miss_periods = (jiffies - dom->period_time) /
						 VM_COMPLETIONS_PERIOD_LEN;

	if (fprop_new_period(&dom->completions, miss_periods + 1)) {
		dom->period_time = wp_next_time(dom->period_time +
				miss_periods * VM_COMPLETIONS_PERIOD_LEN);
		mod_timer(&dom->period_timer, dom->period_time);
	} else {
		 
		dom->period_time = 0;
	}
}

int wb_domain_init(struct wb_domain *dom, gfp_t gfp)
{
	memset(dom, 0, sizeof(*dom));

	spin_lock_init(&dom->lock);

	timer_setup(&dom->period_timer, writeout_period, TIMER_DEFERRABLE);

	dom->dirty_limit_tstamp = jiffies;

	return fprop_global_init(&dom->completions, gfp);
}

#ifdef CONFIG_CGROUP_WRITEBACK
void wb_domain_exit(struct wb_domain *dom)
{
	del_timer_sync(&dom->period_timer);
	fprop_global_destroy(&dom->completions);
}
#endif

 
static unsigned int bdi_min_ratio;

static int bdi_check_pages_limit(unsigned long pages)
{
	unsigned long max_dirty_pages = global_dirtyable_memory();

	if (pages > max_dirty_pages)
		return -EINVAL;

	return 0;
}

static unsigned long bdi_ratio_from_pages(unsigned long pages)
{
	unsigned long background_thresh;
	unsigned long dirty_thresh;
	unsigned long ratio;

	global_dirty_limits(&background_thresh, &dirty_thresh);
	ratio = div64_u64(pages * 100ULL * BDI_RATIO_SCALE, dirty_thresh);

	return ratio;
}

static u64 bdi_get_bytes(unsigned int ratio)
{
	unsigned long background_thresh;
	unsigned long dirty_thresh;
	u64 bytes;

	global_dirty_limits(&background_thresh, &dirty_thresh);
	bytes = (dirty_thresh * PAGE_SIZE * ratio) / BDI_RATIO_SCALE / 100;

	return bytes;
}

static int __bdi_set_min_ratio(struct backing_dev_info *bdi, unsigned int min_ratio)
{
	unsigned int delta;
	int ret = 0;

	if (min_ratio > 100 * BDI_RATIO_SCALE)
		return -EINVAL;
	min_ratio *= BDI_RATIO_SCALE;

	spin_lock_bh(&bdi_lock);
	if (min_ratio > bdi->max_ratio) {
		ret = -EINVAL;
	} else {
		if (min_ratio < bdi->min_ratio) {
			delta = bdi->min_ratio - min_ratio;
			bdi_min_ratio -= delta;
			bdi->min_ratio = min_ratio;
		} else {
			delta = min_ratio - bdi->min_ratio;
			if (bdi_min_ratio + delta < 100 * BDI_RATIO_SCALE) {
				bdi_min_ratio += delta;
				bdi->min_ratio = min_ratio;
			} else {
				ret = -EINVAL;
			}
		}
	}
	spin_unlock_bh(&bdi_lock);

	return ret;
}

static int __bdi_set_max_ratio(struct backing_dev_info *bdi, unsigned int max_ratio)
{
	int ret = 0;

	if (max_ratio > 100 * BDI_RATIO_SCALE)
		return -EINVAL;

	spin_lock_bh(&bdi_lock);
	if (bdi->min_ratio > max_ratio) {
		ret = -EINVAL;
	} else {
		bdi->max_ratio = max_ratio;
		bdi->max_prop_frac = (FPROP_FRAC_BASE * max_ratio) / 100;
	}
	spin_unlock_bh(&bdi_lock);

	return ret;
}

int bdi_set_min_ratio_no_scale(struct backing_dev_info *bdi, unsigned int min_ratio)
{
	return __bdi_set_min_ratio(bdi, min_ratio);
}

int bdi_set_max_ratio_no_scale(struct backing_dev_info *bdi, unsigned int max_ratio)
{
	return __bdi_set_max_ratio(bdi, max_ratio);
}

int bdi_set_min_ratio(struct backing_dev_info *bdi, unsigned int min_ratio)
{
	return __bdi_set_min_ratio(bdi, min_ratio * BDI_RATIO_SCALE);
}

int bdi_set_max_ratio(struct backing_dev_info *bdi, unsigned int max_ratio)
{
	return __bdi_set_max_ratio(bdi, max_ratio * BDI_RATIO_SCALE);
}
EXPORT_SYMBOL(bdi_set_max_ratio);

u64 bdi_get_min_bytes(struct backing_dev_info *bdi)
{
	return bdi_get_bytes(bdi->min_ratio);
}

int bdi_set_min_bytes(struct backing_dev_info *bdi, u64 min_bytes)
{
	int ret;
	unsigned long pages = min_bytes >> PAGE_SHIFT;
	unsigned long min_ratio;

	ret = bdi_check_pages_limit(pages);
	if (ret)
		return ret;

	min_ratio = bdi_ratio_from_pages(pages);
	return __bdi_set_min_ratio(bdi, min_ratio);
}

u64 bdi_get_max_bytes(struct backing_dev_info *bdi)
{
	return bdi_get_bytes(bdi->max_ratio);
}

int bdi_set_max_bytes(struct backing_dev_info *bdi, u64 max_bytes)
{
	int ret;
	unsigned long pages = max_bytes >> PAGE_SHIFT;
	unsigned long max_ratio;

	ret = bdi_check_pages_limit(pages);
	if (ret)
		return ret;

	max_ratio = bdi_ratio_from_pages(pages);
	return __bdi_set_max_ratio(bdi, max_ratio);
}

int bdi_set_strict_limit(struct backing_dev_info *bdi, unsigned int strict_limit)
{
	if (strict_limit > 1)
		return -EINVAL;

	spin_lock_bh(&bdi_lock);
	if (strict_limit)
		bdi->capabilities |= BDI_CAP_STRICTLIMIT;
	else
		bdi->capabilities &= ~BDI_CAP_STRICTLIMIT;
	spin_unlock_bh(&bdi_lock);

	return 0;
}

static unsigned long dirty_freerun_ceiling(unsigned long thresh,
					   unsigned long bg_thresh)
{
	return (thresh + bg_thresh) / 2;
}

static unsigned long hard_dirty_limit(struct wb_domain *dom,
				      unsigned long thresh)
{
	return max(thresh, dom->dirty_limit);
}

 
static void mdtc_calc_avail(struct dirty_throttle_control *mdtc,
			    unsigned long filepages, unsigned long headroom)
{
	struct dirty_throttle_control *gdtc = mdtc_gdtc(mdtc);
	unsigned long clean = filepages - min(filepages, mdtc->dirty);
	unsigned long global_clean = gdtc->avail - min(gdtc->avail, gdtc->dirty);
	unsigned long other_clean = global_clean - min(global_clean, clean);

	mdtc->avail = filepages + min(headroom, other_clean);
}

 
static unsigned long __wb_calc_thresh(struct dirty_throttle_control *dtc)
{
	struct wb_domain *dom = dtc_dom(dtc);
	unsigned long thresh = dtc->thresh;
	u64 wb_thresh;
	unsigned long numerator, denominator;
	unsigned long wb_min_ratio, wb_max_ratio;

	 
	fprop_fraction_percpu(&dom->completions, dtc->wb_completions,
			      &numerator, &denominator);

	wb_thresh = (thresh * (100 * BDI_RATIO_SCALE - bdi_min_ratio)) / (100 * BDI_RATIO_SCALE);
	wb_thresh *= numerator;
	wb_thresh = div64_ul(wb_thresh, denominator);

	wb_min_max_ratio(dtc->wb, &wb_min_ratio, &wb_max_ratio);

	wb_thresh += (thresh * wb_min_ratio) / (100 * BDI_RATIO_SCALE);
	if (wb_thresh > (thresh * wb_max_ratio) / (100 * BDI_RATIO_SCALE))
		wb_thresh = thresh * wb_max_ratio / (100 * BDI_RATIO_SCALE);

	return wb_thresh;
}

unsigned long wb_calc_thresh(struct bdi_writeback *wb, unsigned long thresh)
{
	struct dirty_throttle_control gdtc = { GDTC_INIT(wb),
					       .thresh = thresh };
	return __wb_calc_thresh(&gdtc);
}

 
static long long pos_ratio_polynom(unsigned long setpoint,
					  unsigned long dirty,
					  unsigned long limit)
{
	long long pos_ratio;
	long x;

	x = div64_s64(((s64)setpoint - (s64)dirty) << RATELIMIT_CALC_SHIFT,
		      (limit - setpoint) | 1);
	pos_ratio = x;
	pos_ratio = pos_ratio * x >> RATELIMIT_CALC_SHIFT;
	pos_ratio = pos_ratio * x >> RATELIMIT_CALC_SHIFT;
	pos_ratio += 1 << RATELIMIT_CALC_SHIFT;

	return clamp(pos_ratio, 0LL, 2LL << RATELIMIT_CALC_SHIFT);
}

 
static void wb_position_ratio(struct dirty_throttle_control *dtc)
{
	struct bdi_writeback *wb = dtc->wb;
	unsigned long write_bw = READ_ONCE(wb->avg_write_bandwidth);
	unsigned long freerun = dirty_freerun_ceiling(dtc->thresh, dtc->bg_thresh);
	unsigned long limit = hard_dirty_limit(dtc_dom(dtc), dtc->thresh);
	unsigned long wb_thresh = dtc->wb_thresh;
	unsigned long x_intercept;
	unsigned long setpoint;		 
	unsigned long wb_setpoint;
	unsigned long span;
	long long pos_ratio;		 
	long x;

	dtc->pos_ratio = 0;

	if (unlikely(dtc->dirty >= limit))
		return;

	 
	setpoint = (freerun + limit) / 2;
	pos_ratio = pos_ratio_polynom(setpoint, dtc->dirty, limit);

	 
	if (unlikely(wb->bdi->capabilities & BDI_CAP_STRICTLIMIT)) {
		long long wb_pos_ratio;

		if (dtc->wb_dirty < 8) {
			dtc->pos_ratio = min_t(long long, pos_ratio * 2,
					   2 << RATELIMIT_CALC_SHIFT);
			return;
		}

		if (dtc->wb_dirty >= wb_thresh)
			return;

		wb_setpoint = dirty_freerun_ceiling(wb_thresh,
						    dtc->wb_bg_thresh);

		if (wb_setpoint == 0 || wb_setpoint == wb_thresh)
			return;

		wb_pos_ratio = pos_ratio_polynom(wb_setpoint, dtc->wb_dirty,
						 wb_thresh);

		 
		dtc->pos_ratio = min(pos_ratio, wb_pos_ratio);
		return;
	}

	 

	 
	if (unlikely(wb_thresh > dtc->thresh))
		wb_thresh = dtc->thresh;
	 
	wb_thresh = max(wb_thresh, (limit - dtc->dirty) / 8);
	 
	x = div_u64((u64)wb_thresh << 16, dtc->thresh | 1);
	wb_setpoint = setpoint * (u64)x >> 16;
	 
	span = (dtc->thresh - wb_thresh + 8 * write_bw) * (u64)x >> 16;
	x_intercept = wb_setpoint + span;

	if (dtc->wb_dirty < x_intercept - span / 4) {
		pos_ratio = div64_u64(pos_ratio * (x_intercept - dtc->wb_dirty),
				      (x_intercept - wb_setpoint) | 1);
	} else
		pos_ratio /= 4;

	 
	x_intercept = wb_thresh / 2;
	if (dtc->wb_dirty < x_intercept) {
		if (dtc->wb_dirty > x_intercept / 8)
			pos_ratio = div_u64(pos_ratio * x_intercept,
					    dtc->wb_dirty);
		else
			pos_ratio *= 8;
	}

	dtc->pos_ratio = pos_ratio;
}

static void wb_update_write_bandwidth(struct bdi_writeback *wb,
				      unsigned long elapsed,
				      unsigned long written)
{
	const unsigned long period = roundup_pow_of_two(3 * HZ);
	unsigned long avg = wb->avg_write_bandwidth;
	unsigned long old = wb->write_bandwidth;
	u64 bw;

	 
	bw = written - min(written, wb->written_stamp);
	bw *= HZ;
	if (unlikely(elapsed > period)) {
		bw = div64_ul(bw, elapsed);
		avg = bw;
		goto out;
	}
	bw += (u64)wb->write_bandwidth * (period - elapsed);
	bw >>= ilog2(period);

	 
	if (avg > old && old >= (unsigned long)bw)
		avg -= (avg - old) >> 3;

	if (avg < old && old <= (unsigned long)bw)
		avg += (old - avg) >> 3;

out:
	 
	avg = max(avg, 1LU);
	if (wb_has_dirty_io(wb)) {
		long delta = avg - wb->avg_write_bandwidth;
		WARN_ON_ONCE(atomic_long_add_return(delta,
					&wb->bdi->tot_write_bandwidth) <= 0);
	}
	wb->write_bandwidth = bw;
	WRITE_ONCE(wb->avg_write_bandwidth, avg);
}

static void update_dirty_limit(struct dirty_throttle_control *dtc)
{
	struct wb_domain *dom = dtc_dom(dtc);
	unsigned long thresh = dtc->thresh;
	unsigned long limit = dom->dirty_limit;

	 
	if (limit < thresh) {
		limit = thresh;
		goto update;
	}

	 
	thresh = max(thresh, dtc->dirty);
	if (limit > thresh) {
		limit -= (limit - thresh) >> 5;
		goto update;
	}
	return;
update:
	dom->dirty_limit = limit;
}

static void domain_update_dirty_limit(struct dirty_throttle_control *dtc,
				      unsigned long now)
{
	struct wb_domain *dom = dtc_dom(dtc);

	 
	if (time_before(now, dom->dirty_limit_tstamp + BANDWIDTH_INTERVAL))
		return;

	spin_lock(&dom->lock);
	if (time_after_eq(now, dom->dirty_limit_tstamp + BANDWIDTH_INTERVAL)) {
		update_dirty_limit(dtc);
		dom->dirty_limit_tstamp = now;
	}
	spin_unlock(&dom->lock);
}

 
static void wb_update_dirty_ratelimit(struct dirty_throttle_control *dtc,
				      unsigned long dirtied,
				      unsigned long elapsed)
{
	struct bdi_writeback *wb = dtc->wb;
	unsigned long dirty = dtc->dirty;
	unsigned long freerun = dirty_freerun_ceiling(dtc->thresh, dtc->bg_thresh);
	unsigned long limit = hard_dirty_limit(dtc_dom(dtc), dtc->thresh);
	unsigned long setpoint = (freerun + limit) / 2;
	unsigned long write_bw = wb->avg_write_bandwidth;
	unsigned long dirty_ratelimit = wb->dirty_ratelimit;
	unsigned long dirty_rate;
	unsigned long task_ratelimit;
	unsigned long balanced_dirty_ratelimit;
	unsigned long step;
	unsigned long x;
	unsigned long shift;

	 
	dirty_rate = (dirtied - wb->dirtied_stamp) * HZ / elapsed;

	 
	task_ratelimit = (u64)dirty_ratelimit *
					dtc->pos_ratio >> RATELIMIT_CALC_SHIFT;
	task_ratelimit++;  

	 
	balanced_dirty_ratelimit = div_u64((u64)task_ratelimit * write_bw,
					   dirty_rate | 1);
	 
	if (unlikely(balanced_dirty_ratelimit > write_bw))
		balanced_dirty_ratelimit = write_bw;

	 

	 
	step = 0;

	 
	if (unlikely(wb->bdi->capabilities & BDI_CAP_STRICTLIMIT)) {
		dirty = dtc->wb_dirty;
		if (dtc->wb_dirty < 8)
			setpoint = dtc->wb_dirty + 1;
		else
			setpoint = (dtc->wb_thresh + dtc->wb_bg_thresh) / 2;
	}

	if (dirty < setpoint) {
		x = min3(wb->balanced_dirty_ratelimit,
			 balanced_dirty_ratelimit, task_ratelimit);
		if (dirty_ratelimit < x)
			step = x - dirty_ratelimit;
	} else {
		x = max3(wb->balanced_dirty_ratelimit,
			 balanced_dirty_ratelimit, task_ratelimit);
		if (dirty_ratelimit > x)
			step = dirty_ratelimit - x;
	}

	 
	shift = dirty_ratelimit / (2 * step + 1);
	if (shift < BITS_PER_LONG)
		step = DIV_ROUND_UP(step >> shift, 8);
	else
		step = 0;

	if (dirty_ratelimit < balanced_dirty_ratelimit)
		dirty_ratelimit += step;
	else
		dirty_ratelimit -= step;

	WRITE_ONCE(wb->dirty_ratelimit, max(dirty_ratelimit, 1UL));
	wb->balanced_dirty_ratelimit = balanced_dirty_ratelimit;

	trace_bdi_dirty_ratelimit(wb, dirty_rate, task_ratelimit);
}

static void __wb_update_bandwidth(struct dirty_throttle_control *gdtc,
				  struct dirty_throttle_control *mdtc,
				  bool update_ratelimit)
{
	struct bdi_writeback *wb = gdtc->wb;
	unsigned long now = jiffies;
	unsigned long elapsed;
	unsigned long dirtied;
	unsigned long written;

	spin_lock(&wb->list_lock);

	 
	elapsed = max(now - wb->bw_time_stamp, 1UL);
	dirtied = percpu_counter_read(&wb->stat[WB_DIRTIED]);
	written = percpu_counter_read(&wb->stat[WB_WRITTEN]);

	if (update_ratelimit) {
		domain_update_dirty_limit(gdtc, now);
		wb_update_dirty_ratelimit(gdtc, dirtied, elapsed);

		 
		if (IS_ENABLED(CONFIG_CGROUP_WRITEBACK) && mdtc) {
			domain_update_dirty_limit(mdtc, now);
			wb_update_dirty_ratelimit(mdtc, dirtied, elapsed);
		}
	}
	wb_update_write_bandwidth(wb, elapsed, written);

	wb->dirtied_stamp = dirtied;
	wb->written_stamp = written;
	WRITE_ONCE(wb->bw_time_stamp, now);
	spin_unlock(&wb->list_lock);
}

void wb_update_bandwidth(struct bdi_writeback *wb)
{
	struct dirty_throttle_control gdtc = { GDTC_INIT(wb) };

	__wb_update_bandwidth(&gdtc, NULL, false);
}

 
#define WB_BANDWIDTH_IDLE_JIF (HZ)

static void wb_bandwidth_estimate_start(struct bdi_writeback *wb)
{
	unsigned long now = jiffies;
	unsigned long elapsed = now - READ_ONCE(wb->bw_time_stamp);

	if (elapsed > WB_BANDWIDTH_IDLE_JIF &&
	    !atomic_read(&wb->writeback_inodes)) {
		spin_lock(&wb->list_lock);
		wb->dirtied_stamp = wb_stat(wb, WB_DIRTIED);
		wb->written_stamp = wb_stat(wb, WB_WRITTEN);
		WRITE_ONCE(wb->bw_time_stamp, now);
		spin_unlock(&wb->list_lock);
	}
}

 
static unsigned long dirty_poll_interval(unsigned long dirty,
					 unsigned long thresh)
{
	if (thresh > dirty)
		return 1UL << (ilog2(thresh - dirty) >> 1);

	return 1;
}

static unsigned long wb_max_pause(struct bdi_writeback *wb,
				  unsigned long wb_dirty)
{
	unsigned long bw = READ_ONCE(wb->avg_write_bandwidth);
	unsigned long t;

	 
	t = wb_dirty / (1 + bw / roundup_pow_of_two(1 + HZ / 8));
	t++;

	return min_t(unsigned long, t, MAX_PAUSE);
}

static long wb_min_pause(struct bdi_writeback *wb,
			 long max_pause,
			 unsigned long task_ratelimit,
			 unsigned long dirty_ratelimit,
			 int *nr_dirtied_pause)
{
	long hi = ilog2(READ_ONCE(wb->avg_write_bandwidth));
	long lo = ilog2(READ_ONCE(wb->dirty_ratelimit));
	long t;		 
	long pause;	 
	int pages;	 

	 
	t = max(1, HZ / 100);

	 
	if (hi > lo)
		t += (hi - lo) * (10 * HZ) / 1024;

	 
	t = min(t, 1 + max_pause / 2);
	pages = dirty_ratelimit * t / roundup_pow_of_two(HZ);

	 
	if (pages < DIRTY_POLL_THRESH) {
		t = max_pause;
		pages = dirty_ratelimit * t / roundup_pow_of_two(HZ);
		if (pages > DIRTY_POLL_THRESH) {
			pages = DIRTY_POLL_THRESH;
			t = HZ * DIRTY_POLL_THRESH / dirty_ratelimit;
		}
	}

	pause = HZ * pages / (task_ratelimit + 1);
	if (pause > max_pause) {
		t = max_pause;
		pages = task_ratelimit * t / roundup_pow_of_two(HZ);
	}

	*nr_dirtied_pause = pages;
	 
	return pages >= DIRTY_POLL_THRESH ? 1 + t / 2 : t;
}

static inline void wb_dirty_limits(struct dirty_throttle_control *dtc)
{
	struct bdi_writeback *wb = dtc->wb;
	unsigned long wb_reclaimable;

	 
	dtc->wb_thresh = __wb_calc_thresh(dtc);
	dtc->wb_bg_thresh = dtc->thresh ?
		div_u64((u64)dtc->wb_thresh * dtc->bg_thresh, dtc->thresh) : 0;

	 
	if (dtc->wb_thresh < 2 * wb_stat_error()) {
		wb_reclaimable = wb_stat_sum(wb, WB_RECLAIMABLE);
		dtc->wb_dirty = wb_reclaimable + wb_stat_sum(wb, WB_WRITEBACK);
	} else {
		wb_reclaimable = wb_stat(wb, WB_RECLAIMABLE);
		dtc->wb_dirty = wb_reclaimable + wb_stat(wb, WB_WRITEBACK);
	}
}

 
static int balance_dirty_pages(struct bdi_writeback *wb,
			       unsigned long pages_dirtied, unsigned int flags)
{
	struct dirty_throttle_control gdtc_stor = { GDTC_INIT(wb) };
	struct dirty_throttle_control mdtc_stor = { MDTC_INIT(wb, &gdtc_stor) };
	struct dirty_throttle_control * const gdtc = &gdtc_stor;
	struct dirty_throttle_control * const mdtc = mdtc_valid(&mdtc_stor) ?
						     &mdtc_stor : NULL;
	struct dirty_throttle_control *sdtc;
	unsigned long nr_reclaimable;	 
	long period;
	long pause;
	long max_pause;
	long min_pause;
	int nr_dirtied_pause;
	bool dirty_exceeded = false;
	unsigned long task_ratelimit;
	unsigned long dirty_ratelimit;
	struct backing_dev_info *bdi = wb->bdi;
	bool strictlimit = bdi->capabilities & BDI_CAP_STRICTLIMIT;
	unsigned long start_time = jiffies;
	int ret = 0;

	for (;;) {
		unsigned long now = jiffies;
		unsigned long dirty, thresh, bg_thresh;
		unsigned long m_dirty = 0;	 
		unsigned long m_thresh = 0;
		unsigned long m_bg_thresh = 0;

		nr_reclaimable = global_node_page_state(NR_FILE_DIRTY);
		gdtc->avail = global_dirtyable_memory();
		gdtc->dirty = nr_reclaimable + global_node_page_state(NR_WRITEBACK);

		domain_dirty_limits(gdtc);

		if (unlikely(strictlimit)) {
			wb_dirty_limits(gdtc);

			dirty = gdtc->wb_dirty;
			thresh = gdtc->wb_thresh;
			bg_thresh = gdtc->wb_bg_thresh;
		} else {
			dirty = gdtc->dirty;
			thresh = gdtc->thresh;
			bg_thresh = gdtc->bg_thresh;
		}

		if (mdtc) {
			unsigned long filepages, headroom, writeback;

			 
			mem_cgroup_wb_stats(wb, &filepages, &headroom,
					    &mdtc->dirty, &writeback);
			mdtc->dirty += writeback;
			mdtc_calc_avail(mdtc, filepages, headroom);

			domain_dirty_limits(mdtc);

			if (unlikely(strictlimit)) {
				wb_dirty_limits(mdtc);
				m_dirty = mdtc->wb_dirty;
				m_thresh = mdtc->wb_thresh;
				m_bg_thresh = mdtc->wb_bg_thresh;
			} else {
				m_dirty = mdtc->dirty;
				m_thresh = mdtc->thresh;
				m_bg_thresh = mdtc->bg_thresh;
			}
		}

		 
		if (!laptop_mode && nr_reclaimable > gdtc->bg_thresh &&
		    !writeback_in_progress(wb))
			wb_start_background_writeback(wb);

		 
		if (dirty <= dirty_freerun_ceiling(thresh, bg_thresh) &&
		    (!mdtc ||
		     m_dirty <= dirty_freerun_ceiling(m_thresh, m_bg_thresh))) {
			unsigned long intv;
			unsigned long m_intv;

free_running:
			intv = dirty_poll_interval(dirty, thresh);
			m_intv = ULONG_MAX;

			current->dirty_paused_when = now;
			current->nr_dirtied = 0;
			if (mdtc)
				m_intv = dirty_poll_interval(m_dirty, m_thresh);
			current->nr_dirtied_pause = min(intv, m_intv);
			break;
		}

		 
		if (unlikely(!writeback_in_progress(wb)))
			wb_start_background_writeback(wb);

		mem_cgroup_flush_foreign(wb);

		 
		if (!strictlimit) {
			wb_dirty_limits(gdtc);

			if ((current->flags & PF_LOCAL_THROTTLE) &&
			    gdtc->wb_dirty <
			    dirty_freerun_ceiling(gdtc->wb_thresh,
						  gdtc->wb_bg_thresh))
				 
				goto free_running;
		}

		dirty_exceeded = (gdtc->wb_dirty > gdtc->wb_thresh) &&
			((gdtc->dirty > gdtc->thresh) || strictlimit);

		wb_position_ratio(gdtc);
		sdtc = gdtc;

		if (mdtc) {
			 
			if (!strictlimit) {
				wb_dirty_limits(mdtc);

				if ((current->flags & PF_LOCAL_THROTTLE) &&
				    mdtc->wb_dirty <
				    dirty_freerun_ceiling(mdtc->wb_thresh,
							  mdtc->wb_bg_thresh))
					 
					goto free_running;
			}
			dirty_exceeded |= (mdtc->wb_dirty > mdtc->wb_thresh) &&
				((mdtc->dirty > mdtc->thresh) || strictlimit);

			wb_position_ratio(mdtc);
			if (mdtc->pos_ratio < gdtc->pos_ratio)
				sdtc = mdtc;
		}

		if (dirty_exceeded != wb->dirty_exceeded)
			wb->dirty_exceeded = dirty_exceeded;

		if (time_is_before_jiffies(READ_ONCE(wb->bw_time_stamp) +
					   BANDWIDTH_INTERVAL))
			__wb_update_bandwidth(gdtc, mdtc, true);

		 
		dirty_ratelimit = READ_ONCE(wb->dirty_ratelimit);
		task_ratelimit = ((u64)dirty_ratelimit * sdtc->pos_ratio) >>
							RATELIMIT_CALC_SHIFT;
		max_pause = wb_max_pause(wb, sdtc->wb_dirty);
		min_pause = wb_min_pause(wb, max_pause,
					 task_ratelimit, dirty_ratelimit,
					 &nr_dirtied_pause);

		if (unlikely(task_ratelimit == 0)) {
			period = max_pause;
			pause = max_pause;
			goto pause;
		}
		period = HZ * pages_dirtied / task_ratelimit;
		pause = period;
		if (current->dirty_paused_when)
			pause -= now - current->dirty_paused_when;
		 
		if (pause < min_pause) {
			trace_balance_dirty_pages(wb,
						  sdtc->thresh,
						  sdtc->bg_thresh,
						  sdtc->dirty,
						  sdtc->wb_thresh,
						  sdtc->wb_dirty,
						  dirty_ratelimit,
						  task_ratelimit,
						  pages_dirtied,
						  period,
						  min(pause, 0L),
						  start_time);
			if (pause < -HZ) {
				current->dirty_paused_when = now;
				current->nr_dirtied = 0;
			} else if (period) {
				current->dirty_paused_when += period;
				current->nr_dirtied = 0;
			} else if (current->nr_dirtied_pause <= pages_dirtied)
				current->nr_dirtied_pause += pages_dirtied;
			break;
		}
		if (unlikely(pause > max_pause)) {
			 
			now += min(pause - max_pause, max_pause);
			pause = max_pause;
		}

pause:
		trace_balance_dirty_pages(wb,
					  sdtc->thresh,
					  sdtc->bg_thresh,
					  sdtc->dirty,
					  sdtc->wb_thresh,
					  sdtc->wb_dirty,
					  dirty_ratelimit,
					  task_ratelimit,
					  pages_dirtied,
					  period,
					  pause,
					  start_time);
		if (flags & BDP_ASYNC) {
			ret = -EAGAIN;
			break;
		}
		__set_current_state(TASK_KILLABLE);
		wb->dirty_sleep = now;
		io_schedule_timeout(pause);

		current->dirty_paused_when = now + pause;
		current->nr_dirtied = 0;
		current->nr_dirtied_pause = nr_dirtied_pause;

		 
		if (task_ratelimit)
			break;

		 
		if (sdtc->wb_dirty <= wb_stat_error())
			break;

		if (fatal_signal_pending(current))
			break;
	}
	return ret;
}

static DEFINE_PER_CPU(int, bdp_ratelimits);

 
DEFINE_PER_CPU(int, dirty_throttle_leaks) = 0;

 
int balance_dirty_pages_ratelimited_flags(struct address_space *mapping,
					unsigned int flags)
{
	struct inode *inode = mapping->host;
	struct backing_dev_info *bdi = inode_to_bdi(inode);
	struct bdi_writeback *wb = NULL;
	int ratelimit;
	int ret = 0;
	int *p;

	if (!(bdi->capabilities & BDI_CAP_WRITEBACK))
		return ret;

	if (inode_cgwb_enabled(inode))
		wb = wb_get_create_current(bdi, GFP_KERNEL);
	if (!wb)
		wb = &bdi->wb;

	ratelimit = current->nr_dirtied_pause;
	if (wb->dirty_exceeded)
		ratelimit = min(ratelimit, 32 >> (PAGE_SHIFT - 10));

	preempt_disable();
	 
	p =  this_cpu_ptr(&bdp_ratelimits);
	if (unlikely(current->nr_dirtied >= ratelimit))
		*p = 0;
	else if (unlikely(*p >= ratelimit_pages)) {
		*p = 0;
		ratelimit = 0;
	}
	 
	p = this_cpu_ptr(&dirty_throttle_leaks);
	if (*p > 0 && current->nr_dirtied < ratelimit) {
		unsigned long nr_pages_dirtied;
		nr_pages_dirtied = min(*p, ratelimit - current->nr_dirtied);
		*p -= nr_pages_dirtied;
		current->nr_dirtied += nr_pages_dirtied;
	}
	preempt_enable();

	if (unlikely(current->nr_dirtied >= ratelimit))
		ret = balance_dirty_pages(wb, current->nr_dirtied, flags);

	wb_put(wb);
	return ret;
}
EXPORT_SYMBOL_GPL(balance_dirty_pages_ratelimited_flags);

 
void balance_dirty_pages_ratelimited(struct address_space *mapping)
{
	balance_dirty_pages_ratelimited_flags(mapping, 0);
}
EXPORT_SYMBOL(balance_dirty_pages_ratelimited);

 
bool wb_over_bg_thresh(struct bdi_writeback *wb)
{
	struct dirty_throttle_control gdtc_stor = { GDTC_INIT(wb) };
	struct dirty_throttle_control mdtc_stor = { MDTC_INIT(wb, &gdtc_stor) };
	struct dirty_throttle_control * const gdtc = &gdtc_stor;
	struct dirty_throttle_control * const mdtc = mdtc_valid(&mdtc_stor) ?
						     &mdtc_stor : NULL;
	unsigned long reclaimable;
	unsigned long thresh;

	 
	gdtc->avail = global_dirtyable_memory();
	gdtc->dirty = global_node_page_state(NR_FILE_DIRTY);
	domain_dirty_limits(gdtc);

	if (gdtc->dirty > gdtc->bg_thresh)
		return true;

	thresh = wb_calc_thresh(gdtc->wb, gdtc->bg_thresh);
	if (thresh < 2 * wb_stat_error())
		reclaimable = wb_stat_sum(wb, WB_RECLAIMABLE);
	else
		reclaimable = wb_stat(wb, WB_RECLAIMABLE);

	if (reclaimable > thresh)
		return true;

	if (mdtc) {
		unsigned long filepages, headroom, writeback;

		mem_cgroup_wb_stats(wb, &filepages, &headroom, &mdtc->dirty,
				    &writeback);
		mdtc_calc_avail(mdtc, filepages, headroom);
		domain_dirty_limits(mdtc);	 

		if (mdtc->dirty > mdtc->bg_thresh)
			return true;

		thresh = wb_calc_thresh(mdtc->wb, mdtc->bg_thresh);
		if (thresh < 2 * wb_stat_error())
			reclaimable = wb_stat_sum(wb, WB_RECLAIMABLE);
		else
			reclaimable = wb_stat(wb, WB_RECLAIMABLE);

		if (reclaimable > thresh)
			return true;
	}

	return false;
}

#ifdef CONFIG_SYSCTL
 
static int dirty_writeback_centisecs_handler(struct ctl_table *table, int write,
		void *buffer, size_t *length, loff_t *ppos)
{
	unsigned int old_interval = dirty_writeback_interval;
	int ret;

	ret = proc_dointvec(table, write, buffer, length, ppos);

	 
	if (!ret && write && dirty_writeback_interval &&
		dirty_writeback_interval != old_interval)
		wakeup_flusher_threads(WB_REASON_PERIODIC);

	return ret;
}
#endif

void laptop_mode_timer_fn(struct timer_list *t)
{
	struct backing_dev_info *backing_dev_info =
		from_timer(backing_dev_info, t, laptop_mode_wb_timer);

	wakeup_flusher_threads_bdi(backing_dev_info, WB_REASON_LAPTOP_TIMER);
}

 
void laptop_io_completion(struct backing_dev_info *info)
{
	mod_timer(&info->laptop_mode_wb_timer, jiffies + laptop_mode);
}

 
void laptop_sync_completion(void)
{
	struct backing_dev_info *bdi;

	rcu_read_lock();

	list_for_each_entry_rcu(bdi, &bdi_list, bdi_list)
		del_timer(&bdi->laptop_mode_wb_timer);

	rcu_read_unlock();
}

 

void writeback_set_ratelimit(void)
{
	struct wb_domain *dom = &global_wb_domain;
	unsigned long background_thresh;
	unsigned long dirty_thresh;

	global_dirty_limits(&background_thresh, &dirty_thresh);
	dom->dirty_limit = dirty_thresh;
	ratelimit_pages = dirty_thresh / (num_online_cpus() * 32);
	if (ratelimit_pages < 16)
		ratelimit_pages = 16;
}

static int page_writeback_cpu_online(unsigned int cpu)
{
	writeback_set_ratelimit();
	return 0;
}

#ifdef CONFIG_SYSCTL

 
static const unsigned long dirty_bytes_min = 2 * PAGE_SIZE;

static struct ctl_table vm_page_writeback_sysctls[] = {
	{
		.procname   = "dirty_background_ratio",
		.data       = &dirty_background_ratio,
		.maxlen     = sizeof(dirty_background_ratio),
		.mode       = 0644,
		.proc_handler   = dirty_background_ratio_handler,
		.extra1     = SYSCTL_ZERO,
		.extra2     = SYSCTL_ONE_HUNDRED,
	},
	{
		.procname   = "dirty_background_bytes",
		.data       = &dirty_background_bytes,
		.maxlen     = sizeof(dirty_background_bytes),
		.mode       = 0644,
		.proc_handler   = dirty_background_bytes_handler,
		.extra1     = SYSCTL_LONG_ONE,
	},
	{
		.procname   = "dirty_ratio",
		.data       = &vm_dirty_ratio,
		.maxlen     = sizeof(vm_dirty_ratio),
		.mode       = 0644,
		.proc_handler   = dirty_ratio_handler,
		.extra1     = SYSCTL_ZERO,
		.extra2     = SYSCTL_ONE_HUNDRED,
	},
	{
		.procname   = "dirty_bytes",
		.data       = &vm_dirty_bytes,
		.maxlen     = sizeof(vm_dirty_bytes),
		.mode       = 0644,
		.proc_handler   = dirty_bytes_handler,
		.extra1     = (void *)&dirty_bytes_min,
	},
	{
		.procname   = "dirty_writeback_centisecs",
		.data       = &dirty_writeback_interval,
		.maxlen     = sizeof(dirty_writeback_interval),
		.mode       = 0644,
		.proc_handler   = dirty_writeback_centisecs_handler,
	},
	{
		.procname   = "dirty_expire_centisecs",
		.data       = &dirty_expire_interval,
		.maxlen     = sizeof(dirty_expire_interval),
		.mode       = 0644,
		.proc_handler   = proc_dointvec_minmax,
		.extra1     = SYSCTL_ZERO,
	},
#ifdef CONFIG_HIGHMEM
	{
		.procname	= "highmem_is_dirtyable",
		.data		= &vm_highmem_is_dirtyable,
		.maxlen		= sizeof(vm_highmem_is_dirtyable),
		.mode		= 0644,
		.proc_handler	= proc_dointvec_minmax,
		.extra1		= SYSCTL_ZERO,
		.extra2		= SYSCTL_ONE,
	},
#endif
	{
		.procname	= "laptop_mode",
		.data		= &laptop_mode,
		.maxlen		= sizeof(laptop_mode),
		.mode		= 0644,
		.proc_handler	= proc_dointvec_jiffies,
	},
	{}
};
#endif

 
void __init page_writeback_init(void)
{
	BUG_ON(wb_domain_init(&global_wb_domain, GFP_KERNEL));

	cpuhp_setup_state(CPUHP_AP_ONLINE_DYN, "mm/writeback:online",
			  page_writeback_cpu_online, NULL);
	cpuhp_setup_state(CPUHP_MM_WRITEBACK_DEAD, "mm/writeback:dead", NULL,
			  page_writeback_cpu_online);
#ifdef CONFIG_SYSCTL
	register_sysctl_init("vm", vm_page_writeback_sysctls);
#endif
}

 
void tag_pages_for_writeback(struct address_space *mapping,
			     pgoff_t start, pgoff_t end)
{
	XA_STATE(xas, &mapping->i_pages, start);
	unsigned int tagged = 0;
	void *page;

	xas_lock_irq(&xas);
	xas_for_each_marked(&xas, page, end, PAGECACHE_TAG_DIRTY) {
		xas_set_mark(&xas, PAGECACHE_TAG_TOWRITE);
		if (++tagged % XA_CHECK_SCHED)
			continue;

		xas_pause(&xas);
		xas_unlock_irq(&xas);
		cond_resched();
		xas_lock_irq(&xas);
	}
	xas_unlock_irq(&xas);
}
EXPORT_SYMBOL(tag_pages_for_writeback);

 
int write_cache_pages(struct address_space *mapping,
		      struct writeback_control *wbc, writepage_t writepage,
		      void *data)
{
	int ret = 0;
	int done = 0;
	int error;
	struct folio_batch fbatch;
	int nr_folios;
	pgoff_t index;
	pgoff_t end;		 
	pgoff_t done_index;
	int range_whole = 0;
	xa_mark_t tag;

	folio_batch_init(&fbatch);
	if (wbc->range_cyclic) {
		index = mapping->writeback_index;  
		end = -1;
	} else {
		index = wbc->range_start >> PAGE_SHIFT;
		end = wbc->range_end >> PAGE_SHIFT;
		if (wbc->range_start == 0 && wbc->range_end == LLONG_MAX)
			range_whole = 1;
	}
	if (wbc->sync_mode == WB_SYNC_ALL || wbc->tagged_writepages) {
		tag_pages_for_writeback(mapping, index, end);
		tag = PAGECACHE_TAG_TOWRITE;
	} else {
		tag = PAGECACHE_TAG_DIRTY;
	}
	done_index = index;
	while (!done && (index <= end)) {
		int i;

		nr_folios = filemap_get_folios_tag(mapping, &index, end,
				tag, &fbatch);

		if (nr_folios == 0)
			break;

		for (i = 0; i < nr_folios; i++) {
			struct folio *folio = fbatch.folios[i];
			unsigned long nr;

			done_index = folio->index;

			folio_lock(folio);

			 
			if (unlikely(folio->mapping != mapping)) {
continue_unlock:
				folio_unlock(folio);
				continue;
			}

			if (!folio_test_dirty(folio)) {
				 
				goto continue_unlock;
			}

			if (folio_test_writeback(folio)) {
				if (wbc->sync_mode != WB_SYNC_NONE)
					folio_wait_writeback(folio);
				else
					goto continue_unlock;
			}

			BUG_ON(folio_test_writeback(folio));
			if (!folio_clear_dirty_for_io(folio))
				goto continue_unlock;

			trace_wbc_writepage(wbc, inode_to_bdi(mapping->host));
			error = writepage(folio, wbc, data);
			nr = folio_nr_pages(folio);
			if (unlikely(error)) {
				 
				if (error == AOP_WRITEPAGE_ACTIVATE) {
					folio_unlock(folio);
					error = 0;
				} else if (wbc->sync_mode != WB_SYNC_ALL) {
					ret = error;
					done_index = folio->index + nr;
					done = 1;
					break;
				}
				if (!ret)
					ret = error;
			}

			 
			wbc->nr_to_write -= nr;
			if (wbc->nr_to_write <= 0 &&
			    wbc->sync_mode == WB_SYNC_NONE) {
				done = 1;
				break;
			}
		}
		folio_batch_release(&fbatch);
		cond_resched();
	}

	 
	if (wbc->range_cyclic && !done)
		done_index = 0;
	if (wbc->range_cyclic || (range_whole && wbc->nr_to_write > 0))
		mapping->writeback_index = done_index;

	return ret;
}
EXPORT_SYMBOL(write_cache_pages);

static int writepage_cb(struct folio *folio, struct writeback_control *wbc,
		void *data)
{
	struct address_space *mapping = data;
	int ret = mapping->a_ops->writepage(&folio->page, wbc);
	mapping_set_error(mapping, ret);
	return ret;
}

int do_writepages(struct address_space *mapping, struct writeback_control *wbc)
{
	int ret;
	struct bdi_writeback *wb;

	if (wbc->nr_to_write <= 0)
		return 0;
	wb = inode_to_wb_wbc(mapping->host, wbc);
	wb_bandwidth_estimate_start(wb);
	while (1) {
		if (mapping->a_ops->writepages) {
			ret = mapping->a_ops->writepages(mapping, wbc);
		} else if (mapping->a_ops->writepage) {
			struct blk_plug plug;

			blk_start_plug(&plug);
			ret = write_cache_pages(mapping, wbc, writepage_cb,
						mapping);
			blk_finish_plug(&plug);
		} else {
			 
			ret = 0;
		}
		if (ret != -ENOMEM || wbc->sync_mode != WB_SYNC_ALL)
			break;

		 
		reclaim_throttle(NODE_DATA(numa_node_id()),
			VMSCAN_THROTTLE_WRITEBACK);
	}
	 
	if (time_is_before_jiffies(READ_ONCE(wb->bw_time_stamp) +
				   BANDWIDTH_INTERVAL))
		wb_update_bandwidth(wb);
	return ret;
}

 
bool noop_dirty_folio(struct address_space *mapping, struct folio *folio)
{
	if (!folio_test_dirty(folio))
		return !folio_test_set_dirty(folio);
	return false;
}
EXPORT_SYMBOL(noop_dirty_folio);

 
static void folio_account_dirtied(struct folio *folio,
		struct address_space *mapping)
{
	struct inode *inode = mapping->host;

	trace_writeback_dirty_folio(folio, mapping);

	if (mapping_can_writeback(mapping)) {
		struct bdi_writeback *wb;
		long nr = folio_nr_pages(folio);

		inode_attach_wb(inode, folio);
		wb = inode_to_wb(inode);

		__lruvec_stat_mod_folio(folio, NR_FILE_DIRTY, nr);
		__zone_stat_mod_folio(folio, NR_ZONE_WRITE_PENDING, nr);
		__node_stat_mod_folio(folio, NR_DIRTIED, nr);
		wb_stat_mod(wb, WB_RECLAIMABLE, nr);
		wb_stat_mod(wb, WB_DIRTIED, nr);
		task_io_account_write(nr * PAGE_SIZE);
		current->nr_dirtied += nr;
		__this_cpu_add(bdp_ratelimits, nr);

		mem_cgroup_track_foreign_dirty(folio, wb);
	}
}

 
void folio_account_cleaned(struct folio *folio, struct bdi_writeback *wb)
{
	long nr = folio_nr_pages(folio);

	lruvec_stat_mod_folio(folio, NR_FILE_DIRTY, -nr);
	zone_stat_mod_folio(folio, NR_ZONE_WRITE_PENDING, -nr);
	wb_stat_mod(wb, WB_RECLAIMABLE, -nr);
	task_io_account_cancelled_write(nr * PAGE_SIZE);
}

 
void __folio_mark_dirty(struct folio *folio, struct address_space *mapping,
			     int warn)
{
	unsigned long flags;

	xa_lock_irqsave(&mapping->i_pages, flags);
	if (folio->mapping) {	 
		WARN_ON_ONCE(warn && !folio_test_uptodate(folio));
		folio_account_dirtied(folio, mapping);
		__xa_set_mark(&mapping->i_pages, folio_index(folio),
				PAGECACHE_TAG_DIRTY);
	}
	xa_unlock_irqrestore(&mapping->i_pages, flags);
}

 
bool filemap_dirty_folio(struct address_space *mapping, struct folio *folio)
{
	folio_memcg_lock(folio);
	if (folio_test_set_dirty(folio)) {
		folio_memcg_unlock(folio);
		return false;
	}

	__folio_mark_dirty(folio, mapping, !folio_test_private(folio));
	folio_memcg_unlock(folio);

	if (mapping->host) {
		 
		__mark_inode_dirty(mapping->host, I_DIRTY_PAGES);
	}
	return true;
}
EXPORT_SYMBOL(filemap_dirty_folio);

 
bool folio_redirty_for_writepage(struct writeback_control *wbc,
		struct folio *folio)
{
	struct address_space *mapping = folio->mapping;
	long nr = folio_nr_pages(folio);
	bool ret;

	wbc->pages_skipped += nr;
	ret = filemap_dirty_folio(mapping, folio);
	if (mapping && mapping_can_writeback(mapping)) {
		struct inode *inode = mapping->host;
		struct bdi_writeback *wb;
		struct wb_lock_cookie cookie = {};

		wb = unlocked_inode_to_wb_begin(inode, &cookie);
		current->nr_dirtied -= nr;
		node_stat_mod_folio(folio, NR_DIRTIED, -nr);
		wb_stat_mod(wb, WB_DIRTIED, -nr);
		unlocked_inode_to_wb_end(inode, &cookie);
	}
	return ret;
}
EXPORT_SYMBOL(folio_redirty_for_writepage);

 
bool folio_mark_dirty(struct folio *folio)
{
	struct address_space *mapping = folio_mapping(folio);

	if (likely(mapping)) {
		 
		if (folio_test_reclaim(folio))
			folio_clear_reclaim(folio);
		return mapping->a_ops->dirty_folio(mapping, folio);
	}

	return noop_dirty_folio(mapping, folio);
}
EXPORT_SYMBOL(folio_mark_dirty);

 
int set_page_dirty_lock(struct page *page)
{
	int ret;

	lock_page(page);
	ret = set_page_dirty(page);
	unlock_page(page);
	return ret;
}
EXPORT_SYMBOL(set_page_dirty_lock);

 
void __folio_cancel_dirty(struct folio *folio)
{
	struct address_space *mapping = folio_mapping(folio);

	if (mapping_can_writeback(mapping)) {
		struct inode *inode = mapping->host;
		struct bdi_writeback *wb;
		struct wb_lock_cookie cookie = {};

		folio_memcg_lock(folio);
		wb = unlocked_inode_to_wb_begin(inode, &cookie);

		if (folio_test_clear_dirty(folio))
			folio_account_cleaned(folio, wb);

		unlocked_inode_to_wb_end(inode, &cookie);
		folio_memcg_unlock(folio);
	} else {
		folio_clear_dirty(folio);
	}
}
EXPORT_SYMBOL(__folio_cancel_dirty);

 
bool folio_clear_dirty_for_io(struct folio *folio)
{
	struct address_space *mapping = folio_mapping(folio);
	bool ret = false;

	VM_BUG_ON_FOLIO(!folio_test_locked(folio), folio);

	if (mapping && mapping_can_writeback(mapping)) {
		struct inode *inode = mapping->host;
		struct bdi_writeback *wb;
		struct wb_lock_cookie cookie = {};

		 
		if (folio_mkclean(folio))
			folio_mark_dirty(folio);
		 
		wb = unlocked_inode_to_wb_begin(inode, &cookie);
		if (folio_test_clear_dirty(folio)) {
			long nr = folio_nr_pages(folio);
			lruvec_stat_mod_folio(folio, NR_FILE_DIRTY, -nr);
			zone_stat_mod_folio(folio, NR_ZONE_WRITE_PENDING, -nr);
			wb_stat_mod(wb, WB_RECLAIMABLE, -nr);
			ret = true;
		}
		unlocked_inode_to_wb_end(inode, &cookie);
		return ret;
	}
	return folio_test_clear_dirty(folio);
}
EXPORT_SYMBOL(folio_clear_dirty_for_io);

static void wb_inode_writeback_start(struct bdi_writeback *wb)
{
	atomic_inc(&wb->writeback_inodes);
}

static void wb_inode_writeback_end(struct bdi_writeback *wb)
{
	unsigned long flags;
	atomic_dec(&wb->writeback_inodes);
	 
	spin_lock_irqsave(&wb->work_lock, flags);
	if (test_bit(WB_registered, &wb->state))
		queue_delayed_work(bdi_wq, &wb->bw_dwork, BANDWIDTH_INTERVAL);
	spin_unlock_irqrestore(&wb->work_lock, flags);
}

bool __folio_end_writeback(struct folio *folio)
{
	long nr = folio_nr_pages(folio);
	struct address_space *mapping = folio_mapping(folio);
	bool ret;

	folio_memcg_lock(folio);
	if (mapping && mapping_use_writeback_tags(mapping)) {
		struct inode *inode = mapping->host;
		struct backing_dev_info *bdi = inode_to_bdi(inode);
		unsigned long flags;

		xa_lock_irqsave(&mapping->i_pages, flags);
		ret = folio_test_clear_writeback(folio);
		if (ret) {
			__xa_clear_mark(&mapping->i_pages, folio_index(folio),
						PAGECACHE_TAG_WRITEBACK);
			if (bdi->capabilities & BDI_CAP_WRITEBACK_ACCT) {
				struct bdi_writeback *wb = inode_to_wb(inode);

				wb_stat_mod(wb, WB_WRITEBACK, -nr);
				__wb_writeout_add(wb, nr);
				if (!mapping_tagged(mapping,
						    PAGECACHE_TAG_WRITEBACK))
					wb_inode_writeback_end(wb);
			}
		}

		if (mapping->host && !mapping_tagged(mapping,
						     PAGECACHE_TAG_WRITEBACK))
			sb_clear_inode_writeback(mapping->host);

		xa_unlock_irqrestore(&mapping->i_pages, flags);
	} else {
		ret = folio_test_clear_writeback(folio);
	}
	if (ret) {
		lruvec_stat_mod_folio(folio, NR_WRITEBACK, -nr);
		zone_stat_mod_folio(folio, NR_ZONE_WRITE_PENDING, -nr);
		node_stat_mod_folio(folio, NR_WRITTEN, nr);
	}
	folio_memcg_unlock(folio);
	return ret;
}

bool __folio_start_writeback(struct folio *folio, bool keep_write)
{
	long nr = folio_nr_pages(folio);
	struct address_space *mapping = folio_mapping(folio);
	bool ret;
	int access_ret;

	folio_memcg_lock(folio);
	if (mapping && mapping_use_writeback_tags(mapping)) {
		XA_STATE(xas, &mapping->i_pages, folio_index(folio));
		struct inode *inode = mapping->host;
		struct backing_dev_info *bdi = inode_to_bdi(inode);
		unsigned long flags;

		xas_lock_irqsave(&xas, flags);
		xas_load(&xas);
		ret = folio_test_set_writeback(folio);
		if (!ret) {
			bool on_wblist;

			on_wblist = mapping_tagged(mapping,
						   PAGECACHE_TAG_WRITEBACK);

			xas_set_mark(&xas, PAGECACHE_TAG_WRITEBACK);
			if (bdi->capabilities & BDI_CAP_WRITEBACK_ACCT) {
				struct bdi_writeback *wb = inode_to_wb(inode);

				wb_stat_mod(wb, WB_WRITEBACK, nr);
				if (!on_wblist)
					wb_inode_writeback_start(wb);
			}

			 
			if (mapping->host && !on_wblist)
				sb_mark_inode_writeback(mapping->host);
		}
		if (!folio_test_dirty(folio))
			xas_clear_mark(&xas, PAGECACHE_TAG_DIRTY);
		if (!keep_write)
			xas_clear_mark(&xas, PAGECACHE_TAG_TOWRITE);
		xas_unlock_irqrestore(&xas, flags);
	} else {
		ret = folio_test_set_writeback(folio);
	}
	if (!ret) {
		lruvec_stat_mod_folio(folio, NR_WRITEBACK, nr);
		zone_stat_mod_folio(folio, NR_ZONE_WRITE_PENDING, nr);
	}
	folio_memcg_unlock(folio);
	access_ret = arch_make_folio_accessible(folio);
	 
	VM_BUG_ON_FOLIO(access_ret != 0, folio);

	return ret;
}
EXPORT_SYMBOL(__folio_start_writeback);

 
void folio_wait_writeback(struct folio *folio)
{
	while (folio_test_writeback(folio)) {
		trace_folio_wait_writeback(folio, folio_mapping(folio));
		folio_wait_bit(folio, PG_writeback);
	}
}
EXPORT_SYMBOL_GPL(folio_wait_writeback);

 
int folio_wait_writeback_killable(struct folio *folio)
{
	while (folio_test_writeback(folio)) {
		trace_folio_wait_writeback(folio, folio_mapping(folio));
		if (folio_wait_bit_killable(folio, PG_writeback))
			return -EINTR;
	}

	return 0;
}
EXPORT_SYMBOL_GPL(folio_wait_writeback_killable);

 
void folio_wait_stable(struct folio *folio)
{
	if (mapping_stable_writes(folio_mapping(folio)))
		folio_wait_writeback(folio);
}
EXPORT_SYMBOL_GPL(folio_wait_stable);
