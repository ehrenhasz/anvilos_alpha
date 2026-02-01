
 

#define pr_fmt(fmt) "damon-reclaim: " fmt

#include <linux/damon.h>
#include <linux/kstrtox.h>
#include <linux/module.h>

#include "modules-common.h"

#ifdef MODULE_PARAM_PREFIX
#undef MODULE_PARAM_PREFIX
#endif
#define MODULE_PARAM_PREFIX "damon_reclaim."

 
static bool enabled __read_mostly;

 
static bool commit_inputs __read_mostly;
module_param(commit_inputs, bool, 0600);

 
static unsigned long min_age __read_mostly = 120000000;
module_param(min_age, ulong, 0600);

static struct damos_quota damon_reclaim_quota = {
	 
	.ms = 10,
	.sz = 128 * 1024 * 1024,
	.reset_interval = 1000,
	 
	.weight_sz = 0,
	.weight_nr_accesses = 0,
	.weight_age = 1
};
DEFINE_DAMON_MODULES_DAMOS_QUOTAS(damon_reclaim_quota);

static struct damos_watermarks damon_reclaim_wmarks = {
	.metric = DAMOS_WMARK_FREE_MEM_RATE,
	.interval = 5000000,	 
	.high = 500,		 
	.mid = 400,		 
	.low = 200,		 
};
DEFINE_DAMON_MODULES_WMARKS_PARAMS(damon_reclaim_wmarks);

static struct damon_attrs damon_reclaim_mon_attrs = {
	.sample_interval = 5000,	 
	.aggr_interval = 100000,	 
	.ops_update_interval = 0,
	.min_nr_regions = 10,
	.max_nr_regions = 1000,
};
DEFINE_DAMON_MODULES_MON_ATTRS_PARAMS(damon_reclaim_mon_attrs);

 
static unsigned long monitor_region_start __read_mostly;
module_param(monitor_region_start, ulong, 0600);

 
static unsigned long monitor_region_end __read_mostly;
module_param(monitor_region_end, ulong, 0600);

 
static bool skip_anon __read_mostly;
module_param(skip_anon, bool, 0600);

 
static int kdamond_pid __read_mostly = -1;
module_param(kdamond_pid, int, 0400);

static struct damos_stat damon_reclaim_stat;
DEFINE_DAMON_MODULES_DAMOS_STATS_PARAMS(damon_reclaim_stat,
		reclaim_tried_regions, reclaimed_regions, quota_exceeds);

static struct damon_ctx *ctx;
static struct damon_target *target;

static struct damos *damon_reclaim_new_scheme(void)
{
	struct damos_access_pattern pattern = {
		 
		.min_sz_region = PAGE_SIZE,
		.max_sz_region = ULONG_MAX,
		 
		.min_nr_accesses = 0,
		.max_nr_accesses = 0,
		 
		.min_age_region = min_age /
			damon_reclaim_mon_attrs.aggr_interval,
		.max_age_region = UINT_MAX,
	};

	return damon_new_scheme(
			&pattern,
			 
			DAMOS_PAGEOUT,
			 
			&damon_reclaim_quota,
			 
			&damon_reclaim_wmarks);
}

static int damon_reclaim_apply_parameters(void)
{
	struct damos *scheme;
	struct damos_filter *filter;
	int err = 0;

	err = damon_set_attrs(ctx, &damon_reclaim_mon_attrs);
	if (err)
		return err;

	 
	scheme = damon_reclaim_new_scheme();
	if (!scheme)
		return -ENOMEM;
	if (skip_anon) {
		filter = damos_new_filter(DAMOS_FILTER_TYPE_ANON, true);
		if (!filter) {
			 
			damon_destroy_scheme(scheme);
			return -ENOMEM;
		}
		damos_add_filter(scheme, filter);
	}
	damon_set_schemes(ctx, &scheme, 1);

	return damon_set_region_biggest_system_ram_default(target,
					&monitor_region_start,
					&monitor_region_end);
}

static int damon_reclaim_turn(bool on)
{
	int err;

	if (!on) {
		err = damon_stop(&ctx, 1);
		if (!err)
			kdamond_pid = -1;
		return err;
	}

	err = damon_reclaim_apply_parameters();
	if (err)
		return err;

	err = damon_start(&ctx, 1, true);
	if (err)
		return err;
	kdamond_pid = ctx->kdamond->pid;
	return 0;
}

static int damon_reclaim_enabled_store(const char *val,
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

	err = damon_reclaim_turn(enable);
	if (err)
		return err;

set_param_out:
	enabled = enable;
	return err;
}

static const struct kernel_param_ops enabled_param_ops = {
	.set = damon_reclaim_enabled_store,
	.get = param_get_bool,
};

module_param_cb(enabled, &enabled_param_ops, &enabled, 0600);
MODULE_PARM_DESC(enabled,
	"Enable or disable DAMON_RECLAIM (default: disabled)");

static int damon_reclaim_handle_commit_inputs(void)
{
	int err;

	if (!commit_inputs)
		return 0;

	err = damon_reclaim_apply_parameters();
	commit_inputs = false;
	return err;
}

static int damon_reclaim_after_aggregation(struct damon_ctx *c)
{
	struct damos *s;

	 
	damon_for_each_scheme(s, c)
		damon_reclaim_stat = s->stat;

	return damon_reclaim_handle_commit_inputs();
}

static int damon_reclaim_after_wmarks_check(struct damon_ctx *c)
{
	return damon_reclaim_handle_commit_inputs();
}

static int __init damon_reclaim_init(void)
{
	int err = damon_modules_new_paddr_ctx_target(&ctx, &target);

	if (err)
		return err;

	ctx->callback.after_wmarks_check = damon_reclaim_after_wmarks_check;
	ctx->callback.after_aggregation = damon_reclaim_after_aggregation;

	 
	if (enabled)
		err = damon_reclaim_turn(true);

	return err;
}

module_init(damon_reclaim_init);
