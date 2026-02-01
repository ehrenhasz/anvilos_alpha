
 

#include <linux/clk.h>
#include <linux/clk-provider.h>
#include <linux/clk/clk-conf.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/spinlock.h>
#include <linux/err.h>
#include <linux/list.h>
#include <linux/slab.h>
#include <linux/of.h>
#include <linux/device.h>
#include <linux/init.h>
#include <linux/pm_runtime.h>
#include <linux/sched.h>
#include <linux/clkdev.h>

#include "clk.h"

static DEFINE_SPINLOCK(enable_lock);
static DEFINE_MUTEX(prepare_lock);

static struct task_struct *prepare_owner;
static struct task_struct *enable_owner;

static int prepare_refcnt;
static int enable_refcnt;

static HLIST_HEAD(clk_root_list);
static HLIST_HEAD(clk_orphan_list);
static LIST_HEAD(clk_notifier_list);

static const struct hlist_head *all_lists[] = {
	&clk_root_list,
	&clk_orphan_list,
	NULL,
};

 

struct clk_parent_map {
	const struct clk_hw	*hw;
	struct clk_core		*core;
	const char		*fw_name;
	const char		*name;
	int			index;
};

struct clk_core {
	const char		*name;
	const struct clk_ops	*ops;
	struct clk_hw		*hw;
	struct module		*owner;
	struct device		*dev;
	struct device_node	*of_node;
	struct clk_core		*parent;
	struct clk_parent_map	*parents;
	u8			num_parents;
	u8			new_parent_index;
	unsigned long		rate;
	unsigned long		req_rate;
	unsigned long		new_rate;
	struct clk_core		*new_parent;
	struct clk_core		*new_child;
	unsigned long		flags;
	bool			orphan;
	bool			rpm_enabled;
	unsigned int		enable_count;
	unsigned int		prepare_count;
	unsigned int		protect_count;
	unsigned long		min_rate;
	unsigned long		max_rate;
	unsigned long		accuracy;
	int			phase;
	struct clk_duty		duty;
	struct hlist_head	children;
	struct hlist_node	child_node;
	struct hlist_head	clks;
	unsigned int		notifier_count;
#ifdef CONFIG_DEBUG_FS
	struct dentry		*dentry;
	struct hlist_node	debug_node;
#endif
	struct kref		ref;
};

#define CREATE_TRACE_POINTS
#include <trace/events/clk.h>

struct clk {
	struct clk_core	*core;
	struct device *dev;
	const char *dev_id;
	const char *con_id;
	unsigned long min_rate;
	unsigned long max_rate;
	unsigned int exclusive_count;
	struct hlist_node clks_node;
};

 
static int clk_pm_runtime_get(struct clk_core *core)
{
	if (!core->rpm_enabled)
		return 0;

	return pm_runtime_resume_and_get(core->dev);
}

static void clk_pm_runtime_put(struct clk_core *core)
{
	if (!core->rpm_enabled)
		return;

	pm_runtime_put_sync(core->dev);
}

 
static void clk_prepare_lock(void)
{
	if (!mutex_trylock(&prepare_lock)) {
		if (prepare_owner == current) {
			prepare_refcnt++;
			return;
		}
		mutex_lock(&prepare_lock);
	}
	WARN_ON_ONCE(prepare_owner != NULL);
	WARN_ON_ONCE(prepare_refcnt != 0);
	prepare_owner = current;
	prepare_refcnt = 1;
}

static void clk_prepare_unlock(void)
{
	WARN_ON_ONCE(prepare_owner != current);
	WARN_ON_ONCE(prepare_refcnt == 0);

	if (--prepare_refcnt)
		return;
	prepare_owner = NULL;
	mutex_unlock(&prepare_lock);
}

static unsigned long clk_enable_lock(void)
	__acquires(enable_lock)
{
	unsigned long flags;

	 
	if (!IS_ENABLED(CONFIG_SMP) ||
	    !spin_trylock_irqsave(&enable_lock, flags)) {
		if (enable_owner == current) {
			enable_refcnt++;
			__acquire(enable_lock);
			if (!IS_ENABLED(CONFIG_SMP))
				local_save_flags(flags);
			return flags;
		}
		spin_lock_irqsave(&enable_lock, flags);
	}
	WARN_ON_ONCE(enable_owner != NULL);
	WARN_ON_ONCE(enable_refcnt != 0);
	enable_owner = current;
	enable_refcnt = 1;
	return flags;
}

static void clk_enable_unlock(unsigned long flags)
	__releases(enable_lock)
{
	WARN_ON_ONCE(enable_owner != current);
	WARN_ON_ONCE(enable_refcnt == 0);

	if (--enable_refcnt) {
		__release(enable_lock);
		return;
	}
	enable_owner = NULL;
	spin_unlock_irqrestore(&enable_lock, flags);
}

static bool clk_core_rate_is_protected(struct clk_core *core)
{
	return core->protect_count;
}

static bool clk_core_is_prepared(struct clk_core *core)
{
	bool ret = false;

	 
	if (!core->ops->is_prepared)
		return core->prepare_count;

	if (!clk_pm_runtime_get(core)) {
		ret = core->ops->is_prepared(core->hw);
		clk_pm_runtime_put(core);
	}

	return ret;
}

static bool clk_core_is_enabled(struct clk_core *core)
{
	bool ret = false;

	 
	if (!core->ops->is_enabled)
		return core->enable_count;

	 
	if (core->rpm_enabled) {
		pm_runtime_get_noresume(core->dev);
		if (!pm_runtime_active(core->dev)) {
			ret = false;
			goto done;
		}
	}

	 
	if ((core->flags & CLK_OPS_PARENT_ENABLE) && core->parent)
		if (!clk_core_is_enabled(core->parent)) {
			ret = false;
			goto done;
		}

	ret = core->ops->is_enabled(core->hw);
done:
	if (core->rpm_enabled)
		pm_runtime_put(core->dev);

	return ret;
}

 

const char *__clk_get_name(const struct clk *clk)
{
	return !clk ? NULL : clk->core->name;
}
EXPORT_SYMBOL_GPL(__clk_get_name);

const char *clk_hw_get_name(const struct clk_hw *hw)
{
	return hw->core->name;
}
EXPORT_SYMBOL_GPL(clk_hw_get_name);

struct clk_hw *__clk_get_hw(struct clk *clk)
{
	return !clk ? NULL : clk->core->hw;
}
EXPORT_SYMBOL_GPL(__clk_get_hw);

unsigned int clk_hw_get_num_parents(const struct clk_hw *hw)
{
	return hw->core->num_parents;
}
EXPORT_SYMBOL_GPL(clk_hw_get_num_parents);

struct clk_hw *clk_hw_get_parent(const struct clk_hw *hw)
{
	return hw->core->parent ? hw->core->parent->hw : NULL;
}
EXPORT_SYMBOL_GPL(clk_hw_get_parent);

static struct clk_core *__clk_lookup_subtree(const char *name,
					     struct clk_core *core)
{
	struct clk_core *child;
	struct clk_core *ret;

	if (!strcmp(core->name, name))
		return core;

	hlist_for_each_entry(child, &core->children, child_node) {
		ret = __clk_lookup_subtree(name, child);
		if (ret)
			return ret;
	}

	return NULL;
}

static struct clk_core *clk_core_lookup(const char *name)
{
	struct clk_core *root_clk;
	struct clk_core *ret;

	if (!name)
		return NULL;

	 
	hlist_for_each_entry(root_clk, &clk_root_list, child_node) {
		ret = __clk_lookup_subtree(name, root_clk);
		if (ret)
			return ret;
	}

	 
	hlist_for_each_entry(root_clk, &clk_orphan_list, child_node) {
		ret = __clk_lookup_subtree(name, root_clk);
		if (ret)
			return ret;
	}

	return NULL;
}

#ifdef CONFIG_OF
static int of_parse_clkspec(const struct device_node *np, int index,
			    const char *name, struct of_phandle_args *out_args);
static struct clk_hw *
of_clk_get_hw_from_clkspec(struct of_phandle_args *clkspec);
#else
static inline int of_parse_clkspec(const struct device_node *np, int index,
				   const char *name,
				   struct of_phandle_args *out_args)
{
	return -ENOENT;
}
static inline struct clk_hw *
of_clk_get_hw_from_clkspec(struct of_phandle_args *clkspec)
{
	return ERR_PTR(-ENOENT);
}
#endif

 
static struct clk_core *clk_core_get(struct clk_core *core, u8 p_index)
{
	const char *name = core->parents[p_index].fw_name;
	int index = core->parents[p_index].index;
	struct clk_hw *hw = ERR_PTR(-ENOENT);
	struct device *dev = core->dev;
	const char *dev_id = dev ? dev_name(dev) : NULL;
	struct device_node *np = core->of_node;
	struct of_phandle_args clkspec;

	if (np && (name || index >= 0) &&
	    !of_parse_clkspec(np, index, name, &clkspec)) {
		hw = of_clk_get_hw_from_clkspec(&clkspec);
		of_node_put(clkspec.np);
	} else if (name) {
		 
		hw = clk_find_hw(dev_id, name);
	}

	if (IS_ERR(hw))
		return ERR_CAST(hw);

	return hw->core;
}

static void clk_core_fill_parent_index(struct clk_core *core, u8 index)
{
	struct clk_parent_map *entry = &core->parents[index];
	struct clk_core *parent;

	if (entry->hw) {
		parent = entry->hw->core;
	} else {
		parent = clk_core_get(core, index);
		if (PTR_ERR(parent) == -ENOENT && entry->name)
			parent = clk_core_lookup(entry->name);
	}

	 
	if (!parent)
		parent = ERR_PTR(-EPROBE_DEFER);

	 
	if (!IS_ERR(parent))
		entry->core = parent;
}

static struct clk_core *clk_core_get_parent_by_index(struct clk_core *core,
							 u8 index)
{
	if (!core || index >= core->num_parents || !core->parents)
		return NULL;

	if (!core->parents[index].core)
		clk_core_fill_parent_index(core, index);

	return core->parents[index].core;
}

struct clk_hw *
clk_hw_get_parent_by_index(const struct clk_hw *hw, unsigned int index)
{
	struct clk_core *parent;

	parent = clk_core_get_parent_by_index(hw->core, index);

	return !parent ? NULL : parent->hw;
}
EXPORT_SYMBOL_GPL(clk_hw_get_parent_by_index);

unsigned int __clk_get_enable_count(struct clk *clk)
{
	return !clk ? 0 : clk->core->enable_count;
}

static unsigned long clk_core_get_rate_nolock(struct clk_core *core)
{
	if (!core)
		return 0;

	if (!core->num_parents || core->parent)
		return core->rate;

	 
	return 0;
}

unsigned long clk_hw_get_rate(const struct clk_hw *hw)
{
	return clk_core_get_rate_nolock(hw->core);
}
EXPORT_SYMBOL_GPL(clk_hw_get_rate);

static unsigned long clk_core_get_accuracy_no_lock(struct clk_core *core)
{
	if (!core)
		return 0;

	return core->accuracy;
}

unsigned long clk_hw_get_flags(const struct clk_hw *hw)
{
	return hw->core->flags;
}
EXPORT_SYMBOL_GPL(clk_hw_get_flags);

bool clk_hw_is_prepared(const struct clk_hw *hw)
{
	return clk_core_is_prepared(hw->core);
}
EXPORT_SYMBOL_GPL(clk_hw_is_prepared);

bool clk_hw_rate_is_protected(const struct clk_hw *hw)
{
	return clk_core_rate_is_protected(hw->core);
}
EXPORT_SYMBOL_GPL(clk_hw_rate_is_protected);

bool clk_hw_is_enabled(const struct clk_hw *hw)
{
	return clk_core_is_enabled(hw->core);
}
EXPORT_SYMBOL_GPL(clk_hw_is_enabled);

bool __clk_is_enabled(struct clk *clk)
{
	if (!clk)
		return false;

	return clk_core_is_enabled(clk->core);
}
EXPORT_SYMBOL_GPL(__clk_is_enabled);

static bool mux_is_better_rate(unsigned long rate, unsigned long now,
			   unsigned long best, unsigned long flags)
{
	if (flags & CLK_MUX_ROUND_CLOSEST)
		return abs(now - rate) < abs(best - rate);

	return now <= rate && now > best;
}

static void clk_core_init_rate_req(struct clk_core * const core,
				   struct clk_rate_request *req,
				   unsigned long rate);

static int clk_core_round_rate_nolock(struct clk_core *core,
				      struct clk_rate_request *req);

static bool clk_core_has_parent(struct clk_core *core, const struct clk_core *parent)
{
	struct clk_core *tmp;
	unsigned int i;

	 
	if (core->parent == parent)
		return true;

	for (i = 0; i < core->num_parents; i++) {
		tmp = clk_core_get_parent_by_index(core, i);
		if (!tmp)
			continue;

		if (tmp == parent)
			return true;
	}

	return false;
}

static void
clk_core_forward_rate_req(struct clk_core *core,
			  const struct clk_rate_request *old_req,
			  struct clk_core *parent,
			  struct clk_rate_request *req,
			  unsigned long parent_rate)
{
	if (WARN_ON(!clk_core_has_parent(core, parent)))
		return;

	clk_core_init_rate_req(parent, req, parent_rate);

	if (req->min_rate < old_req->min_rate)
		req->min_rate = old_req->min_rate;

	if (req->max_rate > old_req->max_rate)
		req->max_rate = old_req->max_rate;
}

static int
clk_core_determine_rate_no_reparent(struct clk_hw *hw,
				    struct clk_rate_request *req)
{
	struct clk_core *core = hw->core;
	struct clk_core *parent = core->parent;
	unsigned long best;
	int ret;

	if (core->flags & CLK_SET_RATE_PARENT) {
		struct clk_rate_request parent_req;

		if (!parent) {
			req->rate = 0;
			return 0;
		}

		clk_core_forward_rate_req(core, req, parent, &parent_req,
					  req->rate);

		trace_clk_rate_request_start(&parent_req);

		ret = clk_core_round_rate_nolock(parent, &parent_req);
		if (ret)
			return ret;

		trace_clk_rate_request_done(&parent_req);

		best = parent_req.rate;
	} else if (parent) {
		best = clk_core_get_rate_nolock(parent);
	} else {
		best = clk_core_get_rate_nolock(core);
	}

	req->best_parent_rate = best;
	req->rate = best;

	return 0;
}

int clk_mux_determine_rate_flags(struct clk_hw *hw,
				 struct clk_rate_request *req,
				 unsigned long flags)
{
	struct clk_core *core = hw->core, *parent, *best_parent = NULL;
	int i, num_parents, ret;
	unsigned long best = 0;

	 
	if (core->flags & CLK_SET_RATE_NO_REPARENT)
		return clk_core_determine_rate_no_reparent(hw, req);

	 
	num_parents = core->num_parents;
	for (i = 0; i < num_parents; i++) {
		unsigned long parent_rate;

		parent = clk_core_get_parent_by_index(core, i);
		if (!parent)
			continue;

		if (core->flags & CLK_SET_RATE_PARENT) {
			struct clk_rate_request parent_req;

			clk_core_forward_rate_req(core, req, parent, &parent_req, req->rate);

			trace_clk_rate_request_start(&parent_req);

			ret = clk_core_round_rate_nolock(parent, &parent_req);
			if (ret)
				continue;

			trace_clk_rate_request_done(&parent_req);

			parent_rate = parent_req.rate;
		} else {
			parent_rate = clk_core_get_rate_nolock(parent);
		}

		if (mux_is_better_rate(req->rate, parent_rate,
				       best, flags)) {
			best_parent = parent;
			best = parent_rate;
		}
	}

	if (!best_parent)
		return -EINVAL;

	req->best_parent_hw = best_parent->hw;
	req->best_parent_rate = best;
	req->rate = best;

	return 0;
}
EXPORT_SYMBOL_GPL(clk_mux_determine_rate_flags);

struct clk *__clk_lookup(const char *name)
{
	struct clk_core *core = clk_core_lookup(name);

	return !core ? NULL : core->hw->clk;
}

static void clk_core_get_boundaries(struct clk_core *core,
				    unsigned long *min_rate,
				    unsigned long *max_rate)
{
	struct clk *clk_user;

	lockdep_assert_held(&prepare_lock);

	*min_rate = core->min_rate;
	*max_rate = core->max_rate;

	hlist_for_each_entry(clk_user, &core->clks, clks_node)
		*min_rate = max(*min_rate, clk_user->min_rate);

	hlist_for_each_entry(clk_user, &core->clks, clks_node)
		*max_rate = min(*max_rate, clk_user->max_rate);
}

 
void clk_hw_get_rate_range(struct clk_hw *hw, unsigned long *min_rate,
			   unsigned long *max_rate)
{
	clk_core_get_boundaries(hw->core, min_rate, max_rate);
}
EXPORT_SYMBOL_GPL(clk_hw_get_rate_range);

static bool clk_core_check_boundaries(struct clk_core *core,
				      unsigned long min_rate,
				      unsigned long max_rate)
{
	struct clk *user;

	lockdep_assert_held(&prepare_lock);

	if (min_rate > core->max_rate || max_rate < core->min_rate)
		return false;

	hlist_for_each_entry(user, &core->clks, clks_node)
		if (min_rate > user->max_rate || max_rate < user->min_rate)
			return false;

	return true;
}

void clk_hw_set_rate_range(struct clk_hw *hw, unsigned long min_rate,
			   unsigned long max_rate)
{
	hw->core->min_rate = min_rate;
	hw->core->max_rate = max_rate;
}
EXPORT_SYMBOL_GPL(clk_hw_set_rate_range);

 
int __clk_mux_determine_rate(struct clk_hw *hw,
			     struct clk_rate_request *req)
{
	return clk_mux_determine_rate_flags(hw, req, 0);
}
EXPORT_SYMBOL_GPL(__clk_mux_determine_rate);

int __clk_mux_determine_rate_closest(struct clk_hw *hw,
				     struct clk_rate_request *req)
{
	return clk_mux_determine_rate_flags(hw, req, CLK_MUX_ROUND_CLOSEST);
}
EXPORT_SYMBOL_GPL(__clk_mux_determine_rate_closest);

 
int clk_hw_determine_rate_no_reparent(struct clk_hw *hw,
				      struct clk_rate_request *req)
{
	return clk_core_determine_rate_no_reparent(hw, req);
}
EXPORT_SYMBOL_GPL(clk_hw_determine_rate_no_reparent);

 

static void clk_core_rate_unprotect(struct clk_core *core)
{
	lockdep_assert_held(&prepare_lock);

	if (!core)
		return;

	if (WARN(core->protect_count == 0,
	    "%s already unprotected\n", core->name))
		return;

	if (--core->protect_count > 0)
		return;

	clk_core_rate_unprotect(core->parent);
}

static int clk_core_rate_nuke_protect(struct clk_core *core)
{
	int ret;

	lockdep_assert_held(&prepare_lock);

	if (!core)
		return -EINVAL;

	if (core->protect_count == 0)
		return 0;

	ret = core->protect_count;
	core->protect_count = 1;
	clk_core_rate_unprotect(core);

	return ret;
}

 
void clk_rate_exclusive_put(struct clk *clk)
{
	if (!clk)
		return;

	clk_prepare_lock();

	 
	if (WARN_ON(clk->exclusive_count <= 0))
		goto out;

	clk_core_rate_unprotect(clk->core);
	clk->exclusive_count--;
out:
	clk_prepare_unlock();
}
EXPORT_SYMBOL_GPL(clk_rate_exclusive_put);

static void clk_core_rate_protect(struct clk_core *core)
{
	lockdep_assert_held(&prepare_lock);

	if (!core)
		return;

	if (core->protect_count == 0)
		clk_core_rate_protect(core->parent);

	core->protect_count++;
}

static void clk_core_rate_restore_protect(struct clk_core *core, int count)
{
	lockdep_assert_held(&prepare_lock);

	if (!core)
		return;

	if (count == 0)
		return;

	clk_core_rate_protect(core);
	core->protect_count = count;
}

 
int clk_rate_exclusive_get(struct clk *clk)
{
	if (!clk)
		return 0;

	clk_prepare_lock();
	clk_core_rate_protect(clk->core);
	clk->exclusive_count++;
	clk_prepare_unlock();

	return 0;
}
EXPORT_SYMBOL_GPL(clk_rate_exclusive_get);

static void clk_core_unprepare(struct clk_core *core)
{
	lockdep_assert_held(&prepare_lock);

	if (!core)
		return;

	if (WARN(core->prepare_count == 0,
	    "%s already unprepared\n", core->name))
		return;

	if (WARN(core->prepare_count == 1 && core->flags & CLK_IS_CRITICAL,
	    "Unpreparing critical %s\n", core->name))
		return;

	if (core->flags & CLK_SET_RATE_GATE)
		clk_core_rate_unprotect(core);

	if (--core->prepare_count > 0)
		return;

	WARN(core->enable_count > 0, "Unpreparing enabled %s\n", core->name);

	trace_clk_unprepare(core);

	if (core->ops->unprepare)
		core->ops->unprepare(core->hw);

	trace_clk_unprepare_complete(core);
	clk_core_unprepare(core->parent);
	clk_pm_runtime_put(core);
}

static void clk_core_unprepare_lock(struct clk_core *core)
{
	clk_prepare_lock();
	clk_core_unprepare(core);
	clk_prepare_unlock();
}

 
void clk_unprepare(struct clk *clk)
{
	if (IS_ERR_OR_NULL(clk))
		return;

	clk_core_unprepare_lock(clk->core);
}
EXPORT_SYMBOL_GPL(clk_unprepare);

static int clk_core_prepare(struct clk_core *core)
{
	int ret = 0;

	lockdep_assert_held(&prepare_lock);

	if (!core)
		return 0;

	if (core->prepare_count == 0) {
		ret = clk_pm_runtime_get(core);
		if (ret)
			return ret;

		ret = clk_core_prepare(core->parent);
		if (ret)
			goto runtime_put;

		trace_clk_prepare(core);

		if (core->ops->prepare)
			ret = core->ops->prepare(core->hw);

		trace_clk_prepare_complete(core);

		if (ret)
			goto unprepare;
	}

	core->prepare_count++;

	 
	if (core->flags & CLK_SET_RATE_GATE)
		clk_core_rate_protect(core);

	return 0;
unprepare:
	clk_core_unprepare(core->parent);
runtime_put:
	clk_pm_runtime_put(core);
	return ret;
}

static int clk_core_prepare_lock(struct clk_core *core)
{
	int ret;

	clk_prepare_lock();
	ret = clk_core_prepare(core);
	clk_prepare_unlock();

	return ret;
}

 
int clk_prepare(struct clk *clk)
{
	if (!clk)
		return 0;

	return clk_core_prepare_lock(clk->core);
}
EXPORT_SYMBOL_GPL(clk_prepare);

static void clk_core_disable(struct clk_core *core)
{
	lockdep_assert_held(&enable_lock);

	if (!core)
		return;

	if (WARN(core->enable_count == 0, "%s already disabled\n", core->name))
		return;

	if (WARN(core->enable_count == 1 && core->flags & CLK_IS_CRITICAL,
	    "Disabling critical %s\n", core->name))
		return;

	if (--core->enable_count > 0)
		return;

	trace_clk_disable(core);

	if (core->ops->disable)
		core->ops->disable(core->hw);

	trace_clk_disable_complete(core);

	clk_core_disable(core->parent);
}

static void clk_core_disable_lock(struct clk_core *core)
{
	unsigned long flags;

	flags = clk_enable_lock();
	clk_core_disable(core);
	clk_enable_unlock(flags);
}

 
void clk_disable(struct clk *clk)
{
	if (IS_ERR_OR_NULL(clk))
		return;

	clk_core_disable_lock(clk->core);
}
EXPORT_SYMBOL_GPL(clk_disable);

static int clk_core_enable(struct clk_core *core)
{
	int ret = 0;

	lockdep_assert_held(&enable_lock);

	if (!core)
		return 0;

	if (WARN(core->prepare_count == 0,
	    "Enabling unprepared %s\n", core->name))
		return -ESHUTDOWN;

	if (core->enable_count == 0) {
		ret = clk_core_enable(core->parent);

		if (ret)
			return ret;

		trace_clk_enable(core);

		if (core->ops->enable)
			ret = core->ops->enable(core->hw);

		trace_clk_enable_complete(core);

		if (ret) {
			clk_core_disable(core->parent);
			return ret;
		}
	}

	core->enable_count++;
	return 0;
}

static int clk_core_enable_lock(struct clk_core *core)
{
	unsigned long flags;
	int ret;

	flags = clk_enable_lock();
	ret = clk_core_enable(core);
	clk_enable_unlock(flags);

	return ret;
}

 
void clk_gate_restore_context(struct clk_hw *hw)
{
	struct clk_core *core = hw->core;

	if (core->enable_count)
		core->ops->enable(hw);
	else
		core->ops->disable(hw);
}
EXPORT_SYMBOL_GPL(clk_gate_restore_context);

static int clk_core_save_context(struct clk_core *core)
{
	struct clk_core *child;
	int ret = 0;

	hlist_for_each_entry(child, &core->children, child_node) {
		ret = clk_core_save_context(child);
		if (ret < 0)
			return ret;
	}

	if (core->ops && core->ops->save_context)
		ret = core->ops->save_context(core->hw);

	return ret;
}

static void clk_core_restore_context(struct clk_core *core)
{
	struct clk_core *child;

	if (core->ops && core->ops->restore_context)
		core->ops->restore_context(core->hw);

	hlist_for_each_entry(child, &core->children, child_node)
		clk_core_restore_context(child);
}

 
int clk_save_context(void)
{
	struct clk_core *clk;
	int ret;

	hlist_for_each_entry(clk, &clk_root_list, child_node) {
		ret = clk_core_save_context(clk);
		if (ret < 0)
			return ret;
	}

	hlist_for_each_entry(clk, &clk_orphan_list, child_node) {
		ret = clk_core_save_context(clk);
		if (ret < 0)
			return ret;
	}

	return 0;
}
EXPORT_SYMBOL_GPL(clk_save_context);

 
void clk_restore_context(void)
{
	struct clk_core *core;

	hlist_for_each_entry(core, &clk_root_list, child_node)
		clk_core_restore_context(core);

	hlist_for_each_entry(core, &clk_orphan_list, child_node)
		clk_core_restore_context(core);
}
EXPORT_SYMBOL_GPL(clk_restore_context);

 
int clk_enable(struct clk *clk)
{
	if (!clk)
		return 0;

	return clk_core_enable_lock(clk->core);
}
EXPORT_SYMBOL_GPL(clk_enable);

 
bool clk_is_enabled_when_prepared(struct clk *clk)
{
	return clk && !(clk->core->ops->enable && clk->core->ops->disable);
}
EXPORT_SYMBOL_GPL(clk_is_enabled_when_prepared);

static int clk_core_prepare_enable(struct clk_core *core)
{
	int ret;

	ret = clk_core_prepare_lock(core);
	if (ret)
		return ret;

	ret = clk_core_enable_lock(core);
	if (ret)
		clk_core_unprepare_lock(core);

	return ret;
}

static void clk_core_disable_unprepare(struct clk_core *core)
{
	clk_core_disable_lock(core);
	clk_core_unprepare_lock(core);
}

static void __init clk_unprepare_unused_subtree(struct clk_core *core)
{
	struct clk_core *child;

	lockdep_assert_held(&prepare_lock);

	hlist_for_each_entry(child, &core->children, child_node)
		clk_unprepare_unused_subtree(child);

	if (core->prepare_count)
		return;

	if (core->flags & CLK_IGNORE_UNUSED)
		return;

	if (clk_pm_runtime_get(core))
		return;

	if (clk_core_is_prepared(core)) {
		trace_clk_unprepare(core);
		if (core->ops->unprepare_unused)
			core->ops->unprepare_unused(core->hw);
		else if (core->ops->unprepare)
			core->ops->unprepare(core->hw);
		trace_clk_unprepare_complete(core);
	}

	clk_pm_runtime_put(core);
}

static void __init clk_disable_unused_subtree(struct clk_core *core)
{
	struct clk_core *child;
	unsigned long flags;

	lockdep_assert_held(&prepare_lock);

	hlist_for_each_entry(child, &core->children, child_node)
		clk_disable_unused_subtree(child);

	if (core->flags & CLK_OPS_PARENT_ENABLE)
		clk_core_prepare_enable(core->parent);

	if (clk_pm_runtime_get(core))
		goto unprepare_out;

	flags = clk_enable_lock();

	if (core->enable_count)
		goto unlock_out;

	if (core->flags & CLK_IGNORE_UNUSED)
		goto unlock_out;

	 
	if (clk_core_is_enabled(core)) {
		trace_clk_disable(core);
		if (core->ops->disable_unused)
			core->ops->disable_unused(core->hw);
		else if (core->ops->disable)
			core->ops->disable(core->hw);
		trace_clk_disable_complete(core);
	}

unlock_out:
	clk_enable_unlock(flags);
	clk_pm_runtime_put(core);
unprepare_out:
	if (core->flags & CLK_OPS_PARENT_ENABLE)
		clk_core_disable_unprepare(core->parent);
}

static bool clk_ignore_unused __initdata;
static int __init clk_ignore_unused_setup(char *__unused)
{
	clk_ignore_unused = true;
	return 1;
}
__setup("clk_ignore_unused", clk_ignore_unused_setup);

static int __init clk_disable_unused(void)
{
	struct clk_core *core;

	if (clk_ignore_unused) {
		pr_warn("clk: Not disabling unused clocks\n");
		return 0;
	}

	pr_info("clk: Disabling unused clocks\n");

	clk_prepare_lock();

	hlist_for_each_entry(core, &clk_root_list, child_node)
		clk_disable_unused_subtree(core);

	hlist_for_each_entry(core, &clk_orphan_list, child_node)
		clk_disable_unused_subtree(core);

	hlist_for_each_entry(core, &clk_root_list, child_node)
		clk_unprepare_unused_subtree(core);

	hlist_for_each_entry(core, &clk_orphan_list, child_node)
		clk_unprepare_unused_subtree(core);

	clk_prepare_unlock();

	return 0;
}
late_initcall_sync(clk_disable_unused);

static int clk_core_determine_round_nolock(struct clk_core *core,
					   struct clk_rate_request *req)
{
	long rate;

	lockdep_assert_held(&prepare_lock);

	if (!core)
		return 0;

	 
	if (!req->min_rate && !req->max_rate)
		pr_warn("%s: %s: clk_rate_request has initialized min or max rate.\n",
			__func__, core->name);
	else
		req->rate = clamp(req->rate, req->min_rate, req->max_rate);

	 
	if (clk_core_rate_is_protected(core)) {
		req->rate = core->rate;
	} else if (core->ops->determine_rate) {
		return core->ops->determine_rate(core->hw, req);
	} else if (core->ops->round_rate) {
		rate = core->ops->round_rate(core->hw, req->rate,
					     &req->best_parent_rate);
		if (rate < 0)
			return rate;

		req->rate = rate;
	} else {
		return -EINVAL;
	}

	return 0;
}

static void clk_core_init_rate_req(struct clk_core * const core,
				   struct clk_rate_request *req,
				   unsigned long rate)
{
	struct clk_core *parent;

	if (WARN_ON(!req))
		return;

	memset(req, 0, sizeof(*req));
	req->max_rate = ULONG_MAX;

	if (!core)
		return;

	req->core = core;
	req->rate = rate;
	clk_core_get_boundaries(core, &req->min_rate, &req->max_rate);

	parent = core->parent;
	if (parent) {
		req->best_parent_hw = parent->hw;
		req->best_parent_rate = parent->rate;
	} else {
		req->best_parent_hw = NULL;
		req->best_parent_rate = 0;
	}
}

 
void clk_hw_init_rate_request(const struct clk_hw *hw,
			      struct clk_rate_request *req,
			      unsigned long rate)
{
	if (WARN_ON(!hw || !req))
		return;

	clk_core_init_rate_req(hw->core, req, rate);
}
EXPORT_SYMBOL_GPL(clk_hw_init_rate_request);

 
void clk_hw_forward_rate_request(const struct clk_hw *hw,
				 const struct clk_rate_request *old_req,
				 const struct clk_hw *parent,
				 struct clk_rate_request *req,
				 unsigned long parent_rate)
{
	if (WARN_ON(!hw || !old_req || !parent || !req))
		return;

	clk_core_forward_rate_req(hw->core, old_req,
				  parent->core, req,
				  parent_rate);
}
EXPORT_SYMBOL_GPL(clk_hw_forward_rate_request);

static bool clk_core_can_round(struct clk_core * const core)
{
	return core->ops->determine_rate || core->ops->round_rate;
}

static int clk_core_round_rate_nolock(struct clk_core *core,
				      struct clk_rate_request *req)
{
	int ret;

	lockdep_assert_held(&prepare_lock);

	if (!core) {
		req->rate = 0;
		return 0;
	}

	if (clk_core_can_round(core))
		return clk_core_determine_round_nolock(core, req);

	if (core->flags & CLK_SET_RATE_PARENT) {
		struct clk_rate_request parent_req;

		clk_core_forward_rate_req(core, req, core->parent, &parent_req, req->rate);

		trace_clk_rate_request_start(&parent_req);

		ret = clk_core_round_rate_nolock(core->parent, &parent_req);
		if (ret)
			return ret;

		trace_clk_rate_request_done(&parent_req);

		req->best_parent_rate = parent_req.rate;
		req->rate = parent_req.rate;

		return 0;
	}

	req->rate = core->rate;
	return 0;
}

 
int __clk_determine_rate(struct clk_hw *hw, struct clk_rate_request *req)
{
	if (!hw) {
		req->rate = 0;
		return 0;
	}

	return clk_core_round_rate_nolock(hw->core, req);
}
EXPORT_SYMBOL_GPL(__clk_determine_rate);

 
unsigned long clk_hw_round_rate(struct clk_hw *hw, unsigned long rate)
{
	int ret;
	struct clk_rate_request req;

	clk_core_init_rate_req(hw->core, &req, rate);

	trace_clk_rate_request_start(&req);

	ret = clk_core_round_rate_nolock(hw->core, &req);
	if (ret)
		return 0;

	trace_clk_rate_request_done(&req);

	return req.rate;
}
EXPORT_SYMBOL_GPL(clk_hw_round_rate);

 
long clk_round_rate(struct clk *clk, unsigned long rate)
{
	struct clk_rate_request req;
	int ret;

	if (!clk)
		return 0;

	clk_prepare_lock();

	if (clk->exclusive_count)
		clk_core_rate_unprotect(clk->core);

	clk_core_init_rate_req(clk->core, &req, rate);

	trace_clk_rate_request_start(&req);

	ret = clk_core_round_rate_nolock(clk->core, &req);

	trace_clk_rate_request_done(&req);

	if (clk->exclusive_count)
		clk_core_rate_protect(clk->core);

	clk_prepare_unlock();

	if (ret)
		return ret;

	return req.rate;
}
EXPORT_SYMBOL_GPL(clk_round_rate);

 
static int __clk_notify(struct clk_core *core, unsigned long msg,
		unsigned long old_rate, unsigned long new_rate)
{
	struct clk_notifier *cn;
	struct clk_notifier_data cnd;
	int ret = NOTIFY_DONE;

	cnd.old_rate = old_rate;
	cnd.new_rate = new_rate;

	list_for_each_entry(cn, &clk_notifier_list, node) {
		if (cn->clk->core == core) {
			cnd.clk = cn->clk;
			ret = srcu_notifier_call_chain(&cn->notifier_head, msg,
					&cnd);
			if (ret & NOTIFY_STOP_MASK)
				return ret;
		}
	}

	return ret;
}

 
static void __clk_recalc_accuracies(struct clk_core *core)
{
	unsigned long parent_accuracy = 0;
	struct clk_core *child;

	lockdep_assert_held(&prepare_lock);

	if (core->parent)
		parent_accuracy = core->parent->accuracy;

	if (core->ops->recalc_accuracy)
		core->accuracy = core->ops->recalc_accuracy(core->hw,
							  parent_accuracy);
	else
		core->accuracy = parent_accuracy;

	hlist_for_each_entry(child, &core->children, child_node)
		__clk_recalc_accuracies(child);
}

static long clk_core_get_accuracy_recalc(struct clk_core *core)
{
	if (core && (core->flags & CLK_GET_ACCURACY_NOCACHE))
		__clk_recalc_accuracies(core);

	return clk_core_get_accuracy_no_lock(core);
}

 
long clk_get_accuracy(struct clk *clk)
{
	long accuracy;

	if (!clk)
		return 0;

	clk_prepare_lock();
	accuracy = clk_core_get_accuracy_recalc(clk->core);
	clk_prepare_unlock();

	return accuracy;
}
EXPORT_SYMBOL_GPL(clk_get_accuracy);

static unsigned long clk_recalc(struct clk_core *core,
				unsigned long parent_rate)
{
	unsigned long rate = parent_rate;

	if (core->ops->recalc_rate && !clk_pm_runtime_get(core)) {
		rate = core->ops->recalc_rate(core->hw, parent_rate);
		clk_pm_runtime_put(core);
	}
	return rate;
}

 
static void __clk_recalc_rates(struct clk_core *core, bool update_req,
			       unsigned long msg)
{
	unsigned long old_rate;
	unsigned long parent_rate = 0;
	struct clk_core *child;

	lockdep_assert_held(&prepare_lock);

	old_rate = core->rate;

	if (core->parent)
		parent_rate = core->parent->rate;

	core->rate = clk_recalc(core, parent_rate);
	if (update_req)
		core->req_rate = core->rate;

	 
	if (core->notifier_count && msg)
		__clk_notify(core, msg, old_rate, core->rate);

	hlist_for_each_entry(child, &core->children, child_node)
		__clk_recalc_rates(child, update_req, msg);
}

static unsigned long clk_core_get_rate_recalc(struct clk_core *core)
{
	if (core && (core->flags & CLK_GET_RATE_NOCACHE))
		__clk_recalc_rates(core, false, 0);

	return clk_core_get_rate_nolock(core);
}

 
unsigned long clk_get_rate(struct clk *clk)
{
	unsigned long rate;

	if (!clk)
		return 0;

	clk_prepare_lock();
	rate = clk_core_get_rate_recalc(clk->core);
	clk_prepare_unlock();

	return rate;
}
EXPORT_SYMBOL_GPL(clk_get_rate);

static int clk_fetch_parent_index(struct clk_core *core,
				  struct clk_core *parent)
{
	int i;

	if (!parent)
		return -EINVAL;

	for (i = 0; i < core->num_parents; i++) {
		 
		if (core->parents[i].core == parent)
			return i;

		 
		if (core->parents[i].core)
			continue;

		 
		if (core->parents[i].hw) {
			if (core->parents[i].hw == parent->hw)
				break;

			 
			continue;
		}

		 
		if (parent == clk_core_get(core, i))
			break;

		 
		if (core->parents[i].name &&
		    !strcmp(parent->name, core->parents[i].name))
			break;
	}

	if (i == core->num_parents)
		return -EINVAL;

	core->parents[i].core = parent;
	return i;
}

 
int clk_hw_get_parent_index(struct clk_hw *hw)
{
	struct clk_hw *parent = clk_hw_get_parent(hw);

	if (WARN_ON(parent == NULL))
		return -EINVAL;

	return clk_fetch_parent_index(hw->core, parent->core);
}
EXPORT_SYMBOL_GPL(clk_hw_get_parent_index);

 
static void clk_core_update_orphan_status(struct clk_core *core, bool is_orphan)
{
	struct clk_core *child;

	core->orphan = is_orphan;

	hlist_for_each_entry(child, &core->children, child_node)
		clk_core_update_orphan_status(child, is_orphan);
}

static void clk_reparent(struct clk_core *core, struct clk_core *new_parent)
{
	bool was_orphan = core->orphan;

	hlist_del(&core->child_node);

	if (new_parent) {
		bool becomes_orphan = new_parent->orphan;

		 
		if (new_parent->new_child == core)
			new_parent->new_child = NULL;

		hlist_add_head(&core->child_node, &new_parent->children);

		if (was_orphan != becomes_orphan)
			clk_core_update_orphan_status(core, becomes_orphan);
	} else {
		hlist_add_head(&core->child_node, &clk_orphan_list);
		if (!was_orphan)
			clk_core_update_orphan_status(core, true);
	}

	core->parent = new_parent;
}

static struct clk_core *__clk_set_parent_before(struct clk_core *core,
					   struct clk_core *parent)
{
	unsigned long flags;
	struct clk_core *old_parent = core->parent;

	 

	 
	if (core->flags & CLK_OPS_PARENT_ENABLE) {
		clk_core_prepare_enable(old_parent);
		clk_core_prepare_enable(parent);
	}

	 
	if (core->prepare_count) {
		clk_core_prepare_enable(parent);
		clk_core_enable_lock(core);
	}

	 
	flags = clk_enable_lock();
	clk_reparent(core, parent);
	clk_enable_unlock(flags);

	return old_parent;
}

static void __clk_set_parent_after(struct clk_core *core,
				   struct clk_core *parent,
				   struct clk_core *old_parent)
{
	 
	if (core->prepare_count) {
		clk_core_disable_lock(core);
		clk_core_disable_unprepare(old_parent);
	}

	 
	if (core->flags & CLK_OPS_PARENT_ENABLE) {
		clk_core_disable_unprepare(parent);
		clk_core_disable_unprepare(old_parent);
	}
}

static int __clk_set_parent(struct clk_core *core, struct clk_core *parent,
			    u8 p_index)
{
	unsigned long flags;
	int ret = 0;
	struct clk_core *old_parent;

	old_parent = __clk_set_parent_before(core, parent);

	trace_clk_set_parent(core, parent);

	 
	if (parent && core->ops->set_parent)
		ret = core->ops->set_parent(core->hw, p_index);

	trace_clk_set_parent_complete(core, parent);

	if (ret) {
		flags = clk_enable_lock();
		clk_reparent(core, old_parent);
		clk_enable_unlock(flags);

		__clk_set_parent_after(core, old_parent, parent);

		return ret;
	}

	__clk_set_parent_after(core, parent, old_parent);

	return 0;
}

 
static int __clk_speculate_rates(struct clk_core *core,
				 unsigned long parent_rate)
{
	struct clk_core *child;
	unsigned long new_rate;
	int ret = NOTIFY_DONE;

	lockdep_assert_held(&prepare_lock);

	new_rate = clk_recalc(core, parent_rate);

	 
	if (core->notifier_count)
		ret = __clk_notify(core, PRE_RATE_CHANGE, core->rate, new_rate);

	if (ret & NOTIFY_STOP_MASK) {
		pr_debug("%s: clk notifier callback for clock %s aborted with error %d\n",
				__func__, core->name, ret);
		goto out;
	}

	hlist_for_each_entry(child, &core->children, child_node) {
		ret = __clk_speculate_rates(child, new_rate);
		if (ret & NOTIFY_STOP_MASK)
			break;
	}

out:
	return ret;
}

static void clk_calc_subtree(struct clk_core *core, unsigned long new_rate,
			     struct clk_core *new_parent, u8 p_index)
{
	struct clk_core *child;

	core->new_rate = new_rate;
	core->new_parent = new_parent;
	core->new_parent_index = p_index;
	 
	core->new_child = NULL;
	if (new_parent && new_parent != core->parent)
		new_parent->new_child = core;

	hlist_for_each_entry(child, &core->children, child_node) {
		child->new_rate = clk_recalc(child, new_rate);
		clk_calc_subtree(child, child->new_rate, NULL, 0);
	}
}

 
static struct clk_core *clk_calc_new_rates(struct clk_core *core,
					   unsigned long rate)
{
	struct clk_core *top = core;
	struct clk_core *old_parent, *parent;
	unsigned long best_parent_rate = 0;
	unsigned long new_rate;
	unsigned long min_rate;
	unsigned long max_rate;
	int p_index = 0;
	long ret;

	 
	if (IS_ERR_OR_NULL(core))
		return NULL;

	 
	parent = old_parent = core->parent;
	if (parent)
		best_parent_rate = parent->rate;

	clk_core_get_boundaries(core, &min_rate, &max_rate);

	 
	if (clk_core_can_round(core)) {
		struct clk_rate_request req;

		clk_core_init_rate_req(core, &req, rate);

		trace_clk_rate_request_start(&req);

		ret = clk_core_determine_round_nolock(core, &req);
		if (ret < 0)
			return NULL;

		trace_clk_rate_request_done(&req);

		best_parent_rate = req.best_parent_rate;
		new_rate = req.rate;
		parent = req.best_parent_hw ? req.best_parent_hw->core : NULL;

		if (new_rate < min_rate || new_rate > max_rate)
			return NULL;
	} else if (!parent || !(core->flags & CLK_SET_RATE_PARENT)) {
		 
		core->new_rate = core->rate;
		return NULL;
	} else {
		 
		top = clk_calc_new_rates(parent, rate);
		new_rate = parent->new_rate;
		goto out;
	}

	 
	if (parent != old_parent &&
	    (core->flags & CLK_SET_PARENT_GATE) && core->prepare_count) {
		pr_debug("%s: %s not gated but wants to reparent\n",
			 __func__, core->name);
		return NULL;
	}

	 
	if (parent && core->num_parents > 1) {
		p_index = clk_fetch_parent_index(core, parent);
		if (p_index < 0) {
			pr_debug("%s: clk %s can not be parent of clk %s\n",
				 __func__, parent->name, core->name);
			return NULL;
		}
	}

	if ((core->flags & CLK_SET_RATE_PARENT) && parent &&
	    best_parent_rate != parent->rate)
		top = clk_calc_new_rates(parent, best_parent_rate);

out:
	clk_calc_subtree(core, new_rate, parent, p_index);

	return top;
}

 
static struct clk_core *clk_propagate_rate_change(struct clk_core *core,
						  unsigned long event)
{
	struct clk_core *child, *tmp_clk, *fail_clk = NULL;
	int ret = NOTIFY_DONE;

	if (core->rate == core->new_rate)
		return NULL;

	if (core->notifier_count) {
		ret = __clk_notify(core, event, core->rate, core->new_rate);
		if (ret & NOTIFY_STOP_MASK)
			fail_clk = core;
	}

	hlist_for_each_entry(child, &core->children, child_node) {
		 
		if (child->new_parent && child->new_parent != core)
			continue;
		tmp_clk = clk_propagate_rate_change(child, event);
		if (tmp_clk)
			fail_clk = tmp_clk;
	}

	 
	if (core->new_child) {
		tmp_clk = clk_propagate_rate_change(core->new_child, event);
		if (tmp_clk)
			fail_clk = tmp_clk;
	}

	return fail_clk;
}

 
static void clk_change_rate(struct clk_core *core)
{
	struct clk_core *child;
	struct hlist_node *tmp;
	unsigned long old_rate;
	unsigned long best_parent_rate = 0;
	bool skip_set_rate = false;
	struct clk_core *old_parent;
	struct clk_core *parent = NULL;

	old_rate = core->rate;

	if (core->new_parent) {
		parent = core->new_parent;
		best_parent_rate = core->new_parent->rate;
	} else if (core->parent) {
		parent = core->parent;
		best_parent_rate = core->parent->rate;
	}

	if (clk_pm_runtime_get(core))
		return;

	if (core->flags & CLK_SET_RATE_UNGATE) {
		clk_core_prepare(core);
		clk_core_enable_lock(core);
	}

	if (core->new_parent && core->new_parent != core->parent) {
		old_parent = __clk_set_parent_before(core, core->new_parent);
		trace_clk_set_parent(core, core->new_parent);

		if (core->ops->set_rate_and_parent) {
			skip_set_rate = true;
			core->ops->set_rate_and_parent(core->hw, core->new_rate,
					best_parent_rate,
					core->new_parent_index);
		} else if (core->ops->set_parent) {
			core->ops->set_parent(core->hw, core->new_parent_index);
		}

		trace_clk_set_parent_complete(core, core->new_parent);
		__clk_set_parent_after(core, core->new_parent, old_parent);
	}

	if (core->flags & CLK_OPS_PARENT_ENABLE)
		clk_core_prepare_enable(parent);

	trace_clk_set_rate(core, core->new_rate);

	if (!skip_set_rate && core->ops->set_rate)
		core->ops->set_rate(core->hw, core->new_rate, best_parent_rate);

	trace_clk_set_rate_complete(core, core->new_rate);

	core->rate = clk_recalc(core, best_parent_rate);

	if (core->flags & CLK_SET_RATE_UNGATE) {
		clk_core_disable_lock(core);
		clk_core_unprepare(core);
	}

	if (core->flags & CLK_OPS_PARENT_ENABLE)
		clk_core_disable_unprepare(parent);

	if (core->notifier_count && old_rate != core->rate)
		__clk_notify(core, POST_RATE_CHANGE, old_rate, core->rate);

	if (core->flags & CLK_RECALC_NEW_RATES)
		(void)clk_calc_new_rates(core, core->new_rate);

	 
	hlist_for_each_entry_safe(child, tmp, &core->children, child_node) {
		 
		if (child->new_parent && child->new_parent != core)
			continue;
		clk_change_rate(child);
	}

	 
	if (core->new_child)
		clk_change_rate(core->new_child);

	clk_pm_runtime_put(core);
}

static unsigned long clk_core_req_round_rate_nolock(struct clk_core *core,
						     unsigned long req_rate)
{
	int ret, cnt;
	struct clk_rate_request req;

	lockdep_assert_held(&prepare_lock);

	if (!core)
		return 0;

	 
	cnt = clk_core_rate_nuke_protect(core);
	if (cnt < 0)
		return cnt;

	clk_core_init_rate_req(core, &req, req_rate);

	trace_clk_rate_request_start(&req);

	ret = clk_core_round_rate_nolock(core, &req);

	trace_clk_rate_request_done(&req);

	 
	clk_core_rate_restore_protect(core, cnt);

	return ret ? 0 : req.rate;
}

static int clk_core_set_rate_nolock(struct clk_core *core,
				    unsigned long req_rate)
{
	struct clk_core *top, *fail_clk;
	unsigned long rate;
	int ret;

	if (!core)
		return 0;

	rate = clk_core_req_round_rate_nolock(core, req_rate);

	 
	if (rate == clk_core_get_rate_nolock(core))
		return 0;

	 
	if (clk_core_rate_is_protected(core))
		return -EBUSY;

	 
	top = clk_calc_new_rates(core, req_rate);
	if (!top)
		return -EINVAL;

	ret = clk_pm_runtime_get(core);
	if (ret)
		return ret;

	 
	fail_clk = clk_propagate_rate_change(top, PRE_RATE_CHANGE);
	if (fail_clk) {
		pr_debug("%s: failed to set %s rate\n", __func__,
				fail_clk->name);
		clk_propagate_rate_change(top, ABORT_RATE_CHANGE);
		ret = -EBUSY;
		goto err;
	}

	 
	clk_change_rate(top);

	core->req_rate = req_rate;
err:
	clk_pm_runtime_put(core);

	return ret;
}

 
int clk_set_rate(struct clk *clk, unsigned long rate)
{
	int ret;

	if (!clk)
		return 0;

	 
	clk_prepare_lock();

	if (clk->exclusive_count)
		clk_core_rate_unprotect(clk->core);

	ret = clk_core_set_rate_nolock(clk->core, rate);

	if (clk->exclusive_count)
		clk_core_rate_protect(clk->core);

	clk_prepare_unlock();

	return ret;
}
EXPORT_SYMBOL_GPL(clk_set_rate);

 
int clk_set_rate_exclusive(struct clk *clk, unsigned long rate)
{
	int ret;

	if (!clk)
		return 0;

	 
	clk_prepare_lock();

	 

	ret = clk_core_set_rate_nolock(clk->core, rate);
	if (!ret) {
		clk_core_rate_protect(clk->core);
		clk->exclusive_count++;
	}

	clk_prepare_unlock();

	return ret;
}
EXPORT_SYMBOL_GPL(clk_set_rate_exclusive);

static int clk_set_rate_range_nolock(struct clk *clk,
				     unsigned long min,
				     unsigned long max)
{
	int ret = 0;
	unsigned long old_min, old_max, rate;

	lockdep_assert_held(&prepare_lock);

	if (!clk)
		return 0;

	trace_clk_set_rate_range(clk->core, min, max);

	if (min > max) {
		pr_err("%s: clk %s dev %s con %s: invalid range [%lu, %lu]\n",
		       __func__, clk->core->name, clk->dev_id, clk->con_id,
		       min, max);
		return -EINVAL;
	}

	if (clk->exclusive_count)
		clk_core_rate_unprotect(clk->core);

	 
	old_min = clk->min_rate;
	old_max = clk->max_rate;
	clk->min_rate = min;
	clk->max_rate = max;

	if (!clk_core_check_boundaries(clk->core, min, max)) {
		ret = -EINVAL;
		goto out;
	}

	rate = clk->core->req_rate;
	if (clk->core->flags & CLK_GET_RATE_NOCACHE)
		rate = clk_core_get_rate_recalc(clk->core);

	 
	rate = clamp(rate, min, max);
	ret = clk_core_set_rate_nolock(clk->core, rate);
	if (ret) {
		 
		clk->min_rate = old_min;
		clk->max_rate = old_max;
	}

out:
	if (clk->exclusive_count)
		clk_core_rate_protect(clk->core);

	return ret;
}

 
int clk_set_rate_range(struct clk *clk, unsigned long min, unsigned long max)
{
	int ret;

	if (!clk)
		return 0;

	clk_prepare_lock();

	ret = clk_set_rate_range_nolock(clk, min, max);

	clk_prepare_unlock();

	return ret;
}
EXPORT_SYMBOL_GPL(clk_set_rate_range);

 
int clk_set_min_rate(struct clk *clk, unsigned long rate)
{
	if (!clk)
		return 0;

	trace_clk_set_min_rate(clk->core, rate);

	return clk_set_rate_range(clk, rate, clk->max_rate);
}
EXPORT_SYMBOL_GPL(clk_set_min_rate);

 
int clk_set_max_rate(struct clk *clk, unsigned long rate)
{
	if (!clk)
		return 0;

	trace_clk_set_max_rate(clk->core, rate);

	return clk_set_rate_range(clk, clk->min_rate, rate);
}
EXPORT_SYMBOL_GPL(clk_set_max_rate);

 
struct clk *clk_get_parent(struct clk *clk)
{
	struct clk *parent;

	if (!clk)
		return NULL;

	clk_prepare_lock();
	 
	parent = !clk->core->parent ? NULL : clk->core->parent->hw->clk;
	clk_prepare_unlock();

	return parent;
}
EXPORT_SYMBOL_GPL(clk_get_parent);

static struct clk_core *__clk_init_parent(struct clk_core *core)
{
	u8 index = 0;

	if (core->num_parents > 1 && core->ops->get_parent)
		index = core->ops->get_parent(core->hw);

	return clk_core_get_parent_by_index(core, index);
}

static void clk_core_reparent(struct clk_core *core,
				  struct clk_core *new_parent)
{
	clk_reparent(core, new_parent);
	__clk_recalc_accuracies(core);
	__clk_recalc_rates(core, true, POST_RATE_CHANGE);
}

void clk_hw_reparent(struct clk_hw *hw, struct clk_hw *new_parent)
{
	if (!hw)
		return;

	clk_core_reparent(hw->core, !new_parent ? NULL : new_parent->core);
}

 
bool clk_has_parent(const struct clk *clk, const struct clk *parent)
{
	 
	if (!clk || !parent)
		return true;

	return clk_core_has_parent(clk->core, parent->core);
}
EXPORT_SYMBOL_GPL(clk_has_parent);

static int clk_core_set_parent_nolock(struct clk_core *core,
				      struct clk_core *parent)
{
	int ret = 0;
	int p_index = 0;
	unsigned long p_rate = 0;

	lockdep_assert_held(&prepare_lock);

	if (!core)
		return 0;

	if (core->parent == parent)
		return 0;

	 
	if (core->num_parents > 1 && !core->ops->set_parent)
		return -EPERM;

	 
	if ((core->flags & CLK_SET_PARENT_GATE) && core->prepare_count)
		return -EBUSY;

	if (clk_core_rate_is_protected(core))
		return -EBUSY;

	 
	if (parent) {
		p_index = clk_fetch_parent_index(core, parent);
		if (p_index < 0) {
			pr_debug("%s: clk %s can not be parent of clk %s\n",
					__func__, parent->name, core->name);
			return p_index;
		}
		p_rate = parent->rate;
	}

	ret = clk_pm_runtime_get(core);
	if (ret)
		return ret;

	 
	ret = __clk_speculate_rates(core, p_rate);

	 
	if (ret & NOTIFY_STOP_MASK)
		goto runtime_put;

	 
	ret = __clk_set_parent(core, parent, p_index);

	 
	if (ret) {
		__clk_recalc_rates(core, true, ABORT_RATE_CHANGE);
	} else {
		__clk_recalc_rates(core, true, POST_RATE_CHANGE);
		__clk_recalc_accuracies(core);
	}

runtime_put:
	clk_pm_runtime_put(core);

	return ret;
}

int clk_hw_set_parent(struct clk_hw *hw, struct clk_hw *parent)
{
	return clk_core_set_parent_nolock(hw->core, parent->core);
}
EXPORT_SYMBOL_GPL(clk_hw_set_parent);

 
int clk_set_parent(struct clk *clk, struct clk *parent)
{
	int ret;

	if (!clk)
		return 0;

	clk_prepare_lock();

	if (clk->exclusive_count)
		clk_core_rate_unprotect(clk->core);

	ret = clk_core_set_parent_nolock(clk->core,
					 parent ? parent->core : NULL);

	if (clk->exclusive_count)
		clk_core_rate_protect(clk->core);

	clk_prepare_unlock();

	return ret;
}
EXPORT_SYMBOL_GPL(clk_set_parent);

static int clk_core_set_phase_nolock(struct clk_core *core, int degrees)
{
	int ret = -EINVAL;

	lockdep_assert_held(&prepare_lock);

	if (!core)
		return 0;

	if (clk_core_rate_is_protected(core))
		return -EBUSY;

	trace_clk_set_phase(core, degrees);

	if (core->ops->set_phase) {
		ret = core->ops->set_phase(core->hw, degrees);
		if (!ret)
			core->phase = degrees;
	}

	trace_clk_set_phase_complete(core, degrees);

	return ret;
}

 
int clk_set_phase(struct clk *clk, int degrees)
{
	int ret;

	if (!clk)
		return 0;

	 
	degrees %= 360;
	if (degrees < 0)
		degrees += 360;

	clk_prepare_lock();

	if (clk->exclusive_count)
		clk_core_rate_unprotect(clk->core);

	ret = clk_core_set_phase_nolock(clk->core, degrees);

	if (clk->exclusive_count)
		clk_core_rate_protect(clk->core);

	clk_prepare_unlock();

	return ret;
}
EXPORT_SYMBOL_GPL(clk_set_phase);

static int clk_core_get_phase(struct clk_core *core)
{
	int ret;

	lockdep_assert_held(&prepare_lock);
	if (!core->ops->get_phase)
		return 0;

	 
	ret = core->ops->get_phase(core->hw);
	if (ret >= 0)
		core->phase = ret;

	return ret;
}

 
int clk_get_phase(struct clk *clk)
{
	int ret;

	if (!clk)
		return 0;

	clk_prepare_lock();
	ret = clk_core_get_phase(clk->core);
	clk_prepare_unlock();

	return ret;
}
EXPORT_SYMBOL_GPL(clk_get_phase);

static void clk_core_reset_duty_cycle_nolock(struct clk_core *core)
{
	 
	core->duty.num = 1;
	core->duty.den = 2;
}

static int clk_core_update_duty_cycle_parent_nolock(struct clk_core *core);

static int clk_core_update_duty_cycle_nolock(struct clk_core *core)
{
	struct clk_duty *duty = &core->duty;
	int ret = 0;

	if (!core->ops->get_duty_cycle)
		return clk_core_update_duty_cycle_parent_nolock(core);

	ret = core->ops->get_duty_cycle(core->hw, duty);
	if (ret)
		goto reset;

	 
	if (duty->den == 0 || duty->num > duty->den) {
		ret = -EINVAL;
		goto reset;
	}

	return 0;

reset:
	clk_core_reset_duty_cycle_nolock(core);
	return ret;
}

static int clk_core_update_duty_cycle_parent_nolock(struct clk_core *core)
{
	int ret = 0;

	if (core->parent &&
	    core->flags & CLK_DUTY_CYCLE_PARENT) {
		ret = clk_core_update_duty_cycle_nolock(core->parent);
		memcpy(&core->duty, &core->parent->duty, sizeof(core->duty));
	} else {
		clk_core_reset_duty_cycle_nolock(core);
	}

	return ret;
}

static int clk_core_set_duty_cycle_parent_nolock(struct clk_core *core,
						 struct clk_duty *duty);

static int clk_core_set_duty_cycle_nolock(struct clk_core *core,
					  struct clk_duty *duty)
{
	int ret;

	lockdep_assert_held(&prepare_lock);

	if (clk_core_rate_is_protected(core))
		return -EBUSY;

	trace_clk_set_duty_cycle(core, duty);

	if (!core->ops->set_duty_cycle)
		return clk_core_set_duty_cycle_parent_nolock(core, duty);

	ret = core->ops->set_duty_cycle(core->hw, duty);
	if (!ret)
		memcpy(&core->duty, duty, sizeof(*duty));

	trace_clk_set_duty_cycle_complete(core, duty);

	return ret;
}

static int clk_core_set_duty_cycle_parent_nolock(struct clk_core *core,
						 struct clk_duty *duty)
{
	int ret = 0;

	if (core->parent &&
	    core->flags & (CLK_DUTY_CYCLE_PARENT | CLK_SET_RATE_PARENT)) {
		ret = clk_core_set_duty_cycle_nolock(core->parent, duty);
		memcpy(&core->duty, &core->parent->duty, sizeof(core->duty));
	}

	return ret;
}

 
int clk_set_duty_cycle(struct clk *clk, unsigned int num, unsigned int den)
{
	int ret;
	struct clk_duty duty;

	if (!clk)
		return 0;

	 
	if (den == 0 || num > den)
		return -EINVAL;

	duty.num = num;
	duty.den = den;

	clk_prepare_lock();

	if (clk->exclusive_count)
		clk_core_rate_unprotect(clk->core);

	ret = clk_core_set_duty_cycle_nolock(clk->core, &duty);

	if (clk->exclusive_count)
		clk_core_rate_protect(clk->core);

	clk_prepare_unlock();

	return ret;
}
EXPORT_SYMBOL_GPL(clk_set_duty_cycle);

static int clk_core_get_scaled_duty_cycle(struct clk_core *core,
					  unsigned int scale)
{
	struct clk_duty *duty = &core->duty;
	int ret;

	clk_prepare_lock();

	ret = clk_core_update_duty_cycle_nolock(core);
	if (!ret)
		ret = mult_frac(scale, duty->num, duty->den);

	clk_prepare_unlock();

	return ret;
}

 
int clk_get_scaled_duty_cycle(struct clk *clk, unsigned int scale)
{
	if (!clk)
		return 0;

	return clk_core_get_scaled_duty_cycle(clk->core, scale);
}
EXPORT_SYMBOL_GPL(clk_get_scaled_duty_cycle);

 
bool clk_is_match(const struct clk *p, const struct clk *q)
{
	 
	if (p == q)
		return true;

	 
	if (!IS_ERR_OR_NULL(p) && !IS_ERR_OR_NULL(q))
		if (p->core == q->core)
			return true;

	return false;
}
EXPORT_SYMBOL_GPL(clk_is_match);

 

#ifdef CONFIG_DEBUG_FS
#include <linux/debugfs.h>

static struct dentry *rootdir;
static int inited = 0;
static DEFINE_MUTEX(clk_debug_lock);
static HLIST_HEAD(clk_debug_list);

static struct hlist_head *orphan_list[] = {
	&clk_orphan_list,
	NULL,
};

static void clk_summary_show_one(struct seq_file *s, struct clk_core *c,
				 int level)
{
	int phase;

	seq_printf(s, "%*s%-*s %7d %8d %8d %11lu %10lu ",
		   level * 3 + 1, "",
		   30 - level * 3, c->name,
		   c->enable_count, c->prepare_count, c->protect_count,
		   clk_core_get_rate_recalc(c),
		   clk_core_get_accuracy_recalc(c));

	phase = clk_core_get_phase(c);
	if (phase >= 0)
		seq_printf(s, "%5d", phase);
	else
		seq_puts(s, "-----");

	seq_printf(s, " %6d", clk_core_get_scaled_duty_cycle(c, 100000));

	if (c->ops->is_enabled)
		seq_printf(s, " %9c\n", clk_core_is_enabled(c) ? 'Y' : 'N');
	else if (!c->ops->enable)
		seq_printf(s, " %9c\n", 'Y');
	else
		seq_printf(s, " %9c\n", '?');
}

static void clk_summary_show_subtree(struct seq_file *s, struct clk_core *c,
				     int level)
{
	struct clk_core *child;

	clk_pm_runtime_get(c);
	clk_summary_show_one(s, c, level);
	clk_pm_runtime_put(c);

	hlist_for_each_entry(child, &c->children, child_node)
		clk_summary_show_subtree(s, child, level + 1);
}

static int clk_summary_show(struct seq_file *s, void *data)
{
	struct clk_core *c;
	struct hlist_head **lists = s->private;

	seq_puts(s, "                                 enable  prepare  protect                                duty  hardware\n");
	seq_puts(s, "   clock                          count    count    count        rate   accuracy phase  cycle    enable\n");
	seq_puts(s, "-------------------------------------------------------------------------------------------------------\n");

	clk_prepare_lock();

	for (; *lists; lists++)
		hlist_for_each_entry(c, *lists, child_node)
			clk_summary_show_subtree(s, c, 0);

	clk_prepare_unlock();

	return 0;
}
DEFINE_SHOW_ATTRIBUTE(clk_summary);

static void clk_dump_one(struct seq_file *s, struct clk_core *c, int level)
{
	int phase;
	unsigned long min_rate, max_rate;

	clk_core_get_boundaries(c, &min_rate, &max_rate);

	 
	seq_printf(s, "\"%s\": { ", c->name);
	seq_printf(s, "\"enable_count\": %d,", c->enable_count);
	seq_printf(s, "\"prepare_count\": %d,", c->prepare_count);
	seq_printf(s, "\"protect_count\": %d,", c->protect_count);
	seq_printf(s, "\"rate\": %lu,", clk_core_get_rate_recalc(c));
	seq_printf(s, "\"min_rate\": %lu,", min_rate);
	seq_printf(s, "\"max_rate\": %lu,", max_rate);
	seq_printf(s, "\"accuracy\": %lu,", clk_core_get_accuracy_recalc(c));
	phase = clk_core_get_phase(c);
	if (phase >= 0)
		seq_printf(s, "\"phase\": %d,", phase);
	seq_printf(s, "\"duty_cycle\": %u",
		   clk_core_get_scaled_duty_cycle(c, 100000));
}

static void clk_dump_subtree(struct seq_file *s, struct clk_core *c, int level)
{
	struct clk_core *child;

	clk_dump_one(s, c, level);

	hlist_for_each_entry(child, &c->children, child_node) {
		seq_putc(s, ',');
		clk_dump_subtree(s, child, level + 1);
	}

	seq_putc(s, '}');
}

static int clk_dump_show(struct seq_file *s, void *data)
{
	struct clk_core *c;
	bool first_node = true;
	struct hlist_head **lists = s->private;

	seq_putc(s, '{');
	clk_prepare_lock();

	for (; *lists; lists++) {
		hlist_for_each_entry(c, *lists, child_node) {
			if (!first_node)
				seq_putc(s, ',');
			first_node = false;
			clk_dump_subtree(s, c, 0);
		}
	}

	clk_prepare_unlock();

	seq_puts(s, "}\n");
	return 0;
}
DEFINE_SHOW_ATTRIBUTE(clk_dump);

#undef CLOCK_ALLOW_WRITE_DEBUGFS
#ifdef CLOCK_ALLOW_WRITE_DEBUGFS
 
static int clk_rate_set(void *data, u64 val)
{
	struct clk_core *core = data;
	int ret;

	clk_prepare_lock();
	ret = clk_core_set_rate_nolock(core, val);
	clk_prepare_unlock();

	return ret;
}

#define clk_rate_mode	0644

static int clk_prepare_enable_set(void *data, u64 val)
{
	struct clk_core *core = data;
	int ret = 0;

	if (val)
		ret = clk_prepare_enable(core->hw->clk);
	else
		clk_disable_unprepare(core->hw->clk);

	return ret;
}

static int clk_prepare_enable_get(void *data, u64 *val)
{
	struct clk_core *core = data;

	*val = core->enable_count && core->prepare_count;
	return 0;
}

DEFINE_DEBUGFS_ATTRIBUTE(clk_prepare_enable_fops, clk_prepare_enable_get,
			 clk_prepare_enable_set, "%llu\n");

#else
#define clk_rate_set	NULL
#define clk_rate_mode	0444
#endif

static int clk_rate_get(void *data, u64 *val)
{
	struct clk_core *core = data;

	clk_prepare_lock();
	*val = clk_core_get_rate_recalc(core);
	clk_prepare_unlock();

	return 0;
}

DEFINE_DEBUGFS_ATTRIBUTE(clk_rate_fops, clk_rate_get, clk_rate_set, "%llu\n");

static const struct {
	unsigned long flag;
	const char *name;
} clk_flags[] = {
#define ENTRY(f) { f, #f }
	ENTRY(CLK_SET_RATE_GATE),
	ENTRY(CLK_SET_PARENT_GATE),
	ENTRY(CLK_SET_RATE_PARENT),
	ENTRY(CLK_IGNORE_UNUSED),
	ENTRY(CLK_GET_RATE_NOCACHE),
	ENTRY(CLK_SET_RATE_NO_REPARENT),
	ENTRY(CLK_GET_ACCURACY_NOCACHE),
	ENTRY(CLK_RECALC_NEW_RATES),
	ENTRY(CLK_SET_RATE_UNGATE),
	ENTRY(CLK_IS_CRITICAL),
	ENTRY(CLK_OPS_PARENT_ENABLE),
	ENTRY(CLK_DUTY_CYCLE_PARENT),
#undef ENTRY
};

static int clk_flags_show(struct seq_file *s, void *data)
{
	struct clk_core *core = s->private;
	unsigned long flags = core->flags;
	unsigned int i;

	for (i = 0; flags && i < ARRAY_SIZE(clk_flags); i++) {
		if (flags & clk_flags[i].flag) {
			seq_printf(s, "%s\n", clk_flags[i].name);
			flags &= ~clk_flags[i].flag;
		}
	}
	if (flags) {
		 
		seq_printf(s, "0x%lx\n", flags);
	}

	return 0;
}
DEFINE_SHOW_ATTRIBUTE(clk_flags);

static void possible_parent_show(struct seq_file *s, struct clk_core *core,
				 unsigned int i, char terminator)
{
	struct clk_core *parent;
	const char *name = NULL;

	 
	parent = clk_core_get_parent_by_index(core, i);
	if (parent) {
		seq_puts(s, parent->name);
	} else if (core->parents[i].name) {
		seq_puts(s, core->parents[i].name);
	} else if (core->parents[i].fw_name) {
		seq_printf(s, "<%s>(fw)", core->parents[i].fw_name);
	} else {
		if (core->parents[i].index >= 0)
			name = of_clk_get_parent_name(core->of_node, core->parents[i].index);
		if (!name)
			name = "(missing)";

		seq_puts(s, name);
	}

	seq_putc(s, terminator);
}

static int possible_parents_show(struct seq_file *s, void *data)
{
	struct clk_core *core = s->private;
	int i;

	for (i = 0; i < core->num_parents - 1; i++)
		possible_parent_show(s, core, i, ' ');

	possible_parent_show(s, core, i, '\n');

	return 0;
}
DEFINE_SHOW_ATTRIBUTE(possible_parents);

static int current_parent_show(struct seq_file *s, void *data)
{
	struct clk_core *core = s->private;

	if (core->parent)
		seq_printf(s, "%s\n", core->parent->name);

	return 0;
}
DEFINE_SHOW_ATTRIBUTE(current_parent);

#ifdef CLOCK_ALLOW_WRITE_DEBUGFS
static ssize_t current_parent_write(struct file *file, const char __user *ubuf,
				    size_t count, loff_t *ppos)
{
	struct seq_file *s = file->private_data;
	struct clk_core *core = s->private;
	struct clk_core *parent;
	u8 idx;
	int err;

	err = kstrtou8_from_user(ubuf, count, 0, &idx);
	if (err < 0)
		return err;

	parent = clk_core_get_parent_by_index(core, idx);
	if (!parent)
		return -ENOENT;

	clk_prepare_lock();
	err = clk_core_set_parent_nolock(core, parent);
	clk_prepare_unlock();
	if (err)
		return err;

	return count;
}

static const struct file_operations current_parent_rw_fops = {
	.open		= current_parent_open,
	.write		= current_parent_write,
	.read		= seq_read,
	.llseek		= seq_lseek,
	.release	= single_release,
};
#endif

static int clk_duty_cycle_show(struct seq_file *s, void *data)
{
	struct clk_core *core = s->private;
	struct clk_duty *duty = &core->duty;

	seq_printf(s, "%u/%u\n", duty->num, duty->den);

	return 0;
}
DEFINE_SHOW_ATTRIBUTE(clk_duty_cycle);

static int clk_min_rate_show(struct seq_file *s, void *data)
{
	struct clk_core *core = s->private;
	unsigned long min_rate, max_rate;

	clk_prepare_lock();
	clk_core_get_boundaries(core, &min_rate, &max_rate);
	clk_prepare_unlock();
	seq_printf(s, "%lu\n", min_rate);

	return 0;
}
DEFINE_SHOW_ATTRIBUTE(clk_min_rate);

static int clk_max_rate_show(struct seq_file *s, void *data)
{
	struct clk_core *core = s->private;
	unsigned long min_rate, max_rate;

	clk_prepare_lock();
	clk_core_get_boundaries(core, &min_rate, &max_rate);
	clk_prepare_unlock();
	seq_printf(s, "%lu\n", max_rate);

	return 0;
}
DEFINE_SHOW_ATTRIBUTE(clk_max_rate);

static void clk_debug_create_one(struct clk_core *core, struct dentry *pdentry)
{
	struct dentry *root;

	if (!core || !pdentry)
		return;

	root = debugfs_create_dir(core->name, pdentry);
	core->dentry = root;

	debugfs_create_file("clk_rate", clk_rate_mode, root, core,
			    &clk_rate_fops);
	debugfs_create_file("clk_min_rate", 0444, root, core, &clk_min_rate_fops);
	debugfs_create_file("clk_max_rate", 0444, root, core, &clk_max_rate_fops);
	debugfs_create_ulong("clk_accuracy", 0444, root, &core->accuracy);
	debugfs_create_u32("clk_phase", 0444, root, &core->phase);
	debugfs_create_file("clk_flags", 0444, root, core, &clk_flags_fops);
	debugfs_create_u32("clk_prepare_count", 0444, root, &core->prepare_count);
	debugfs_create_u32("clk_enable_count", 0444, root, &core->enable_count);
	debugfs_create_u32("clk_protect_count", 0444, root, &core->protect_count);
	debugfs_create_u32("clk_notifier_count", 0444, root, &core->notifier_count);
	debugfs_create_file("clk_duty_cycle", 0444, root, core,
			    &clk_duty_cycle_fops);
#ifdef CLOCK_ALLOW_WRITE_DEBUGFS
	debugfs_create_file("clk_prepare_enable", 0644, root, core,
			    &clk_prepare_enable_fops);

	if (core->num_parents > 1)
		debugfs_create_file("clk_parent", 0644, root, core,
				    &current_parent_rw_fops);
	else
#endif
	if (core->num_parents > 0)
		debugfs_create_file("clk_parent", 0444, root, core,
				    &current_parent_fops);

	if (core->num_parents > 1)
		debugfs_create_file("clk_possible_parents", 0444, root, core,
				    &possible_parents_fops);

	if (core->ops->debug_init)
		core->ops->debug_init(core->hw, core->dentry);
}

 
static void clk_debug_register(struct clk_core *core)
{
	mutex_lock(&clk_debug_lock);
	hlist_add_head(&core->debug_node, &clk_debug_list);
	if (inited)
		clk_debug_create_one(core, rootdir);
	mutex_unlock(&clk_debug_lock);
}

  
static void clk_debug_unregister(struct clk_core *core)
{
	mutex_lock(&clk_debug_lock);
	hlist_del_init(&core->debug_node);
	debugfs_remove_recursive(core->dentry);
	core->dentry = NULL;
	mutex_unlock(&clk_debug_lock);
}

 
static int __init clk_debug_init(void)
{
	struct clk_core *core;

#ifdef CLOCK_ALLOW_WRITE_DEBUGFS
	pr_warn("\n");
	pr_warn("********************************************************************\n");
	pr_warn("**     NOTICE NOTICE NOTICE NOTICE NOTICE NOTICE NOTICE           **\n");
	pr_warn("**                                                                **\n");
	pr_warn("**  WRITEABLE clk DebugFS SUPPORT HAS BEEN ENABLED IN THIS KERNEL **\n");
	pr_warn("**                                                                **\n");
	pr_warn("** This means that this kernel is built to expose clk operations  **\n");
	pr_warn("** such as parent or rate setting, enabling, disabling, etc.      **\n");
	pr_warn("** to userspace, which may compromise security on your system.    **\n");
	pr_warn("**                                                                **\n");
	pr_warn("** If you see this message and you are not debugging the          **\n");
	pr_warn("** kernel, report this immediately to your vendor!                **\n");
	pr_warn("**                                                                **\n");
	pr_warn("**     NOTICE NOTICE NOTICE NOTICE NOTICE NOTICE NOTICE           **\n");
	pr_warn("********************************************************************\n");
#endif

	rootdir = debugfs_create_dir("clk", NULL);

	debugfs_create_file("clk_summary", 0444, rootdir, &all_lists,
			    &clk_summary_fops);
	debugfs_create_file("clk_dump", 0444, rootdir, &all_lists,
			    &clk_dump_fops);
	debugfs_create_file("clk_orphan_summary", 0444, rootdir, &orphan_list,
			    &clk_summary_fops);
	debugfs_create_file("clk_orphan_dump", 0444, rootdir, &orphan_list,
			    &clk_dump_fops);

	mutex_lock(&clk_debug_lock);
	hlist_for_each_entry(core, &clk_debug_list, debug_node)
		clk_debug_create_one(core, rootdir);

	inited = 1;
	mutex_unlock(&clk_debug_lock);

	return 0;
}
late_initcall(clk_debug_init);
#else
static inline void clk_debug_register(struct clk_core *core) { }
static inline void clk_debug_unregister(struct clk_core *core)
{
}
#endif

static void clk_core_reparent_orphans_nolock(void)
{
	struct clk_core *orphan;
	struct hlist_node *tmp2;

	 
	hlist_for_each_entry_safe(orphan, tmp2, &clk_orphan_list, child_node) {
		struct clk_core *parent = __clk_init_parent(orphan);

		 
		if (parent) {
			 
			__clk_set_parent_before(orphan, parent);
			__clk_set_parent_after(orphan, parent, NULL);
			__clk_recalc_accuracies(orphan);
			__clk_recalc_rates(orphan, true, 0);

			 
			orphan->req_rate = orphan->rate;
		}
	}
}

 
static int __clk_core_init(struct clk_core *core)
{
	int ret;
	struct clk_core *parent;
	unsigned long rate;
	int phase;

	clk_prepare_lock();

	 
	core->hw->core = core;

	ret = clk_pm_runtime_get(core);
	if (ret)
		goto unlock;

	 
	if (clk_core_lookup(core->name)) {
		pr_debug("%s: clk %s already initialized\n",
				__func__, core->name);
		ret = -EEXIST;
		goto out;
	}

	 
	if (core->ops->set_rate &&
	    !((core->ops->round_rate || core->ops->determine_rate) &&
	      core->ops->recalc_rate)) {
		pr_err("%s: %s must implement .round_rate or .determine_rate in addition to .recalc_rate\n",
		       __func__, core->name);
		ret = -EINVAL;
		goto out;
	}

	if (core->ops->set_parent && !core->ops->get_parent) {
		pr_err("%s: %s must implement .get_parent & .set_parent\n",
		       __func__, core->name);
		ret = -EINVAL;
		goto out;
	}

	if (core->ops->set_parent && !core->ops->determine_rate) {
		pr_err("%s: %s must implement .set_parent & .determine_rate\n",
			__func__, core->name);
		ret = -EINVAL;
		goto out;
	}

	if (core->num_parents > 1 && !core->ops->get_parent) {
		pr_err("%s: %s must implement .get_parent as it has multi parents\n",
		       __func__, core->name);
		ret = -EINVAL;
		goto out;
	}

	if (core->ops->set_rate_and_parent &&
			!(core->ops->set_parent && core->ops->set_rate)) {
		pr_err("%s: %s must implement .set_parent & .set_rate\n",
				__func__, core->name);
		ret = -EINVAL;
		goto out;
	}

	 
	if (core->ops->init) {
		ret = core->ops->init(core->hw);
		if (ret)
			goto out;
	}

	parent = core->parent = __clk_init_parent(core);

	 
	if (parent) {
		hlist_add_head(&core->child_node, &parent->children);
		core->orphan = parent->orphan;
	} else if (!core->num_parents) {
		hlist_add_head(&core->child_node, &clk_root_list);
		core->orphan = false;
	} else {
		hlist_add_head(&core->child_node, &clk_orphan_list);
		core->orphan = true;
	}

	 
	if (core->ops->recalc_accuracy)
		core->accuracy = core->ops->recalc_accuracy(core->hw,
					clk_core_get_accuracy_no_lock(parent));
	else if (parent)
		core->accuracy = parent->accuracy;
	else
		core->accuracy = 0;

	 
	phase = clk_core_get_phase(core);
	if (phase < 0) {
		ret = phase;
		pr_warn("%s: Failed to get phase for clk '%s'\n", __func__,
			core->name);
		goto out;
	}

	 
	clk_core_update_duty_cycle_nolock(core);

	 
	if (core->ops->recalc_rate)
		rate = core->ops->recalc_rate(core->hw,
				clk_core_get_rate_nolock(parent));
	else if (parent)
		rate = parent->rate;
	else
		rate = 0;
	core->rate = core->req_rate = rate;

	 
	if (core->flags & CLK_IS_CRITICAL) {
		ret = clk_core_prepare(core);
		if (ret) {
			pr_warn("%s: critical clk '%s' failed to prepare\n",
			       __func__, core->name);
			goto out;
		}

		ret = clk_core_enable_lock(core);
		if (ret) {
			pr_warn("%s: critical clk '%s' failed to enable\n",
			       __func__, core->name);
			clk_core_unprepare(core);
			goto out;
		}
	}

	clk_core_reparent_orphans_nolock();

	kref_init(&core->ref);
out:
	clk_pm_runtime_put(core);
unlock:
	if (ret) {
		hlist_del_init(&core->child_node);
		core->hw->core = NULL;
	}

	clk_prepare_unlock();

	if (!ret)
		clk_debug_register(core);

	return ret;
}

 
static void clk_core_link_consumer(struct clk_core *core, struct clk *clk)
{
	clk_prepare_lock();
	hlist_add_head(&clk->clks_node, &core->clks);
	clk_prepare_unlock();
}

 
static void clk_core_unlink_consumer(struct clk *clk)
{
	lockdep_assert_held(&prepare_lock);
	hlist_del(&clk->clks_node);
}

 
static struct clk *alloc_clk(struct clk_core *core, const char *dev_id,
			     const char *con_id)
{
	struct clk *clk;

	clk = kzalloc(sizeof(*clk), GFP_KERNEL);
	if (!clk)
		return ERR_PTR(-ENOMEM);

	clk->core = core;
	clk->dev_id = dev_id;
	clk->con_id = kstrdup_const(con_id, GFP_KERNEL);
	clk->max_rate = ULONG_MAX;

	return clk;
}

 
static void free_clk(struct clk *clk)
{
	kfree_const(clk->con_id);
	kfree(clk);
}

 
struct clk *clk_hw_create_clk(struct device *dev, struct clk_hw *hw,
			      const char *dev_id, const char *con_id)
{
	struct clk *clk;
	struct clk_core *core;

	 
	if (IS_ERR_OR_NULL(hw))
		return ERR_CAST(hw);

	core = hw->core;
	clk = alloc_clk(core, dev_id, con_id);
	if (IS_ERR(clk))
		return clk;
	clk->dev = dev;

	if (!try_module_get(core->owner)) {
		free_clk(clk);
		return ERR_PTR(-ENOENT);
	}

	kref_get(&core->ref);
	clk_core_link_consumer(core, clk);

	return clk;
}

 
struct clk *clk_hw_get_clk(struct clk_hw *hw, const char *con_id)
{
	struct device *dev = hw->core->dev;
	const char *name = dev ? dev_name(dev) : NULL;

	return clk_hw_create_clk(dev, hw, name, con_id);
}
EXPORT_SYMBOL(clk_hw_get_clk);

static int clk_cpy_name(const char **dst_p, const char *src, bool must_exist)
{
	const char *dst;

	if (!src) {
		if (must_exist)
			return -EINVAL;
		return 0;
	}

	*dst_p = dst = kstrdup_const(src, GFP_KERNEL);
	if (!dst)
		return -ENOMEM;

	return 0;
}

static int clk_core_populate_parent_map(struct clk_core *core,
					const struct clk_init_data *init)
{
	u8 num_parents = init->num_parents;
	const char * const *parent_names = init->parent_names;
	const struct clk_hw **parent_hws = init->parent_hws;
	const struct clk_parent_data *parent_data = init->parent_data;
	int i, ret = 0;
	struct clk_parent_map *parents, *parent;

	if (!num_parents)
		return 0;

	 
	parents = kcalloc(num_parents, sizeof(*parents), GFP_KERNEL);
	core->parents = parents;
	if (!parents)
		return -ENOMEM;

	 
	for (i = 0, parent = parents; i < num_parents; i++, parent++) {
		parent->index = -1;
		if (parent_names) {
			 
			WARN(!parent_names[i],
				"%s: invalid NULL in %s's .parent_names\n",
				__func__, core->name);
			ret = clk_cpy_name(&parent->name, parent_names[i],
					   true);
		} else if (parent_data) {
			parent->hw = parent_data[i].hw;
			parent->index = parent_data[i].index;
			ret = clk_cpy_name(&parent->fw_name,
					   parent_data[i].fw_name, false);
			if (!ret)
				ret = clk_cpy_name(&parent->name,
						   parent_data[i].name,
						   false);
		} else if (parent_hws) {
			parent->hw = parent_hws[i];
		} else {
			ret = -EINVAL;
			WARN(1, "Must specify parents if num_parents > 0\n");
		}

		if (ret) {
			do {
				kfree_const(parents[i].name);
				kfree_const(parents[i].fw_name);
			} while (--i >= 0);
			kfree(parents);

			return ret;
		}
	}

	return 0;
}

static void clk_core_free_parent_map(struct clk_core *core)
{
	int i = core->num_parents;

	if (!core->num_parents)
		return;

	while (--i >= 0) {
		kfree_const(core->parents[i].name);
		kfree_const(core->parents[i].fw_name);
	}

	kfree(core->parents);
}

static struct clk *
__clk_register(struct device *dev, struct device_node *np, struct clk_hw *hw)
{
	int ret;
	struct clk_core *core;
	const struct clk_init_data *init = hw->init;

	 
	hw->init = NULL;

	core = kzalloc(sizeof(*core), GFP_KERNEL);
	if (!core) {
		ret = -ENOMEM;
		goto fail_out;
	}

	core->name = kstrdup_const(init->name, GFP_KERNEL);
	if (!core->name) {
		ret = -ENOMEM;
		goto fail_name;
	}

	if (WARN_ON(!init->ops)) {
		ret = -EINVAL;
		goto fail_ops;
	}
	core->ops = init->ops;

	if (dev && pm_runtime_enabled(dev))
		core->rpm_enabled = true;
	core->dev = dev;
	core->of_node = np;
	if (dev && dev->driver)
		core->owner = dev->driver->owner;
	core->hw = hw;
	core->flags = init->flags;
	core->num_parents = init->num_parents;
	core->min_rate = 0;
	core->max_rate = ULONG_MAX;

	ret = clk_core_populate_parent_map(core, init);
	if (ret)
		goto fail_parents;

	INIT_HLIST_HEAD(&core->clks);

	 
	hw->clk = alloc_clk(core, NULL, NULL);
	if (IS_ERR(hw->clk)) {
		ret = PTR_ERR(hw->clk);
		goto fail_create_clk;
	}

	clk_core_link_consumer(core, hw->clk);

	ret = __clk_core_init(core);
	if (!ret)
		return hw->clk;

	clk_prepare_lock();
	clk_core_unlink_consumer(hw->clk);
	clk_prepare_unlock();

	free_clk(hw->clk);
	hw->clk = NULL;

fail_create_clk:
	clk_core_free_parent_map(core);
fail_parents:
fail_ops:
	kfree_const(core->name);
fail_name:
	kfree(core);
fail_out:
	return ERR_PTR(ret);
}

 
static struct device_node *dev_or_parent_of_node(struct device *dev)
{
	struct device_node *np;

	if (!dev)
		return NULL;

	np = dev_of_node(dev);
	if (!np)
		np = dev_of_node(dev->parent);

	return np;
}

 
struct clk *clk_register(struct device *dev, struct clk_hw *hw)
{
	return __clk_register(dev, dev_or_parent_of_node(dev), hw);
}
EXPORT_SYMBOL_GPL(clk_register);

 
int clk_hw_register(struct device *dev, struct clk_hw *hw)
{
	return PTR_ERR_OR_ZERO(__clk_register(dev, dev_or_parent_of_node(dev),
			       hw));
}
EXPORT_SYMBOL_GPL(clk_hw_register);

 
int of_clk_hw_register(struct device_node *node, struct clk_hw *hw)
{
	return PTR_ERR_OR_ZERO(__clk_register(NULL, node, hw));
}
EXPORT_SYMBOL_GPL(of_clk_hw_register);

 
static void __clk_release(struct kref *ref)
{
	struct clk_core *core = container_of(ref, struct clk_core, ref);

	lockdep_assert_held(&prepare_lock);

	clk_core_free_parent_map(core);
	kfree_const(core->name);
	kfree(core);
}

 
static int clk_nodrv_prepare_enable(struct clk_hw *hw)
{
	return -ENXIO;
}

static void clk_nodrv_disable_unprepare(struct clk_hw *hw)
{
	WARN_ON_ONCE(1);
}

static int clk_nodrv_set_rate(struct clk_hw *hw, unsigned long rate,
					unsigned long parent_rate)
{
	return -ENXIO;
}

static int clk_nodrv_set_parent(struct clk_hw *hw, u8 index)
{
	return -ENXIO;
}

static int clk_nodrv_determine_rate(struct clk_hw *hw,
				    struct clk_rate_request *req)
{
	return -ENXIO;
}

static const struct clk_ops clk_nodrv_ops = {
	.enable		= clk_nodrv_prepare_enable,
	.disable	= clk_nodrv_disable_unprepare,
	.prepare	= clk_nodrv_prepare_enable,
	.unprepare	= clk_nodrv_disable_unprepare,
	.determine_rate	= clk_nodrv_determine_rate,
	.set_rate	= clk_nodrv_set_rate,
	.set_parent	= clk_nodrv_set_parent,
};

static void clk_core_evict_parent_cache_subtree(struct clk_core *root,
						const struct clk_core *target)
{
	int i;
	struct clk_core *child;

	for (i = 0; i < root->num_parents; i++)
		if (root->parents[i].core == target)
			root->parents[i].core = NULL;

	hlist_for_each_entry(child, &root->children, child_node)
		clk_core_evict_parent_cache_subtree(child, target);
}

 
static void clk_core_evict_parent_cache(struct clk_core *core)
{
	const struct hlist_head **lists;
	struct clk_core *root;

	lockdep_assert_held(&prepare_lock);

	for (lists = all_lists; *lists; lists++)
		hlist_for_each_entry(root, *lists, child_node)
			clk_core_evict_parent_cache_subtree(root, core);

}

 
void clk_unregister(struct clk *clk)
{
	unsigned long flags;
	const struct clk_ops *ops;

	if (!clk || WARN_ON_ONCE(IS_ERR(clk)))
		return;

	clk_debug_unregister(clk->core);

	clk_prepare_lock();

	ops = clk->core->ops;
	if (ops == &clk_nodrv_ops) {
		pr_err("%s: unregistered clock: %s\n", __func__,
		       clk->core->name);
		goto unlock;
	}
	 
	flags = clk_enable_lock();
	clk->core->ops = &clk_nodrv_ops;
	clk_enable_unlock(flags);

	if (ops->terminate)
		ops->terminate(clk->core->hw);

	if (!hlist_empty(&clk->core->children)) {
		struct clk_core *child;
		struct hlist_node *t;

		 
		hlist_for_each_entry_safe(child, t, &clk->core->children,
					  child_node)
			clk_core_set_parent_nolock(child, NULL);
	}

	clk_core_evict_parent_cache(clk->core);

	hlist_del_init(&clk->core->child_node);

	if (clk->core->prepare_count)
		pr_warn("%s: unregistering prepared clock: %s\n",
					__func__, clk->core->name);

	if (clk->core->protect_count)
		pr_warn("%s: unregistering protected clock: %s\n",
					__func__, clk->core->name);

	kref_put(&clk->core->ref, __clk_release);
	free_clk(clk);
unlock:
	clk_prepare_unlock();
}
EXPORT_SYMBOL_GPL(clk_unregister);

 
void clk_hw_unregister(struct clk_hw *hw)
{
	clk_unregister(hw->clk);
}
EXPORT_SYMBOL_GPL(clk_hw_unregister);

static void devm_clk_unregister_cb(struct device *dev, void *res)
{
	clk_unregister(*(struct clk **)res);
}

static void devm_clk_hw_unregister_cb(struct device *dev, void *res)
{
	clk_hw_unregister(*(struct clk_hw **)res);
}

 
struct clk *devm_clk_register(struct device *dev, struct clk_hw *hw)
{
	struct clk *clk;
	struct clk **clkp;

	clkp = devres_alloc(devm_clk_unregister_cb, sizeof(*clkp), GFP_KERNEL);
	if (!clkp)
		return ERR_PTR(-ENOMEM);

	clk = clk_register(dev, hw);
	if (!IS_ERR(clk)) {
		*clkp = clk;
		devres_add(dev, clkp);
	} else {
		devres_free(clkp);
	}

	return clk;
}
EXPORT_SYMBOL_GPL(devm_clk_register);

 
int devm_clk_hw_register(struct device *dev, struct clk_hw *hw)
{
	struct clk_hw **hwp;
	int ret;

	hwp = devres_alloc(devm_clk_hw_unregister_cb, sizeof(*hwp), GFP_KERNEL);
	if (!hwp)
		return -ENOMEM;

	ret = clk_hw_register(dev, hw);
	if (!ret) {
		*hwp = hw;
		devres_add(dev, hwp);
	} else {
		devres_free(hwp);
	}

	return ret;
}
EXPORT_SYMBOL_GPL(devm_clk_hw_register);

static void devm_clk_release(struct device *dev, void *res)
{
	clk_put(*(struct clk **)res);
}

 
struct clk *devm_clk_hw_get_clk(struct device *dev, struct clk_hw *hw,
				const char *con_id)
{
	struct clk *clk;
	struct clk **clkp;

	 
	WARN_ON_ONCE(dev != hw->core->dev);

	clkp = devres_alloc(devm_clk_release, sizeof(*clkp), GFP_KERNEL);
	if (!clkp)
		return ERR_PTR(-ENOMEM);

	clk = clk_hw_get_clk(hw, con_id);
	if (!IS_ERR(clk)) {
		*clkp = clk;
		devres_add(dev, clkp);
	} else {
		devres_free(clkp);
	}

	return clk;
}
EXPORT_SYMBOL_GPL(devm_clk_hw_get_clk);

 

void __clk_put(struct clk *clk)
{
	struct module *owner;

	if (!clk || WARN_ON_ONCE(IS_ERR(clk)))
		return;

	clk_prepare_lock();

	 
	if (WARN_ON(clk->exclusive_count)) {
		 
		clk->core->protect_count -= (clk->exclusive_count - 1);
		clk_core_rate_unprotect(clk->core);
		clk->exclusive_count = 0;
	}

	hlist_del(&clk->clks_node);

	 
	if (clk->min_rate > 0 || clk->max_rate < ULONG_MAX)
		clk_set_rate_range_nolock(clk, 0, ULONG_MAX);

	owner = clk->core->owner;
	kref_put(&clk->core->ref, __clk_release);

	clk_prepare_unlock();

	module_put(owner);

	free_clk(clk);
}

 

 
int clk_notifier_register(struct clk *clk, struct notifier_block *nb)
{
	struct clk_notifier *cn;
	int ret = -ENOMEM;

	if (!clk || !nb)
		return -EINVAL;

	clk_prepare_lock();

	 
	list_for_each_entry(cn, &clk_notifier_list, node)
		if (cn->clk == clk)
			goto found;

	 
	cn = kzalloc(sizeof(*cn), GFP_KERNEL);
	if (!cn)
		goto out;

	cn->clk = clk;
	srcu_init_notifier_head(&cn->notifier_head);

	list_add(&cn->node, &clk_notifier_list);

found:
	ret = srcu_notifier_chain_register(&cn->notifier_head, nb);

	clk->core->notifier_count++;

out:
	clk_prepare_unlock();

	return ret;
}
EXPORT_SYMBOL_GPL(clk_notifier_register);

 
int clk_notifier_unregister(struct clk *clk, struct notifier_block *nb)
{
	struct clk_notifier *cn;
	int ret = -ENOENT;

	if (!clk || !nb)
		return -EINVAL;

	clk_prepare_lock();

	list_for_each_entry(cn, &clk_notifier_list, node) {
		if (cn->clk == clk) {
			ret = srcu_notifier_chain_unregister(&cn->notifier_head, nb);

			clk->core->notifier_count--;

			 
			if (!cn->notifier_head.head) {
				srcu_cleanup_notifier_head(&cn->notifier_head);
				list_del(&cn->node);
				kfree(cn);
			}
			break;
		}
	}

	clk_prepare_unlock();

	return ret;
}
EXPORT_SYMBOL_GPL(clk_notifier_unregister);

struct clk_notifier_devres {
	struct clk *clk;
	struct notifier_block *nb;
};

static void devm_clk_notifier_release(struct device *dev, void *res)
{
	struct clk_notifier_devres *devres = res;

	clk_notifier_unregister(devres->clk, devres->nb);
}

int devm_clk_notifier_register(struct device *dev, struct clk *clk,
			       struct notifier_block *nb)
{
	struct clk_notifier_devres *devres;
	int ret;

	devres = devres_alloc(devm_clk_notifier_release,
			      sizeof(*devres), GFP_KERNEL);

	if (!devres)
		return -ENOMEM;

	ret = clk_notifier_register(clk, nb);
	if (!ret) {
		devres->clk = clk;
		devres->nb = nb;
		devres_add(dev, devres);
	} else {
		devres_free(devres);
	}

	return ret;
}
EXPORT_SYMBOL_GPL(devm_clk_notifier_register);

#ifdef CONFIG_OF
static void clk_core_reparent_orphans(void)
{
	clk_prepare_lock();
	clk_core_reparent_orphans_nolock();
	clk_prepare_unlock();
}

 
struct of_clk_provider {
	struct list_head link;

	struct device_node *node;
	struct clk *(*get)(struct of_phandle_args *clkspec, void *data);
	struct clk_hw *(*get_hw)(struct of_phandle_args *clkspec, void *data);
	void *data;
};

extern struct of_device_id __clk_of_table;
static const struct of_device_id __clk_of_table_sentinel
	__used __section("__clk_of_table_end");

static LIST_HEAD(of_clk_providers);
static DEFINE_MUTEX(of_clk_mutex);

struct clk *of_clk_src_simple_get(struct of_phandle_args *clkspec,
				     void *data)
{
	return data;
}
EXPORT_SYMBOL_GPL(of_clk_src_simple_get);

struct clk_hw *of_clk_hw_simple_get(struct of_phandle_args *clkspec, void *data)
{
	return data;
}
EXPORT_SYMBOL_GPL(of_clk_hw_simple_get);

struct clk *of_clk_src_onecell_get(struct of_phandle_args *clkspec, void *data)
{
	struct clk_onecell_data *clk_data = data;
	unsigned int idx = clkspec->args[0];

	if (idx >= clk_data->clk_num) {
		pr_err("%s: invalid clock index %u\n", __func__, idx);
		return ERR_PTR(-EINVAL);
	}

	return clk_data->clks[idx];
}
EXPORT_SYMBOL_GPL(of_clk_src_onecell_get);

struct clk_hw *
of_clk_hw_onecell_get(struct of_phandle_args *clkspec, void *data)
{
	struct clk_hw_onecell_data *hw_data = data;
	unsigned int idx = clkspec->args[0];

	if (idx >= hw_data->num) {
		pr_err("%s: invalid index %u\n", __func__, idx);
		return ERR_PTR(-EINVAL);
	}

	return hw_data->hws[idx];
}
EXPORT_SYMBOL_GPL(of_clk_hw_onecell_get);

 
int of_clk_add_provider(struct device_node *np,
			struct clk *(*clk_src_get)(struct of_phandle_args *clkspec,
						   void *data),
			void *data)
{
	struct of_clk_provider *cp;
	int ret;

	if (!np)
		return 0;

	cp = kzalloc(sizeof(*cp), GFP_KERNEL);
	if (!cp)
		return -ENOMEM;

	cp->node = of_node_get(np);
	cp->data = data;
	cp->get = clk_src_get;

	mutex_lock(&of_clk_mutex);
	list_add(&cp->link, &of_clk_providers);
	mutex_unlock(&of_clk_mutex);
	pr_debug("Added clock from %pOF\n", np);

	clk_core_reparent_orphans();

	ret = of_clk_set_defaults(np, true);
	if (ret < 0)
		of_clk_del_provider(np);

	fwnode_dev_initialized(&np->fwnode, true);

	return ret;
}
EXPORT_SYMBOL_GPL(of_clk_add_provider);

 
int of_clk_add_hw_provider(struct device_node *np,
			   struct clk_hw *(*get)(struct of_phandle_args *clkspec,
						 void *data),
			   void *data)
{
	struct of_clk_provider *cp;
	int ret;

	if (!np)
		return 0;

	cp = kzalloc(sizeof(*cp), GFP_KERNEL);
	if (!cp)
		return -ENOMEM;

	cp->node = of_node_get(np);
	cp->data = data;
	cp->get_hw = get;

	mutex_lock(&of_clk_mutex);
	list_add(&cp->link, &of_clk_providers);
	mutex_unlock(&of_clk_mutex);
	pr_debug("Added clk_hw provider from %pOF\n", np);

	clk_core_reparent_orphans();

	ret = of_clk_set_defaults(np, true);
	if (ret < 0)
		of_clk_del_provider(np);

	fwnode_dev_initialized(&np->fwnode, true);

	return ret;
}
EXPORT_SYMBOL_GPL(of_clk_add_hw_provider);

static void devm_of_clk_release_provider(struct device *dev, void *res)
{
	of_clk_del_provider(*(struct device_node **)res);
}

 
static struct device_node *get_clk_provider_node(struct device *dev)
{
	struct device_node *np, *parent_np;

	np = dev->of_node;
	parent_np = dev->parent ? dev->parent->of_node : NULL;

	if (!of_property_present(np, "#clock-cells"))
		if (of_property_present(parent_np, "#clock-cells"))
			np = parent_np;

	return np;
}

 
int devm_of_clk_add_hw_provider(struct device *dev,
			struct clk_hw *(*get)(struct of_phandle_args *clkspec,
					      void *data),
			void *data)
{
	struct device_node **ptr, *np;
	int ret;

	ptr = devres_alloc(devm_of_clk_release_provider, sizeof(*ptr),
			   GFP_KERNEL);
	if (!ptr)
		return -ENOMEM;

	np = get_clk_provider_node(dev);
	ret = of_clk_add_hw_provider(np, get, data);
	if (!ret) {
		*ptr = np;
		devres_add(dev, ptr);
	} else {
		devres_free(ptr);
	}

	return ret;
}
EXPORT_SYMBOL_GPL(devm_of_clk_add_hw_provider);

 
void of_clk_del_provider(struct device_node *np)
{
	struct of_clk_provider *cp;

	if (!np)
		return;

	mutex_lock(&of_clk_mutex);
	list_for_each_entry(cp, &of_clk_providers, link) {
		if (cp->node == np) {
			list_del(&cp->link);
			fwnode_dev_initialized(&np->fwnode, false);
			of_node_put(cp->node);
			kfree(cp);
			break;
		}
	}
	mutex_unlock(&of_clk_mutex);
}
EXPORT_SYMBOL_GPL(of_clk_del_provider);

 
static int of_parse_clkspec(const struct device_node *np, int index,
			    const char *name, struct of_phandle_args *out_args)
{
	int ret = -ENOENT;

	 
	while (np) {
		 
		if (name)
			index = of_property_match_string(np, "clock-names", name);
		ret = of_parse_phandle_with_args(np, "clocks", "#clock-cells",
						 index, out_args);
		if (!ret)
			break;
		if (name && index >= 0)
			break;

		 
		np = np->parent;
		if (np && !of_get_property(np, "clock-ranges", NULL))
			break;
		index = 0;
	}

	return ret;
}

static struct clk_hw *
__of_clk_get_hw_from_provider(struct of_clk_provider *provider,
			      struct of_phandle_args *clkspec)
{
	struct clk *clk;

	if (provider->get_hw)
		return provider->get_hw(clkspec, provider->data);

	clk = provider->get(clkspec, provider->data);
	if (IS_ERR(clk))
		return ERR_CAST(clk);
	return __clk_get_hw(clk);
}

static struct clk_hw *
of_clk_get_hw_from_clkspec(struct of_phandle_args *clkspec)
{
	struct of_clk_provider *provider;
	struct clk_hw *hw = ERR_PTR(-EPROBE_DEFER);

	if (!clkspec)
		return ERR_PTR(-EINVAL);

	mutex_lock(&of_clk_mutex);
	list_for_each_entry(provider, &of_clk_providers, link) {
		if (provider->node == clkspec->np) {
			hw = __of_clk_get_hw_from_provider(provider, clkspec);
			if (!IS_ERR(hw))
				break;
		}
	}
	mutex_unlock(&of_clk_mutex);

	return hw;
}

 
struct clk *of_clk_get_from_provider(struct of_phandle_args *clkspec)
{
	struct clk_hw *hw = of_clk_get_hw_from_clkspec(clkspec);

	return clk_hw_create_clk(NULL, hw, NULL, __func__);
}
EXPORT_SYMBOL_GPL(of_clk_get_from_provider);

struct clk_hw *of_clk_get_hw(struct device_node *np, int index,
			     const char *con_id)
{
	int ret;
	struct clk_hw *hw;
	struct of_phandle_args clkspec;

	ret = of_parse_clkspec(np, index, con_id, &clkspec);
	if (ret)
		return ERR_PTR(ret);

	hw = of_clk_get_hw_from_clkspec(&clkspec);
	of_node_put(clkspec.np);

	return hw;
}

static struct clk *__of_clk_get(struct device_node *np,
				int index, const char *dev_id,
				const char *con_id)
{
	struct clk_hw *hw = of_clk_get_hw(np, index, con_id);

	return clk_hw_create_clk(NULL, hw, dev_id, con_id);
}

struct clk *of_clk_get(struct device_node *np, int index)
{
	return __of_clk_get(np, index, np->full_name, NULL);
}
EXPORT_SYMBOL(of_clk_get);

 
struct clk *of_clk_get_by_name(struct device_node *np, const char *name)
{
	if (!np)
		return ERR_PTR(-ENOENT);

	return __of_clk_get(np, 0, np->full_name, name);
}
EXPORT_SYMBOL(of_clk_get_by_name);

 
unsigned int of_clk_get_parent_count(const struct device_node *np)
{
	int count;

	count = of_count_phandle_with_args(np, "clocks", "#clock-cells");
	if (count < 0)
		return 0;

	return count;
}
EXPORT_SYMBOL_GPL(of_clk_get_parent_count);

const char *of_clk_get_parent_name(const struct device_node *np, int index)
{
	struct of_phandle_args clkspec;
	struct property *prop;
	const char *clk_name;
	const __be32 *vp;
	u32 pv;
	int rc;
	int count;
	struct clk *clk;

	rc = of_parse_phandle_with_args(np, "clocks", "#clock-cells", index,
					&clkspec);
	if (rc)
		return NULL;

	index = clkspec.args_count ? clkspec.args[0] : 0;
	count = 0;

	 
	of_property_for_each_u32(clkspec.np, "clock-indices", prop, vp, pv) {
		if (index == pv) {
			index = count;
			break;
		}
		count++;
	}
	 
	if (prop && !vp)
		return NULL;

	if (of_property_read_string_index(clkspec.np, "clock-output-names",
					  index,
					  &clk_name) < 0) {
		 
		clk = of_clk_get_from_provider(&clkspec);
		if (IS_ERR(clk)) {
			if (clkspec.args_count == 0)
				clk_name = clkspec.np->name;
			else
				clk_name = NULL;
		} else {
			clk_name = __clk_get_name(clk);
			clk_put(clk);
		}
	}


	of_node_put(clkspec.np);
	return clk_name;
}
EXPORT_SYMBOL_GPL(of_clk_get_parent_name);

 
int of_clk_parent_fill(struct device_node *np, const char **parents,
		       unsigned int size)
{
	unsigned int i = 0;

	while (i < size && (parents[i] = of_clk_get_parent_name(np, i)) != NULL)
		i++;

	return i;
}
EXPORT_SYMBOL_GPL(of_clk_parent_fill);

struct clock_provider {
	void (*clk_init_cb)(struct device_node *);
	struct device_node *np;
	struct list_head node;
};

 
static int parent_ready(struct device_node *np)
{
	int i = 0;

	while (true) {
		struct clk *clk = of_clk_get(np, i);

		 
		if (!IS_ERR(clk)) {
			clk_put(clk);
			i++;
			continue;
		}

		 
		if (PTR_ERR(clk) == -EPROBE_DEFER)
			return 0;

		 
		return 1;
	}
}

 
int of_clk_detect_critical(struct device_node *np, int index,
			   unsigned long *flags)
{
	struct property *prop;
	const __be32 *cur;
	uint32_t idx;

	if (!np || !flags)
		return -EINVAL;

	of_property_for_each_u32(np, "clock-critical", prop, cur, idx)
		if (index == idx)
			*flags |= CLK_IS_CRITICAL;

	return 0;
}

 
void __init of_clk_init(const struct of_device_id *matches)
{
	const struct of_device_id *match;
	struct device_node *np;
	struct clock_provider *clk_provider, *next;
	bool is_init_done;
	bool force = false;
	LIST_HEAD(clk_provider_list);

	if (!matches)
		matches = &__clk_of_table;

	 
	for_each_matching_node_and_match(np, matches, &match) {
		struct clock_provider *parent;

		if (!of_device_is_available(np))
			continue;

		parent = kzalloc(sizeof(*parent), GFP_KERNEL);
		if (!parent) {
			list_for_each_entry_safe(clk_provider, next,
						 &clk_provider_list, node) {
				list_del(&clk_provider->node);
				of_node_put(clk_provider->np);
				kfree(clk_provider);
			}
			of_node_put(np);
			return;
		}

		parent->clk_init_cb = match->data;
		parent->np = of_node_get(np);
		list_add_tail(&parent->node, &clk_provider_list);
	}

	while (!list_empty(&clk_provider_list)) {
		is_init_done = false;
		list_for_each_entry_safe(clk_provider, next,
					&clk_provider_list, node) {
			if (force || parent_ready(clk_provider->np)) {

				 
				of_node_set_flag(clk_provider->np,
						 OF_POPULATED);

				clk_provider->clk_init_cb(clk_provider->np);
				of_clk_set_defaults(clk_provider->np, true);

				list_del(&clk_provider->node);
				of_node_put(clk_provider->np);
				kfree(clk_provider);
				is_init_done = true;
			}
		}

		 
		if (!is_init_done)
			force = true;
	}
}
#endif
