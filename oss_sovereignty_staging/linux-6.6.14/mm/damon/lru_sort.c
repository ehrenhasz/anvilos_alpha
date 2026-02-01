
 

#define pr_fmt(fmt) "damon-lru-sort: " fmt

#include <linux/damon.h>
#include <linux/kstrtox.h>
#include <linux/module.h>

#include "modules-common.h"

#ifdef MODULE_PARAM_PREFIX
#undef MODULE_PARAM_PREFIX
#endif
#define MODULE_PARAM_PREFIX "damon_lru_sort."

 
static bool enabled __read_mostly;

 
static bool commit_inputs __read_mostly;
module_param(commit_inputs, bool, 0600);

 
static unsigned long hot_thres_access_freq = 500;
module_param(hot_thres_access_freq, ulong, 0600);

 
static unsigned long cold_min_age __read_mostly = 120000000;
module_param(cold_min_age, ulong, 0600);

static struct damos_quota damon_lru_sort_quota = {
	 
	.ms = 10,
	.sz = 0,
	.reset_interval = 1000,
	 
	.weight_sz = 0,
	.weight_nr_accesses = 1,
	.weight_age = 0,
};
DEFINE_DAMON_MODULES_DAMOS_TIME_QUOTA(damon_lru_sort_quota);

static struct damos_watermarks damon_lru_sort_wmarks = {
	.metric = DAMOS_WMARK_FREE_MEM_RATE,
	.interval = 5000000,	 
	.high = 200,		 
	.mid = 150,		 
	.low = 50,		 
};
DEFINE_DAMON_MODULES_WMARKS_PARAMS(damon_lru_sort_wmarks);

static struct damon_attrs damon_lru_sort_mon_attrs = {
	.sample_interval = 5000,	 
	.aggr_interval = 100000,	 
	.ops_update_interval = 0,
	.min_nr_regions = 10,
	.max_nr_regions = 1000,
};
DEFINE_DAMON_MODULES_MON_ATTRS_PARAMS(damon_lru_sort_mon_attrs);

 
static unsigned long monitor_region_start __read_mostly;
module_param(monitor_region_start, ulong, 0600);

 
static unsigned long monitor_region_end __read_mostly;
module_param(monitor_region_end, ulong, 0600);

 
static int kdamond_pid __read_mostly = -1;
module_param(kdamond_pid, int, 0400);

static struct damos_stat damon_lru_sort_hot_stat;
DEFINE_DAMON_MODULES_DAMOS_STATS_PARAMS(damon_lru_sort_hot_stat,
		lru_sort_tried_hot_regions, lru_sorted_hot_regions,
		hot_quota_exceeds);

static struct damos_stat damon_lru_sort_cold_stat;
DEFINE_DAMON_MODULES_DAMOS_STATS_PARAMS(damon_lru_sort_cold_stat,
		lru_sort_tried_cold_regions, lru_sorted_cold_regions,
		cold_quota_exceeds);

static struct damos_access_pattern damon_lru_sort_stub_pattern = {
	 
	.min_sz_region = PAGE_SIZE,
	.max_sz_region = ULONG_MAX,
	 
	.min_nr_accesses = 0,
	.max_nr_accesses = UINT_MAX,
	 
	.min_age_region = 0,
	.max_age_region = UINT_MAX,
};

static struct damon_ctx *ctx;
static struct damon_target *target;

static struct damos *damon_lru_sort_new_scheme(
		struct damos_access_pattern *pattern, enum damos_action action)
{
	struct damos_quota quota = damon_lru_sort_quota;

	 
	quota.ms = quota.ms / 2;

	return damon_new_scheme(
			 
			pattern,
			 
			action,
			 
			&quota,
			 
			&damon_lru_sort_wmarks);
}

 
static struct damos *damon_lru_sort_new_hot_scheme(unsigned int hot_thres)
{
	struct damos_access_pattern pattern = damon_lru_sort_stub_pattern;

	pattern.min_nr_accesses = hot_thres;
	return damon_lru_sort_new_scheme(&pattern, DAMOS_LRU_PRIO);
}

 
static struct damos *damon_lru_sort_new_cold_scheme(unsigned int cold_thres)
{
	struct damos_access_pattern pattern = damon_lru_sort_stub_pattern;

	pattern.max_nr_accesses = 0;
	pattern.min_age_region = cold_thres;
	return damon_lru_sort_new_scheme(&pattern, DAMOS_LRU_DEPRIO);
}

static int damon_lru_sort_apply_parameters(void)
{
	struct damos *scheme;
	unsigned int hot_thres, cold_thres;
	int err = 0;

	err = damon_set_attrs(ctx, &damon_lru_sort_mon_attrs);
	if (err)
		return err;

	hot_thres = damon_max_nr_accesses(&damon_lru_sort_mon_attrs) *
		hot_thres_access_freq / 1000;
	scheme = damon_lru_sort_new_hot_scheme(hot_thres);
	if (!scheme)
		return -ENOMEM;
	damon_set_schemes(ctx, &scheme, 1);

	cold_thres = cold_min_age / damon_lru_sort_mon_attrs.aggr_interval;
	scheme = damon_lru_sort_new_cold_scheme(cold_thres);
	if (!scheme)
		return -ENOMEM;
	damon_add_scheme(ctx, scheme);

	return damon_set_region_biggest_system_ram_default(target,
					&monitor_region_start,
					&monitor_region_end);
}

static int damon_lru_sort_turn(bool on)
{
	int err;

	if (!on) {
		err = damon_stop(&ctx, 1);
		if (!err)
			kdamond_pid = -1;
		return err;
	}

	err = damon_lru_sort_apply_parameters();
	if (err)
		return err;

	err = damon_start(&ctx, 1, true);
	if (err)
		return err;
	kdamond_pid = ctx->kdamond->pid;
	return 0;
}

static int damon_lru_sort_enabled_store(const char *val,
		const struct kernel_param *kp)
{
	bool is_enabled = enabled;
	bool enable;
	int err;

	err = kstrtobool(val, &enable);
	if (err)
		return err;

	if (is_enabled == enable)
		return 0;

	 
	if (!ctx)
		goto set_param_out;

	err = damon_lru_sort_turn(enable);
	if (err)
		return err;

set_param_out:
	enabled = enable;
	return err;
}

static const struct kernel_param_ops enabled_param_ops = {
	.set = damon_lru_sort_enabled_store,
	.get = param_get_bool,
};

module_param_cb(enabled, &enabled_param_ops, &enabled, 0600);
MODULE_PARM_DESC(enabled,
	"Enable or disable DAMON_LRU_SORT (default: disabled)");

static int damon_lru_sort_handle_commit_inputs(void)
{
	int err;

	if (!commit_inputs)
		return 0;

	err = damon_lru_sort_apply_parameters();
	commit_inputs = false;
	return err;
}

static int damon_lru_sort_after_aggregation(struct damon_ctx *c)
{
	struct damos *s;

	 
	damon_for_each_scheme(s, c) {
		if (s->action == DAMOS_LRU_PRIO)
			damon_lru_sort_hot_stat = s->stat;
		else if (s->action == DAMOS_LRU_DEPRIO)
			damon_lru_sort_cold_stat = s->stat;
	}

	return damon_lru_sort_handle_commit_inputs();
}

static int damon_lru_sort_after_wmarks_check(struct damon_ctx *c)
{
	return damon_lru_sort_handle_commit_inputs();
}

static int __init damon_lru_sort_init(void)
{
	int err = damon_modules_new_paddr_ctx_target(&ctx, &target);

	if (err)
		return err;

	ctx->callback.after_wmarks_check = damon_lru_sort_after_wmarks_check;
	ctx->callback.after_aggregation = damon_lru_sort_after_aggregation;

	 
	if (enabled)
		err = damon_lru_sort_turn(true);

	return err;
}

module_init(damon_lru_sort_init);
