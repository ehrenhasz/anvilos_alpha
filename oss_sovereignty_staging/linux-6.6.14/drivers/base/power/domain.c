
 
#define pr_fmt(fmt) "PM: " fmt

#include <linux/delay.h>
#include <linux/kernel.h>
#include <linux/io.h>
#include <linux/platform_device.h>
#include <linux/pm_opp.h>
#include <linux/pm_runtime.h>
#include <linux/pm_domain.h>
#include <linux/pm_qos.h>
#include <linux/pm_clock.h>
#include <linux/slab.h>
#include <linux/err.h>
#include <linux/sched.h>
#include <linux/suspend.h>
#include <linux/export.h>
#include <linux/cpu.h>
#include <linux/debugfs.h>

#include "power.h"

#define GENPD_RETRY_MAX_MS	250		 

#define GENPD_DEV_CALLBACK(genpd, type, callback, dev)		\
({								\
	type (*__routine)(struct device *__d); 			\
	type __ret = (type)0;					\
								\
	__routine = genpd->dev_ops.callback; 			\
	if (__routine) {					\
		__ret = __routine(dev); 			\
	}							\
	__ret;							\
})

static LIST_HEAD(gpd_list);
static DEFINE_MUTEX(gpd_list_lock);

struct genpd_lock_ops {
	void (*lock)(struct generic_pm_domain *genpd);
	void (*lock_nested)(struct generic_pm_domain *genpd, int depth);
	int (*lock_interruptible)(struct generic_pm_domain *genpd);
	void (*unlock)(struct generic_pm_domain *genpd);
};

static void genpd_lock_mtx(struct generic_pm_domain *genpd)
{
	mutex_lock(&genpd->mlock);
}

static void genpd_lock_nested_mtx(struct generic_pm_domain *genpd,
					int depth)
{
	mutex_lock_nested(&genpd->mlock, depth);
}

static int genpd_lock_interruptible_mtx(struct generic_pm_domain *genpd)
{
	return mutex_lock_interruptible(&genpd->mlock);
}

static void genpd_unlock_mtx(struct generic_pm_domain *genpd)
{
	return mutex_unlock(&genpd->mlock);
}

static const struct genpd_lock_ops genpd_mtx_ops = {
	.lock = genpd_lock_mtx,
	.lock_nested = genpd_lock_nested_mtx,
	.lock_interruptible = genpd_lock_interruptible_mtx,
	.unlock = genpd_unlock_mtx,
};

static void genpd_lock_spin(struct generic_pm_domain *genpd)
	__acquires(&genpd->slock)
{
	unsigned long flags;

	spin_lock_irqsave(&genpd->slock, flags);
	genpd->lock_flags = flags;
}

static void genpd_lock_nested_spin(struct generic_pm_domain *genpd,
					int depth)
	__acquires(&genpd->slock)
{
	unsigned long flags;

	spin_lock_irqsave_nested(&genpd->slock, flags, depth);
	genpd->lock_flags = flags;
}

static int genpd_lock_interruptible_spin(struct generic_pm_domain *genpd)
	__acquires(&genpd->slock)
{
	unsigned long flags;

	spin_lock_irqsave(&genpd->slock, flags);
	genpd->lock_flags = flags;
	return 0;
}

static void genpd_unlock_spin(struct generic_pm_domain *genpd)
	__releases(&genpd->slock)
{
	spin_unlock_irqrestore(&genpd->slock, genpd->lock_flags);
}

static const struct genpd_lock_ops genpd_spin_ops = {
	.lock = genpd_lock_spin,
	.lock_nested = genpd_lock_nested_spin,
	.lock_interruptible = genpd_lock_interruptible_spin,
	.unlock = genpd_unlock_spin,
};

#define genpd_lock(p)			p->lock_ops->lock(p)
#define genpd_lock_nested(p, d)		p->lock_ops->lock_nested(p, d)
#define genpd_lock_interruptible(p)	p->lock_ops->lock_interruptible(p)
#define genpd_unlock(p)			p->lock_ops->unlock(p)

#define genpd_status_on(genpd)		(genpd->status == GENPD_STATE_ON)
#define genpd_is_irq_safe(genpd)	(genpd->flags & GENPD_FLAG_IRQ_SAFE)
#define genpd_is_always_on(genpd)	(genpd->flags & GENPD_FLAG_ALWAYS_ON)
#define genpd_is_active_wakeup(genpd)	(genpd->flags & GENPD_FLAG_ACTIVE_WAKEUP)
#define genpd_is_cpu_domain(genpd)	(genpd->flags & GENPD_FLAG_CPU_DOMAIN)
#define genpd_is_rpm_always_on(genpd)	(genpd->flags & GENPD_FLAG_RPM_ALWAYS_ON)

static inline bool irq_safe_dev_in_sleep_domain(struct device *dev,
		const struct generic_pm_domain *genpd)
{
	bool ret;

	ret = pm_runtime_is_irq_safe(dev) && !genpd_is_irq_safe(genpd);

	 
	if (genpd_is_always_on(genpd) || genpd_is_rpm_always_on(genpd))
		return ret;

	if (ret)
		dev_warn_once(dev, "PM domain %s will not be powered off\n",
				genpd->name);

	return ret;
}

static int genpd_runtime_suspend(struct device *dev);

 
static struct generic_pm_domain *dev_to_genpd_safe(struct device *dev)
{
	if (IS_ERR_OR_NULL(dev) || IS_ERR_OR_NULL(dev->pm_domain))
		return NULL;

	 
	if (dev->pm_domain->ops.runtime_suspend == genpd_runtime_suspend)
		return pd_to_genpd(dev->pm_domain);

	return NULL;
}

 
static struct generic_pm_domain *dev_to_genpd(struct device *dev)
{
	if (IS_ERR_OR_NULL(dev->pm_domain))
		return ERR_PTR(-EINVAL);

	return pd_to_genpd(dev->pm_domain);
}

static int genpd_stop_dev(const struct generic_pm_domain *genpd,
			  struct device *dev)
{
	return GENPD_DEV_CALLBACK(genpd, int, stop, dev);
}

static int genpd_start_dev(const struct generic_pm_domain *genpd,
			   struct device *dev)
{
	return GENPD_DEV_CALLBACK(genpd, int, start, dev);
}

static bool genpd_sd_counter_dec(struct generic_pm_domain *genpd)
{
	bool ret = false;

	if (!WARN_ON(atomic_read(&genpd->sd_count) == 0))
		ret = !!atomic_dec_and_test(&genpd->sd_count);

	return ret;
}

static void genpd_sd_counter_inc(struct generic_pm_domain *genpd)
{
	atomic_inc(&genpd->sd_count);
	smp_mb__after_atomic();
}

#ifdef CONFIG_DEBUG_FS
static struct dentry *genpd_debugfs_dir;

static void genpd_debug_add(struct generic_pm_domain *genpd);

static void genpd_debug_remove(struct generic_pm_domain *genpd)
{
	if (!genpd_debugfs_dir)
		return;

	debugfs_lookup_and_remove(genpd->name, genpd_debugfs_dir);
}

static void genpd_update_accounting(struct generic_pm_domain *genpd)
{
	u64 delta, now;

	now = ktime_get_mono_fast_ns();
	if (now <= genpd->accounting_time)
		return;

	delta = now - genpd->accounting_time;

	 
	if (genpd->status == GENPD_STATE_ON)
		genpd->states[genpd->state_idx].idle_time += delta;
	else
		genpd->on_time += delta;

	genpd->accounting_time = now;
}
#else
static inline void genpd_debug_add(struct generic_pm_domain *genpd) {}
static inline void genpd_debug_remove(struct generic_pm_domain *genpd) {}
static inline void genpd_update_accounting(struct generic_pm_domain *genpd) {}
#endif

static int _genpd_reeval_performance_state(struct generic_pm_domain *genpd,
					   unsigned int state)
{
	struct generic_pm_domain_data *pd_data;
	struct pm_domain_data *pdd;
	struct gpd_link *link;

	 
	if (state == genpd->performance_state)
		return state;

	 
	if (state > genpd->performance_state)
		return state;

	 
	list_for_each_entry(pdd, &genpd->dev_list, list_node) {
		pd_data = to_gpd_data(pdd);

		if (pd_data->performance_state > state)
			state = pd_data->performance_state;
	}

	 
	list_for_each_entry(link, &genpd->parent_links, parent_node) {
		if (link->performance_state > state)
			state = link->performance_state;
	}

	return state;
}

static int genpd_xlate_performance_state(struct generic_pm_domain *genpd,
					 struct generic_pm_domain *parent,
					 unsigned int pstate)
{
	if (!parent->set_performance_state)
		return pstate;

	return dev_pm_opp_xlate_performance_state(genpd->opp_table,
						  parent->opp_table,
						  pstate);
}

static int _genpd_set_performance_state(struct generic_pm_domain *genpd,
					unsigned int state, int depth)
{
	struct generic_pm_domain *parent;
	struct gpd_link *link;
	int parent_state, ret;

	if (state == genpd->performance_state)
		return 0;

	 
	list_for_each_entry(link, &genpd->child_links, child_node) {
		parent = link->parent;

		 
		ret = genpd_xlate_performance_state(genpd, parent, state);
		if (unlikely(ret < 0))
			goto err;

		parent_state = ret;

		genpd_lock_nested(parent, depth + 1);

		link->prev_performance_state = link->performance_state;
		link->performance_state = parent_state;
		parent_state = _genpd_reeval_performance_state(parent,
						parent_state);
		ret = _genpd_set_performance_state(parent, parent_state, depth + 1);
		if (ret)
			link->performance_state = link->prev_performance_state;

		genpd_unlock(parent);

		if (ret)
			goto err;
	}

	if (genpd->set_performance_state) {
		ret = genpd->set_performance_state(genpd, state);
		if (ret)
			goto err;
	}

	genpd->performance_state = state;
	return 0;

err:
	 
	list_for_each_entry_continue_reverse(link, &genpd->child_links,
					     child_node) {
		parent = link->parent;

		genpd_lock_nested(parent, depth + 1);

		parent_state = link->prev_performance_state;
		link->performance_state = parent_state;

		parent_state = _genpd_reeval_performance_state(parent,
						parent_state);
		if (_genpd_set_performance_state(parent, parent_state, depth + 1)) {
			pr_err("%s: Failed to roll back to %d performance state\n",
			       parent->name, parent_state);
		}

		genpd_unlock(parent);
	}

	return ret;
}

static int genpd_set_performance_state(struct device *dev, unsigned int state)
{
	struct generic_pm_domain *genpd = dev_to_genpd(dev);
	struct generic_pm_domain_data *gpd_data = dev_gpd_data(dev);
	unsigned int prev_state;
	int ret;

	prev_state = gpd_data->performance_state;
	if (prev_state == state)
		return 0;

	gpd_data->performance_state = state;
	state = _genpd_reeval_performance_state(genpd, state);

	ret = _genpd_set_performance_state(genpd, state, 0);
	if (ret)
		gpd_data->performance_state = prev_state;

	return ret;
}

static int genpd_drop_performance_state(struct device *dev)
{
	unsigned int prev_state = dev_gpd_data(dev)->performance_state;

	if (!genpd_set_performance_state(dev, 0))
		return prev_state;

	return 0;
}

static void genpd_restore_performance_state(struct device *dev,
					    unsigned int state)
{
	if (state)
		genpd_set_performance_state(dev, state);
}

 
int dev_pm_genpd_set_performance_state(struct device *dev, unsigned int state)
{
	struct generic_pm_domain *genpd;
	int ret = 0;

	genpd = dev_to_genpd_safe(dev);
	if (!genpd)
		return -ENODEV;

	if (WARN_ON(!dev->power.subsys_data ||
		     !dev->power.subsys_data->domain_data))
		return -EINVAL;

	genpd_lock(genpd);
	if (pm_runtime_suspended(dev)) {
		dev_gpd_data(dev)->rpm_pstate = state;
	} else {
		ret = genpd_set_performance_state(dev, state);
		if (!ret)
			dev_gpd_data(dev)->rpm_pstate = 0;
	}
	genpd_unlock(genpd);

	return ret;
}
EXPORT_SYMBOL_GPL(dev_pm_genpd_set_performance_state);

 
void dev_pm_genpd_set_next_wakeup(struct device *dev, ktime_t next)
{
	struct generic_pm_domain *genpd;
	struct gpd_timing_data *td;

	genpd = dev_to_genpd_safe(dev);
	if (!genpd)
		return;

	td = to_gpd_data(dev->power.subsys_data->domain_data)->td;
	if (td)
		td->next_wakeup = next;
}
EXPORT_SYMBOL_GPL(dev_pm_genpd_set_next_wakeup);

 
ktime_t dev_pm_genpd_get_next_hrtimer(struct device *dev)
{
	struct generic_pm_domain *genpd;

	genpd = dev_to_genpd_safe(dev);
	if (!genpd)
		return KTIME_MAX;

	if (genpd->gd)
		return genpd->gd->next_hrtimer;

	return KTIME_MAX;
}
EXPORT_SYMBOL_GPL(dev_pm_genpd_get_next_hrtimer);

 
void dev_pm_genpd_synced_poweroff(struct device *dev)
{
	struct generic_pm_domain *genpd;

	genpd = dev_to_genpd_safe(dev);
	if (!genpd)
		return;

	genpd_lock(genpd);
	genpd->synced_poweroff = true;
	genpd_unlock(genpd);
}
EXPORT_SYMBOL_GPL(dev_pm_genpd_synced_poweroff);

static int _genpd_power_on(struct generic_pm_domain *genpd, bool timed)
{
	unsigned int state_idx = genpd->state_idx;
	ktime_t time_start;
	s64 elapsed_ns;
	int ret;

	 
	ret = raw_notifier_call_chain_robust(&genpd->power_notifiers,
					     GENPD_NOTIFY_PRE_ON,
					     GENPD_NOTIFY_OFF, NULL);
	ret = notifier_to_errno(ret);
	if (ret)
		return ret;

	if (!genpd->power_on)
		goto out;

	timed = timed && genpd->gd && !genpd->states[state_idx].fwnode;
	if (!timed) {
		ret = genpd->power_on(genpd);
		if (ret)
			goto err;

		goto out;
	}

	time_start = ktime_get();
	ret = genpd->power_on(genpd);
	if (ret)
		goto err;

	elapsed_ns = ktime_to_ns(ktime_sub(ktime_get(), time_start));
	if (elapsed_ns <= genpd->states[state_idx].power_on_latency_ns)
		goto out;

	genpd->states[state_idx].power_on_latency_ns = elapsed_ns;
	genpd->gd->max_off_time_changed = true;
	pr_debug("%s: Power-%s latency exceeded, new value %lld ns\n",
		 genpd->name, "on", elapsed_ns);

out:
	raw_notifier_call_chain(&genpd->power_notifiers, GENPD_NOTIFY_ON, NULL);
	genpd->synced_poweroff = false;
	return 0;
err:
	raw_notifier_call_chain(&genpd->power_notifiers, GENPD_NOTIFY_OFF,
				NULL);
	return ret;
}

static int _genpd_power_off(struct generic_pm_domain *genpd, bool timed)
{
	unsigned int state_idx = genpd->state_idx;
	ktime_t time_start;
	s64 elapsed_ns;
	int ret;

	 
	ret = raw_notifier_call_chain_robust(&genpd->power_notifiers,
					     GENPD_NOTIFY_PRE_OFF,
					     GENPD_NOTIFY_ON, NULL);
	ret = notifier_to_errno(ret);
	if (ret)
		return ret;

	if (!genpd->power_off)
		goto out;

	timed = timed && genpd->gd && !genpd->states[state_idx].fwnode;
	if (!timed) {
		ret = genpd->power_off(genpd);
		if (ret)
			goto busy;

		goto out;
	}

	time_start = ktime_get();
	ret = genpd->power_off(genpd);
	if (ret)
		goto busy;

	elapsed_ns = ktime_to_ns(ktime_sub(ktime_get(), time_start));
	if (elapsed_ns <= genpd->states[state_idx].power_off_latency_ns)
		goto out;

	genpd->states[state_idx].power_off_latency_ns = elapsed_ns;
	genpd->gd->max_off_time_changed = true;
	pr_debug("%s: Power-%s latency exceeded, new value %lld ns\n",
		 genpd->name, "off", elapsed_ns);

out:
	raw_notifier_call_chain(&genpd->power_notifiers, GENPD_NOTIFY_OFF,
				NULL);
	return 0;
busy:
	raw_notifier_call_chain(&genpd->power_notifiers, GENPD_NOTIFY_ON, NULL);
	return ret;
}

 
static void genpd_queue_power_off_work(struct generic_pm_domain *genpd)
{
	queue_work(pm_wq, &genpd->power_off_work);
}

 
static int genpd_power_off(struct generic_pm_domain *genpd, bool one_dev_on,
			   unsigned int depth)
{
	struct pm_domain_data *pdd;
	struct gpd_link *link;
	unsigned int not_suspended = 0;
	int ret;

	 
	if (!genpd_status_on(genpd) || genpd->prepared_count > 0)
		return 0;

	 
	if (genpd_is_always_on(genpd) ||
			genpd_is_rpm_always_on(genpd) ||
			atomic_read(&genpd->sd_count) > 0)
		return -EBUSY;

	 
	list_for_each_entry(link, &genpd->parent_links, parent_node) {
		struct generic_pm_domain *child = link->child;
		if (child->state_idx < child->state_count - 1)
			return -EBUSY;
	}

	list_for_each_entry(pdd, &genpd->dev_list, list_node) {
		 
		if (!pm_runtime_suspended(pdd->dev) ||
			irq_safe_dev_in_sleep_domain(pdd->dev, genpd))
			not_suspended++;
	}

	if (not_suspended > 1 || (not_suspended == 1 && !one_dev_on))
		return -EBUSY;

	if (genpd->gov && genpd->gov->power_down_ok) {
		if (!genpd->gov->power_down_ok(&genpd->domain))
			return -EAGAIN;
	}

	 
	if (!genpd->gov)
		genpd->state_idx = 0;

	 
	if (atomic_read(&genpd->sd_count) > 0)
		return -EBUSY;

	ret = _genpd_power_off(genpd, true);
	if (ret) {
		genpd->states[genpd->state_idx].rejected++;
		return ret;
	}

	genpd->status = GENPD_STATE_OFF;
	genpd_update_accounting(genpd);
	genpd->states[genpd->state_idx].usage++;

	list_for_each_entry(link, &genpd->child_links, child_node) {
		genpd_sd_counter_dec(link->parent);
		genpd_lock_nested(link->parent, depth + 1);
		genpd_power_off(link->parent, false, depth + 1);
		genpd_unlock(link->parent);
	}

	return 0;
}

 
static int genpd_power_on(struct generic_pm_domain *genpd, unsigned int depth)
{
	struct gpd_link *link;
	int ret = 0;

	if (genpd_status_on(genpd))
		return 0;

	 
	list_for_each_entry(link, &genpd->child_links, child_node) {
		struct generic_pm_domain *parent = link->parent;

		genpd_sd_counter_inc(parent);

		genpd_lock_nested(parent, depth + 1);
		ret = genpd_power_on(parent, depth + 1);
		genpd_unlock(parent);

		if (ret) {
			genpd_sd_counter_dec(parent);
			goto err;
		}
	}

	ret = _genpd_power_on(genpd, true);
	if (ret)
		goto err;

	genpd->status = GENPD_STATE_ON;
	genpd_update_accounting(genpd);

	return 0;

 err:
	list_for_each_entry_continue_reverse(link,
					&genpd->child_links,
					child_node) {
		genpd_sd_counter_dec(link->parent);
		genpd_lock_nested(link->parent, depth + 1);
		genpd_power_off(link->parent, false, depth + 1);
		genpd_unlock(link->parent);
	}

	return ret;
}

static int genpd_dev_pm_start(struct device *dev)
{
	struct generic_pm_domain *genpd = dev_to_genpd(dev);

	return genpd_start_dev(genpd, dev);
}

static int genpd_dev_pm_qos_notifier(struct notifier_block *nb,
				     unsigned long val, void *ptr)
{
	struct generic_pm_domain_data *gpd_data;
	struct device *dev;

	gpd_data = container_of(nb, struct generic_pm_domain_data, nb);
	dev = gpd_data->base.dev;

	for (;;) {
		struct generic_pm_domain *genpd = ERR_PTR(-ENODATA);
		struct pm_domain_data *pdd;
		struct gpd_timing_data *td;

		spin_lock_irq(&dev->power.lock);

		pdd = dev->power.subsys_data ?
				dev->power.subsys_data->domain_data : NULL;
		if (pdd) {
			td = to_gpd_data(pdd)->td;
			if (td) {
				td->constraint_changed = true;
				genpd = dev_to_genpd(dev);
			}
		}

		spin_unlock_irq(&dev->power.lock);

		if (!IS_ERR(genpd)) {
			genpd_lock(genpd);
			genpd->gd->max_off_time_changed = true;
			genpd_unlock(genpd);
		}

		dev = dev->parent;
		if (!dev || dev->power.ignore_children)
			break;
	}

	return NOTIFY_DONE;
}

 
static void genpd_power_off_work_fn(struct work_struct *work)
{
	struct generic_pm_domain *genpd;

	genpd = container_of(work, struct generic_pm_domain, power_off_work);

	genpd_lock(genpd);
	genpd_power_off(genpd, false, 0);
	genpd_unlock(genpd);
}

 
static int __genpd_runtime_suspend(struct device *dev)
{
	int (*cb)(struct device *__dev);

	if (dev->type && dev->type->pm)
		cb = dev->type->pm->runtime_suspend;
	else if (dev->class && dev->class->pm)
		cb = dev->class->pm->runtime_suspend;
	else if (dev->bus && dev->bus->pm)
		cb = dev->bus->pm->runtime_suspend;
	else
		cb = NULL;

	if (!cb && dev->driver && dev->driver->pm)
		cb = dev->driver->pm->runtime_suspend;

	return cb ? cb(dev) : 0;
}

 
static int __genpd_runtime_resume(struct device *dev)
{
	int (*cb)(struct device *__dev);

	if (dev->type && dev->type->pm)
		cb = dev->type->pm->runtime_resume;
	else if (dev->class && dev->class->pm)
		cb = dev->class->pm->runtime_resume;
	else if (dev->bus && dev->bus->pm)
		cb = dev->bus->pm->runtime_resume;
	else
		cb = NULL;

	if (!cb && dev->driver && dev->driver->pm)
		cb = dev->driver->pm->runtime_resume;

	return cb ? cb(dev) : 0;
}

 
static int genpd_runtime_suspend(struct device *dev)
{
	struct generic_pm_domain *genpd;
	bool (*suspend_ok)(struct device *__dev);
	struct generic_pm_domain_data *gpd_data = dev_gpd_data(dev);
	struct gpd_timing_data *td = gpd_data->td;
	bool runtime_pm = pm_runtime_enabled(dev);
	ktime_t time_start = 0;
	s64 elapsed_ns;
	int ret;

	dev_dbg(dev, "%s()\n", __func__);

	genpd = dev_to_genpd(dev);
	if (IS_ERR(genpd))
		return -EINVAL;

	 
	suspend_ok = genpd->gov ? genpd->gov->suspend_ok : NULL;
	if (runtime_pm && suspend_ok && !suspend_ok(dev))
		return -EBUSY;

	 
	if (td && runtime_pm)
		time_start = ktime_get();

	ret = __genpd_runtime_suspend(dev);
	if (ret)
		return ret;

	ret = genpd_stop_dev(genpd, dev);
	if (ret) {
		__genpd_runtime_resume(dev);
		return ret;
	}

	 
	if (td && runtime_pm) {
		elapsed_ns = ktime_to_ns(ktime_sub(ktime_get(), time_start));
		if (elapsed_ns > td->suspend_latency_ns) {
			td->suspend_latency_ns = elapsed_ns;
			dev_dbg(dev, "suspend latency exceeded, %lld ns\n",
				elapsed_ns);
			genpd->gd->max_off_time_changed = true;
			td->constraint_changed = true;
		}
	}

	 
	if (irq_safe_dev_in_sleep_domain(dev, genpd))
		return 0;

	genpd_lock(genpd);
	genpd_power_off(genpd, true, 0);
	gpd_data->rpm_pstate = genpd_drop_performance_state(dev);
	genpd_unlock(genpd);

	return 0;
}

 
static int genpd_runtime_resume(struct device *dev)
{
	struct generic_pm_domain *genpd;
	struct generic_pm_domain_data *gpd_data = dev_gpd_data(dev);
	struct gpd_timing_data *td = gpd_data->td;
	bool timed = td && pm_runtime_enabled(dev);
	ktime_t time_start = 0;
	s64 elapsed_ns;
	int ret;

	dev_dbg(dev, "%s()\n", __func__);

	genpd = dev_to_genpd(dev);
	if (IS_ERR(genpd))
		return -EINVAL;

	 
	if (irq_safe_dev_in_sleep_domain(dev, genpd))
		goto out;

	genpd_lock(genpd);
	genpd_restore_performance_state(dev, gpd_data->rpm_pstate);
	ret = genpd_power_on(genpd, 0);
	genpd_unlock(genpd);

	if (ret)
		return ret;

 out:
	 
	if (timed)
		time_start = ktime_get();

	ret = genpd_start_dev(genpd, dev);
	if (ret)
		goto err_poweroff;

	ret = __genpd_runtime_resume(dev);
	if (ret)
		goto err_stop;

	 
	if (timed) {
		elapsed_ns = ktime_to_ns(ktime_sub(ktime_get(), time_start));
		if (elapsed_ns > td->resume_latency_ns) {
			td->resume_latency_ns = elapsed_ns;
			dev_dbg(dev, "resume latency exceeded, %lld ns\n",
				elapsed_ns);
			genpd->gd->max_off_time_changed = true;
			td->constraint_changed = true;
		}
	}

	return 0;

err_stop:
	genpd_stop_dev(genpd, dev);
err_poweroff:
	if (!pm_runtime_is_irq_safe(dev) || genpd_is_irq_safe(genpd)) {
		genpd_lock(genpd);
		genpd_power_off(genpd, true, 0);
		gpd_data->rpm_pstate = genpd_drop_performance_state(dev);
		genpd_unlock(genpd);
	}

	return ret;
}

static bool pd_ignore_unused;
static int __init pd_ignore_unused_setup(char *__unused)
{
	pd_ignore_unused = true;
	return 1;
}
__setup("pd_ignore_unused", pd_ignore_unused_setup);

 
static int __init genpd_power_off_unused(void)
{
	struct generic_pm_domain *genpd;

	if (pd_ignore_unused) {
		pr_warn("genpd: Not disabling unused power domains\n");
		return 0;
	}

	mutex_lock(&gpd_list_lock);

	list_for_each_entry(genpd, &gpd_list, gpd_list_node)
		genpd_queue_power_off_work(genpd);

	mutex_unlock(&gpd_list_lock);

	return 0;
}
late_initcall(genpd_power_off_unused);

#ifdef CONFIG_PM_SLEEP

 
static void genpd_sync_power_off(struct generic_pm_domain *genpd, bool use_lock,
				 unsigned int depth)
{
	struct gpd_link *link;

	if (!genpd_status_on(genpd) || genpd_is_always_on(genpd))
		return;

	if (genpd->suspended_count != genpd->device_count
	    || atomic_read(&genpd->sd_count) > 0)
		return;

	 
	list_for_each_entry(link, &genpd->parent_links, parent_node) {
		struct generic_pm_domain *child = link->child;
		if (child->state_idx < child->state_count - 1)
			return;
	}

	 
	genpd->state_idx = genpd->state_count - 1;
	if (_genpd_power_off(genpd, false))
		return;

	genpd->status = GENPD_STATE_OFF;

	list_for_each_entry(link, &genpd->child_links, child_node) {
		genpd_sd_counter_dec(link->parent);

		if (use_lock)
			genpd_lock_nested(link->parent, depth + 1);

		genpd_sync_power_off(link->parent, use_lock, depth + 1);

		if (use_lock)
			genpd_unlock(link->parent);
	}
}

 
static void genpd_sync_power_on(struct generic_pm_domain *genpd, bool use_lock,
				unsigned int depth)
{
	struct gpd_link *link;

	if (genpd_status_on(genpd))
		return;

	list_for_each_entry(link, &genpd->child_links, child_node) {
		genpd_sd_counter_inc(link->parent);

		if (use_lock)
			genpd_lock_nested(link->parent, depth + 1);

		genpd_sync_power_on(link->parent, use_lock, depth + 1);

		if (use_lock)
			genpd_unlock(link->parent);
	}

	_genpd_power_on(genpd, false);
	genpd->status = GENPD_STATE_ON;
}

 
static int genpd_prepare(struct device *dev)
{
	struct generic_pm_domain *genpd;
	int ret;

	dev_dbg(dev, "%s()\n", __func__);

	genpd = dev_to_genpd(dev);
	if (IS_ERR(genpd))
		return -EINVAL;

	genpd_lock(genpd);

	if (genpd->prepared_count++ == 0)
		genpd->suspended_count = 0;

	genpd_unlock(genpd);

	ret = pm_generic_prepare(dev);
	if (ret < 0) {
		genpd_lock(genpd);

		genpd->prepared_count--;

		genpd_unlock(genpd);
	}

	 
	return ret >= 0 ? 0 : ret;
}

 
static int genpd_finish_suspend(struct device *dev,
				int (*suspend_noirq)(struct device *dev),
				int (*resume_noirq)(struct device *dev))
{
	struct generic_pm_domain *genpd;
	int ret = 0;

	genpd = dev_to_genpd(dev);
	if (IS_ERR(genpd))
		return -EINVAL;

	ret = suspend_noirq(dev);
	if (ret)
		return ret;

	if (device_wakeup_path(dev) && genpd_is_active_wakeup(genpd))
		return 0;

	if (genpd->dev_ops.stop && genpd->dev_ops.start &&
	    !pm_runtime_status_suspended(dev)) {
		ret = genpd_stop_dev(genpd, dev);
		if (ret) {
			resume_noirq(dev);
			return ret;
		}
	}

	genpd_lock(genpd);
	genpd->suspended_count++;
	genpd_sync_power_off(genpd, true, 0);
	genpd_unlock(genpd);

	return 0;
}

 
static int genpd_suspend_noirq(struct device *dev)
{
	dev_dbg(dev, "%s()\n", __func__);

	return genpd_finish_suspend(dev,
				    pm_generic_suspend_noirq,
				    pm_generic_resume_noirq);
}

 
static int genpd_finish_resume(struct device *dev,
			       int (*resume_noirq)(struct device *dev))
{
	struct generic_pm_domain *genpd;
	int ret;

	dev_dbg(dev, "%s()\n", __func__);

	genpd = dev_to_genpd(dev);
	if (IS_ERR(genpd))
		return -EINVAL;

	if (device_wakeup_path(dev) && genpd_is_active_wakeup(genpd))
		return resume_noirq(dev);

	genpd_lock(genpd);
	genpd_sync_power_on(genpd, true, 0);
	genpd->suspended_count--;
	genpd_unlock(genpd);

	if (genpd->dev_ops.stop && genpd->dev_ops.start &&
	    !pm_runtime_status_suspended(dev)) {
		ret = genpd_start_dev(genpd, dev);
		if (ret)
			return ret;
	}

	return pm_generic_resume_noirq(dev);
}

 
static int genpd_resume_noirq(struct device *dev)
{
	dev_dbg(dev, "%s()\n", __func__);

	return genpd_finish_resume(dev, pm_generic_resume_noirq);
}

 
static int genpd_freeze_noirq(struct device *dev)
{
	dev_dbg(dev, "%s()\n", __func__);

	return genpd_finish_suspend(dev,
				    pm_generic_freeze_noirq,
				    pm_generic_thaw_noirq);
}

 
static int genpd_thaw_noirq(struct device *dev)
{
	dev_dbg(dev, "%s()\n", __func__);

	return genpd_finish_resume(dev, pm_generic_thaw_noirq);
}

 
static int genpd_poweroff_noirq(struct device *dev)
{
	dev_dbg(dev, "%s()\n", __func__);

	return genpd_finish_suspend(dev,
				    pm_generic_poweroff_noirq,
				    pm_generic_restore_noirq);
}

 
static int genpd_restore_noirq(struct device *dev)
{
	dev_dbg(dev, "%s()\n", __func__);

	return genpd_finish_resume(dev, pm_generic_restore_noirq);
}

 
static void genpd_complete(struct device *dev)
{
	struct generic_pm_domain *genpd;

	dev_dbg(dev, "%s()\n", __func__);

	genpd = dev_to_genpd(dev);
	if (IS_ERR(genpd))
		return;

	pm_generic_complete(dev);

	genpd_lock(genpd);

	genpd->prepared_count--;
	if (!genpd->prepared_count)
		genpd_queue_power_off_work(genpd);

	genpd_unlock(genpd);
}

static void genpd_switch_state(struct device *dev, bool suspend)
{
	struct generic_pm_domain *genpd;
	bool use_lock;

	genpd = dev_to_genpd_safe(dev);
	if (!genpd)
		return;

	use_lock = genpd_is_irq_safe(genpd);

	if (use_lock)
		genpd_lock(genpd);

	if (suspend) {
		genpd->suspended_count++;
		genpd_sync_power_off(genpd, use_lock, 0);
	} else {
		genpd_sync_power_on(genpd, use_lock, 0);
		genpd->suspended_count--;
	}

	if (use_lock)
		genpd_unlock(genpd);
}

 
void dev_pm_genpd_suspend(struct device *dev)
{
	genpd_switch_state(dev, true);
}
EXPORT_SYMBOL_GPL(dev_pm_genpd_suspend);

 
void dev_pm_genpd_resume(struct device *dev)
{
	genpd_switch_state(dev, false);
}
EXPORT_SYMBOL_GPL(dev_pm_genpd_resume);

#else  

#define genpd_prepare		NULL
#define genpd_suspend_noirq	NULL
#define genpd_resume_noirq	NULL
#define genpd_freeze_noirq	NULL
#define genpd_thaw_noirq	NULL
#define genpd_poweroff_noirq	NULL
#define genpd_restore_noirq	NULL
#define genpd_complete		NULL

#endif  

static struct generic_pm_domain_data *genpd_alloc_dev_data(struct device *dev,
							   bool has_governor)
{
	struct generic_pm_domain_data *gpd_data;
	struct gpd_timing_data *td;
	int ret;

	ret = dev_pm_get_subsys_data(dev);
	if (ret)
		return ERR_PTR(ret);

	gpd_data = kzalloc(sizeof(*gpd_data), GFP_KERNEL);
	if (!gpd_data) {
		ret = -ENOMEM;
		goto err_put;
	}

	gpd_data->base.dev = dev;
	gpd_data->nb.notifier_call = genpd_dev_pm_qos_notifier;

	 
	if (has_governor) {
		td = kzalloc(sizeof(*td), GFP_KERNEL);
		if (!td) {
			ret = -ENOMEM;
			goto err_free;
		}

		td->constraint_changed = true;
		td->effective_constraint_ns = PM_QOS_RESUME_LATENCY_NO_CONSTRAINT_NS;
		td->next_wakeup = KTIME_MAX;
		gpd_data->td = td;
	}

	spin_lock_irq(&dev->power.lock);

	if (dev->power.subsys_data->domain_data)
		ret = -EINVAL;
	else
		dev->power.subsys_data->domain_data = &gpd_data->base;

	spin_unlock_irq(&dev->power.lock);

	if (ret)
		goto err_free;

	return gpd_data;

 err_free:
	kfree(gpd_data->td);
	kfree(gpd_data);
 err_put:
	dev_pm_put_subsys_data(dev);
	return ERR_PTR(ret);
}

static void genpd_free_dev_data(struct device *dev,
				struct generic_pm_domain_data *gpd_data)
{
	spin_lock_irq(&dev->power.lock);

	dev->power.subsys_data->domain_data = NULL;

	spin_unlock_irq(&dev->power.lock);

	kfree(gpd_data->td);
	kfree(gpd_data);
	dev_pm_put_subsys_data(dev);
}

static void genpd_update_cpumask(struct generic_pm_domain *genpd,
				 int cpu, bool set, unsigned int depth)
{
	struct gpd_link *link;

	if (!genpd_is_cpu_domain(genpd))
		return;

	list_for_each_entry(link, &genpd->child_links, child_node) {
		struct generic_pm_domain *parent = link->parent;

		genpd_lock_nested(parent, depth + 1);
		genpd_update_cpumask(parent, cpu, set, depth + 1);
		genpd_unlock(parent);
	}

	if (set)
		cpumask_set_cpu(cpu, genpd->cpus);
	else
		cpumask_clear_cpu(cpu, genpd->cpus);
}

static void genpd_set_cpumask(struct generic_pm_domain *genpd, int cpu)
{
	if (cpu >= 0)
		genpd_update_cpumask(genpd, cpu, true, 0);
}

static void genpd_clear_cpumask(struct generic_pm_domain *genpd, int cpu)
{
	if (cpu >= 0)
		genpd_update_cpumask(genpd, cpu, false, 0);
}

static int genpd_get_cpu(struct generic_pm_domain *genpd, struct device *dev)
{
	int cpu;

	if (!genpd_is_cpu_domain(genpd))
		return -1;

	for_each_possible_cpu(cpu) {
		if (get_cpu_device(cpu) == dev)
			return cpu;
	}

	return -1;
}

static int genpd_add_device(struct generic_pm_domain *genpd, struct device *dev,
			    struct device *base_dev)
{
	struct genpd_governor_data *gd = genpd->gd;
	struct generic_pm_domain_data *gpd_data;
	int ret;

	dev_dbg(dev, "%s()\n", __func__);

	gpd_data = genpd_alloc_dev_data(dev, gd);
	if (IS_ERR(gpd_data))
		return PTR_ERR(gpd_data);

	gpd_data->cpu = genpd_get_cpu(genpd, base_dev);

	ret = genpd->attach_dev ? genpd->attach_dev(genpd, dev) : 0;
	if (ret)
		goto out;

	genpd_lock(genpd);

	genpd_set_cpumask(genpd, gpd_data->cpu);
	dev_pm_domain_set(dev, &genpd->domain);

	genpd->device_count++;
	if (gd)
		gd->max_off_time_changed = true;

	list_add_tail(&gpd_data->base.list_node, &genpd->dev_list);

	genpd_unlock(genpd);
 out:
	if (ret)
		genpd_free_dev_data(dev, gpd_data);
	else
		dev_pm_qos_add_notifier(dev, &gpd_data->nb,
					DEV_PM_QOS_RESUME_LATENCY);

	return ret;
}

 
int pm_genpd_add_device(struct generic_pm_domain *genpd, struct device *dev)
{
	int ret;

	if (!genpd || !dev)
		return -EINVAL;

	mutex_lock(&gpd_list_lock);
	ret = genpd_add_device(genpd, dev, dev);
	mutex_unlock(&gpd_list_lock);

	return ret;
}
EXPORT_SYMBOL_GPL(pm_genpd_add_device);

static int genpd_remove_device(struct generic_pm_domain *genpd,
			       struct device *dev)
{
	struct generic_pm_domain_data *gpd_data;
	struct pm_domain_data *pdd;
	int ret = 0;

	dev_dbg(dev, "%s()\n", __func__);

	pdd = dev->power.subsys_data->domain_data;
	gpd_data = to_gpd_data(pdd);
	dev_pm_qos_remove_notifier(dev, &gpd_data->nb,
				   DEV_PM_QOS_RESUME_LATENCY);

	genpd_lock(genpd);

	if (genpd->prepared_count > 0) {
		ret = -EAGAIN;
		goto out;
	}

	genpd->device_count--;
	if (genpd->gd)
		genpd->gd->max_off_time_changed = true;

	genpd_clear_cpumask(genpd, gpd_data->cpu);
	dev_pm_domain_set(dev, NULL);

	list_del_init(&pdd->list_node);

	genpd_unlock(genpd);

	if (genpd->detach_dev)
		genpd->detach_dev(genpd, dev);

	genpd_free_dev_data(dev, gpd_data);

	return 0;

 out:
	genpd_unlock(genpd);
	dev_pm_qos_add_notifier(dev, &gpd_data->nb, DEV_PM_QOS_RESUME_LATENCY);

	return ret;
}

 
int pm_genpd_remove_device(struct device *dev)
{
	struct generic_pm_domain *genpd = dev_to_genpd_safe(dev);

	if (!genpd)
		return -EINVAL;

	return genpd_remove_device(genpd, dev);
}
EXPORT_SYMBOL_GPL(pm_genpd_remove_device);

 
int dev_pm_genpd_add_notifier(struct device *dev, struct notifier_block *nb)
{
	struct generic_pm_domain *genpd;
	struct generic_pm_domain_data *gpd_data;
	int ret;

	genpd = dev_to_genpd_safe(dev);
	if (!genpd)
		return -ENODEV;

	if (WARN_ON(!dev->power.subsys_data ||
		     !dev->power.subsys_data->domain_data))
		return -EINVAL;

	gpd_data = to_gpd_data(dev->power.subsys_data->domain_data);
	if (gpd_data->power_nb)
		return -EEXIST;

	genpd_lock(genpd);
	ret = raw_notifier_chain_register(&genpd->power_notifiers, nb);
	genpd_unlock(genpd);

	if (ret) {
		dev_warn(dev, "failed to add notifier for PM domain %s\n",
			 genpd->name);
		return ret;
	}

	gpd_data->power_nb = nb;
	return 0;
}
EXPORT_SYMBOL_GPL(dev_pm_genpd_add_notifier);

 
int dev_pm_genpd_remove_notifier(struct device *dev)
{
	struct generic_pm_domain *genpd;
	struct generic_pm_domain_data *gpd_data;
	int ret;

	genpd = dev_to_genpd_safe(dev);
	if (!genpd)
		return -ENODEV;

	if (WARN_ON(!dev->power.subsys_data ||
		     !dev->power.subsys_data->domain_data))
		return -EINVAL;

	gpd_data = to_gpd_data(dev->power.subsys_data->domain_data);
	if (!gpd_data->power_nb)
		return -ENODEV;

	genpd_lock(genpd);
	ret = raw_notifier_chain_unregister(&genpd->power_notifiers,
					    gpd_data->power_nb);
	genpd_unlock(genpd);

	if (ret) {
		dev_warn(dev, "failed to remove notifier for PM domain %s\n",
			 genpd->name);
		return ret;
	}

	gpd_data->power_nb = NULL;
	return 0;
}
EXPORT_SYMBOL_GPL(dev_pm_genpd_remove_notifier);

static int genpd_add_subdomain(struct generic_pm_domain *genpd,
			       struct generic_pm_domain *subdomain)
{
	struct gpd_link *link, *itr;
	int ret = 0;

	if (IS_ERR_OR_NULL(genpd) || IS_ERR_OR_NULL(subdomain)
	    || genpd == subdomain)
		return -EINVAL;

	 
	if (!genpd_is_irq_safe(genpd) && genpd_is_irq_safe(subdomain)) {
		WARN(1, "Parent %s of subdomain %s must be IRQ safe\n",
				genpd->name, subdomain->name);
		return -EINVAL;
	}

	link = kzalloc(sizeof(*link), GFP_KERNEL);
	if (!link)
		return -ENOMEM;

	genpd_lock(subdomain);
	genpd_lock_nested(genpd, SINGLE_DEPTH_NESTING);

	if (!genpd_status_on(genpd) && genpd_status_on(subdomain)) {
		ret = -EINVAL;
		goto out;
	}

	list_for_each_entry(itr, &genpd->parent_links, parent_node) {
		if (itr->child == subdomain && itr->parent == genpd) {
			ret = -EINVAL;
			goto out;
		}
	}

	link->parent = genpd;
	list_add_tail(&link->parent_node, &genpd->parent_links);
	link->child = subdomain;
	list_add_tail(&link->child_node, &subdomain->child_links);
	if (genpd_status_on(subdomain))
		genpd_sd_counter_inc(genpd);

 out:
	genpd_unlock(genpd);
	genpd_unlock(subdomain);
	if (ret)
		kfree(link);
	return ret;
}

 
int pm_genpd_add_subdomain(struct generic_pm_domain *genpd,
			   struct generic_pm_domain *subdomain)
{
	int ret;

	mutex_lock(&gpd_list_lock);
	ret = genpd_add_subdomain(genpd, subdomain);
	mutex_unlock(&gpd_list_lock);

	return ret;
}
EXPORT_SYMBOL_GPL(pm_genpd_add_subdomain);

 
int pm_genpd_remove_subdomain(struct generic_pm_domain *genpd,
			      struct generic_pm_domain *subdomain)
{
	struct gpd_link *l, *link;
	int ret = -EINVAL;

	if (IS_ERR_OR_NULL(genpd) || IS_ERR_OR_NULL(subdomain))
		return -EINVAL;

	genpd_lock(subdomain);
	genpd_lock_nested(genpd, SINGLE_DEPTH_NESTING);

	if (!list_empty(&subdomain->parent_links) || subdomain->device_count) {
		pr_warn("%s: unable to remove subdomain %s\n",
			genpd->name, subdomain->name);
		ret = -EBUSY;
		goto out;
	}

	list_for_each_entry_safe(link, l, &genpd->parent_links, parent_node) {
		if (link->child != subdomain)
			continue;

		list_del(&link->parent_node);
		list_del(&link->child_node);
		kfree(link);
		if (genpd_status_on(subdomain))
			genpd_sd_counter_dec(genpd);

		ret = 0;
		break;
	}

out:
	genpd_unlock(genpd);
	genpd_unlock(subdomain);

	return ret;
}
EXPORT_SYMBOL_GPL(pm_genpd_remove_subdomain);

static void genpd_free_default_power_state(struct genpd_power_state *states,
					   unsigned int state_count)
{
	kfree(states);
}

static int genpd_set_default_power_state(struct generic_pm_domain *genpd)
{
	struct genpd_power_state *state;

	state = kzalloc(sizeof(*state), GFP_KERNEL);
	if (!state)
		return -ENOMEM;

	genpd->states = state;
	genpd->state_count = 1;
	genpd->free_states = genpd_free_default_power_state;

	return 0;
}

static int genpd_alloc_data(struct generic_pm_domain *genpd)
{
	struct genpd_governor_data *gd = NULL;
	int ret;

	if (genpd_is_cpu_domain(genpd) &&
	    !zalloc_cpumask_var(&genpd->cpus, GFP_KERNEL))
		return -ENOMEM;

	if (genpd->gov) {
		gd = kzalloc(sizeof(*gd), GFP_KERNEL);
		if (!gd) {
			ret = -ENOMEM;
			goto free;
		}

		gd->max_off_time_ns = -1;
		gd->max_off_time_changed = true;
		gd->next_wakeup = KTIME_MAX;
		gd->next_hrtimer = KTIME_MAX;
	}

	 
	if (genpd->state_count == 0) {
		ret = genpd_set_default_power_state(genpd);
		if (ret)
			goto free;
	}

	genpd->gd = gd;
	return 0;

free:
	if (genpd_is_cpu_domain(genpd))
		free_cpumask_var(genpd->cpus);
	kfree(gd);
	return ret;
}

static void genpd_free_data(struct generic_pm_domain *genpd)
{
	if (genpd_is_cpu_domain(genpd))
		free_cpumask_var(genpd->cpus);
	if (genpd->free_states)
		genpd->free_states(genpd->states, genpd->state_count);
	kfree(genpd->gd);
}

static void genpd_lock_init(struct generic_pm_domain *genpd)
{
	if (genpd->flags & GENPD_FLAG_IRQ_SAFE) {
		spin_lock_init(&genpd->slock);
		genpd->lock_ops = &genpd_spin_ops;
	} else {
		mutex_init(&genpd->mlock);
		genpd->lock_ops = &genpd_mtx_ops;
	}
}

 
int pm_genpd_init(struct generic_pm_domain *genpd,
		  struct dev_power_governor *gov, bool is_off)
{
	int ret;

	if (IS_ERR_OR_NULL(genpd))
		return -EINVAL;

	INIT_LIST_HEAD(&genpd->parent_links);
	INIT_LIST_HEAD(&genpd->child_links);
	INIT_LIST_HEAD(&genpd->dev_list);
	RAW_INIT_NOTIFIER_HEAD(&genpd->power_notifiers);
	genpd_lock_init(genpd);
	genpd->gov = gov;
	INIT_WORK(&genpd->power_off_work, genpd_power_off_work_fn);
	atomic_set(&genpd->sd_count, 0);
	genpd->status = is_off ? GENPD_STATE_OFF : GENPD_STATE_ON;
	genpd->device_count = 0;
	genpd->provider = NULL;
	genpd->has_provider = false;
	genpd->accounting_time = ktime_get_mono_fast_ns();
	genpd->domain.ops.runtime_suspend = genpd_runtime_suspend;
	genpd->domain.ops.runtime_resume = genpd_runtime_resume;
	genpd->domain.ops.prepare = genpd_prepare;
	genpd->domain.ops.suspend_noirq = genpd_suspend_noirq;
	genpd->domain.ops.resume_noirq = genpd_resume_noirq;
	genpd->domain.ops.freeze_noirq = genpd_freeze_noirq;
	genpd->domain.ops.thaw_noirq = genpd_thaw_noirq;
	genpd->domain.ops.poweroff_noirq = genpd_poweroff_noirq;
	genpd->domain.ops.restore_noirq = genpd_restore_noirq;
	genpd->domain.ops.complete = genpd_complete;
	genpd->domain.start = genpd_dev_pm_start;

	if (genpd->flags & GENPD_FLAG_PM_CLK) {
		genpd->dev_ops.stop = pm_clk_suspend;
		genpd->dev_ops.start = pm_clk_resume;
	}

	 
	if (gov == &pm_domain_always_on_gov)
		genpd->flags |= GENPD_FLAG_RPM_ALWAYS_ON;

	 
	if ((genpd_is_always_on(genpd) || genpd_is_rpm_always_on(genpd)) &&
			!genpd_status_on(genpd)) {
		pr_err("always-on PM domain %s is not on\n", genpd->name);
		return -EINVAL;
	}

	 
	if (!gov && genpd->state_count > 1)
		pr_warn("%s: no governor for states\n", genpd->name);

	ret = genpd_alloc_data(genpd);
	if (ret)
		return ret;

	device_initialize(&genpd->dev);
	dev_set_name(&genpd->dev, "%s", genpd->name);

	mutex_lock(&gpd_list_lock);
	list_add(&genpd->gpd_list_node, &gpd_list);
	mutex_unlock(&gpd_list_lock);
	genpd_debug_add(genpd);

	return 0;
}
EXPORT_SYMBOL_GPL(pm_genpd_init);

static int genpd_remove(struct generic_pm_domain *genpd)
{
	struct gpd_link *l, *link;

	if (IS_ERR_OR_NULL(genpd))
		return -EINVAL;

	genpd_lock(genpd);

	if (genpd->has_provider) {
		genpd_unlock(genpd);
		pr_err("Provider present, unable to remove %s\n", genpd->name);
		return -EBUSY;
	}

	if (!list_empty(&genpd->parent_links) || genpd->device_count) {
		genpd_unlock(genpd);
		pr_err("%s: unable to remove %s\n", __func__, genpd->name);
		return -EBUSY;
	}

	list_for_each_entry_safe(link, l, &genpd->child_links, child_node) {
		list_del(&link->parent_node);
		list_del(&link->child_node);
		kfree(link);
	}

	list_del(&genpd->gpd_list_node);
	genpd_unlock(genpd);
	genpd_debug_remove(genpd);
	cancel_work_sync(&genpd->power_off_work);
	genpd_free_data(genpd);

	pr_debug("%s: removed %s\n", __func__, genpd->name);

	return 0;
}

 
int pm_genpd_remove(struct generic_pm_domain *genpd)
{
	int ret;

	mutex_lock(&gpd_list_lock);
	ret = genpd_remove(genpd);
	mutex_unlock(&gpd_list_lock);

	return ret;
}
EXPORT_SYMBOL_GPL(pm_genpd_remove);

#ifdef CONFIG_PM_GENERIC_DOMAINS_OF

 

 
struct of_genpd_provider {
	struct list_head link;
	struct device_node *node;
	genpd_xlate_t xlate;
	void *data;
};

 
static LIST_HEAD(of_genpd_providers);
 
static DEFINE_MUTEX(of_genpd_mutex);

 
static struct generic_pm_domain *genpd_xlate_simple(
					struct of_phandle_args *genpdspec,
					void *data)
{
	return data;
}

 
static struct generic_pm_domain *genpd_xlate_onecell(
					struct of_phandle_args *genpdspec,
					void *data)
{
	struct genpd_onecell_data *genpd_data = data;
	unsigned int idx = genpdspec->args[0];

	if (genpdspec->args_count != 1)
		return ERR_PTR(-EINVAL);

	if (idx >= genpd_data->num_domains) {
		pr_err("%s: invalid domain index %u\n", __func__, idx);
		return ERR_PTR(-EINVAL);
	}

	if (!genpd_data->domains[idx])
		return ERR_PTR(-ENOENT);

	return genpd_data->domains[idx];
}

 
static int genpd_add_provider(struct device_node *np, genpd_xlate_t xlate,
			      void *data)
{
	struct of_genpd_provider *cp;

	cp = kzalloc(sizeof(*cp), GFP_KERNEL);
	if (!cp)
		return -ENOMEM;

	cp->node = of_node_get(np);
	cp->data = data;
	cp->xlate = xlate;
	fwnode_dev_initialized(&np->fwnode, true);

	mutex_lock(&of_genpd_mutex);
	list_add(&cp->link, &of_genpd_providers);
	mutex_unlock(&of_genpd_mutex);
	pr_debug("Added domain provider from %pOF\n", np);

	return 0;
}

static bool genpd_present(const struct generic_pm_domain *genpd)
{
	bool ret = false;
	const struct generic_pm_domain *gpd;

	mutex_lock(&gpd_list_lock);
	list_for_each_entry(gpd, &gpd_list, gpd_list_node) {
		if (gpd == genpd) {
			ret = true;
			break;
		}
	}
	mutex_unlock(&gpd_list_lock);

	return ret;
}

 
int of_genpd_add_provider_simple(struct device_node *np,
				 struct generic_pm_domain *genpd)
{
	int ret;

	if (!np || !genpd)
		return -EINVAL;

	if (!genpd_present(genpd))
		return -EINVAL;

	genpd->dev.of_node = np;

	 
	if (genpd->set_performance_state) {
		ret = dev_pm_opp_of_add_table(&genpd->dev);
		if (ret)
			return dev_err_probe(&genpd->dev, ret, "Failed to add OPP table\n");

		 
		genpd->opp_table = dev_pm_opp_get_opp_table(&genpd->dev);
		WARN_ON(IS_ERR(genpd->opp_table));
	}

	ret = genpd_add_provider(np, genpd_xlate_simple, genpd);
	if (ret) {
		if (genpd->set_performance_state) {
			dev_pm_opp_put_opp_table(genpd->opp_table);
			dev_pm_opp_of_remove_table(&genpd->dev);
		}

		return ret;
	}

	genpd->provider = &np->fwnode;
	genpd->has_provider = true;

	return 0;
}
EXPORT_SYMBOL_GPL(of_genpd_add_provider_simple);

 
int of_genpd_add_provider_onecell(struct device_node *np,
				  struct genpd_onecell_data *data)
{
	struct generic_pm_domain *genpd;
	unsigned int i;
	int ret = -EINVAL;

	if (!np || !data)
		return -EINVAL;

	if (!data->xlate)
		data->xlate = genpd_xlate_onecell;

	for (i = 0; i < data->num_domains; i++) {
		genpd = data->domains[i];

		if (!genpd)
			continue;
		if (!genpd_present(genpd))
			goto error;

		genpd->dev.of_node = np;

		 
		if (genpd->set_performance_state) {
			ret = dev_pm_opp_of_add_table_indexed(&genpd->dev, i);
			if (ret) {
				dev_err_probe(&genpd->dev, ret,
					      "Failed to add OPP table for index %d\n", i);
				goto error;
			}

			 
			genpd->opp_table = dev_pm_opp_get_opp_table(&genpd->dev);
			WARN_ON(IS_ERR(genpd->opp_table));
		}

		genpd->provider = &np->fwnode;
		genpd->has_provider = true;
	}

	ret = genpd_add_provider(np, data->xlate, data);
	if (ret < 0)
		goto error;

	return 0;

error:
	while (i--) {
		genpd = data->domains[i];

		if (!genpd)
			continue;

		genpd->provider = NULL;
		genpd->has_provider = false;

		if (genpd->set_performance_state) {
			dev_pm_opp_put_opp_table(genpd->opp_table);
			dev_pm_opp_of_remove_table(&genpd->dev);
		}
	}

	return ret;
}
EXPORT_SYMBOL_GPL(of_genpd_add_provider_onecell);

 
void of_genpd_del_provider(struct device_node *np)
{
	struct of_genpd_provider *cp, *tmp;
	struct generic_pm_domain *gpd;

	mutex_lock(&gpd_list_lock);
	mutex_lock(&of_genpd_mutex);
	list_for_each_entry_safe(cp, tmp, &of_genpd_providers, link) {
		if (cp->node == np) {
			 
			list_for_each_entry(gpd, &gpd_list, gpd_list_node) {
				if (gpd->provider == &np->fwnode) {
					gpd->has_provider = false;

					if (!gpd->set_performance_state)
						continue;

					dev_pm_opp_put_opp_table(gpd->opp_table);
					dev_pm_opp_of_remove_table(&gpd->dev);
				}
			}

			fwnode_dev_initialized(&cp->node->fwnode, false);
			list_del(&cp->link);
			of_node_put(cp->node);
			kfree(cp);
			break;
		}
	}
	mutex_unlock(&of_genpd_mutex);
	mutex_unlock(&gpd_list_lock);
}
EXPORT_SYMBOL_GPL(of_genpd_del_provider);

 
static struct generic_pm_domain *genpd_get_from_provider(
					struct of_phandle_args *genpdspec)
{
	struct generic_pm_domain *genpd = ERR_PTR(-ENOENT);
	struct of_genpd_provider *provider;

	if (!genpdspec)
		return ERR_PTR(-EINVAL);

	mutex_lock(&of_genpd_mutex);

	 
	list_for_each_entry(provider, &of_genpd_providers, link) {
		if (provider->node == genpdspec->np)
			genpd = provider->xlate(genpdspec, provider->data);
		if (!IS_ERR(genpd))
			break;
	}

	mutex_unlock(&of_genpd_mutex);

	return genpd;
}

 
int of_genpd_add_device(struct of_phandle_args *genpdspec, struct device *dev)
{
	struct generic_pm_domain *genpd;
	int ret;

	if (!dev)
		return -EINVAL;

	mutex_lock(&gpd_list_lock);

	genpd = genpd_get_from_provider(genpdspec);
	if (IS_ERR(genpd)) {
		ret = PTR_ERR(genpd);
		goto out;
	}

	ret = genpd_add_device(genpd, dev, dev);

out:
	mutex_unlock(&gpd_list_lock);

	return ret;
}
EXPORT_SYMBOL_GPL(of_genpd_add_device);

 
int of_genpd_add_subdomain(struct of_phandle_args *parent_spec,
			   struct of_phandle_args *subdomain_spec)
{
	struct generic_pm_domain *parent, *subdomain;
	int ret;

	mutex_lock(&gpd_list_lock);

	parent = genpd_get_from_provider(parent_spec);
	if (IS_ERR(parent)) {
		ret = PTR_ERR(parent);
		goto out;
	}

	subdomain = genpd_get_from_provider(subdomain_spec);
	if (IS_ERR(subdomain)) {
		ret = PTR_ERR(subdomain);
		goto out;
	}

	ret = genpd_add_subdomain(parent, subdomain);

out:
	mutex_unlock(&gpd_list_lock);

	return ret == -ENOENT ? -EPROBE_DEFER : ret;
}
EXPORT_SYMBOL_GPL(of_genpd_add_subdomain);

 
int of_genpd_remove_subdomain(struct of_phandle_args *parent_spec,
			      struct of_phandle_args *subdomain_spec)
{
	struct generic_pm_domain *parent, *subdomain;
	int ret;

	mutex_lock(&gpd_list_lock);

	parent = genpd_get_from_provider(parent_spec);
	if (IS_ERR(parent)) {
		ret = PTR_ERR(parent);
		goto out;
	}

	subdomain = genpd_get_from_provider(subdomain_spec);
	if (IS_ERR(subdomain)) {
		ret = PTR_ERR(subdomain);
		goto out;
	}

	ret = pm_genpd_remove_subdomain(parent, subdomain);

out:
	mutex_unlock(&gpd_list_lock);

	return ret;
}
EXPORT_SYMBOL_GPL(of_genpd_remove_subdomain);

 
struct generic_pm_domain *of_genpd_remove_last(struct device_node *np)
{
	struct generic_pm_domain *gpd, *tmp, *genpd = ERR_PTR(-ENOENT);
	int ret;

	if (IS_ERR_OR_NULL(np))
		return ERR_PTR(-EINVAL);

	mutex_lock(&gpd_list_lock);
	list_for_each_entry_safe(gpd, tmp, &gpd_list, gpd_list_node) {
		if (gpd->provider == &np->fwnode) {
			ret = genpd_remove(gpd);
			genpd = ret ? ERR_PTR(ret) : gpd;
			break;
		}
	}
	mutex_unlock(&gpd_list_lock);

	return genpd;
}
EXPORT_SYMBOL_GPL(of_genpd_remove_last);

static void genpd_release_dev(struct device *dev)
{
	of_node_put(dev->of_node);
	kfree(dev);
}

static struct bus_type genpd_bus_type = {
	.name		= "genpd",
};

 
static void genpd_dev_pm_detach(struct device *dev, bool power_off)
{
	struct generic_pm_domain *pd;
	unsigned int i;
	int ret = 0;

	pd = dev_to_genpd(dev);
	if (IS_ERR(pd))
		return;

	dev_dbg(dev, "removing from PM domain %s\n", pd->name);

	 
	if (dev_gpd_data(dev)->default_pstate) {
		dev_pm_genpd_set_performance_state(dev, 0);
		dev_gpd_data(dev)->default_pstate = 0;
	}

	for (i = 1; i < GENPD_RETRY_MAX_MS; i <<= 1) {
		ret = genpd_remove_device(pd, dev);
		if (ret != -EAGAIN)
			break;

		mdelay(i);
		cond_resched();
	}

	if (ret < 0) {
		dev_err(dev, "failed to remove from PM domain %s: %d",
			pd->name, ret);
		return;
	}

	 
	genpd_queue_power_off_work(pd);

	 
	if (dev->bus == &genpd_bus_type)
		device_unregister(dev);
}

static void genpd_dev_pm_sync(struct device *dev)
{
	struct generic_pm_domain *pd;

	pd = dev_to_genpd(dev);
	if (IS_ERR(pd))
		return;

	genpd_queue_power_off_work(pd);
}

static int __genpd_dev_pm_attach(struct device *dev, struct device *base_dev,
				 unsigned int index, bool power_on)
{
	struct of_phandle_args pd_args;
	struct generic_pm_domain *pd;
	int pstate;
	int ret;

	ret = of_parse_phandle_with_args(dev->of_node, "power-domains",
				"#power-domain-cells", index, &pd_args);
	if (ret < 0)
		return ret;

	mutex_lock(&gpd_list_lock);
	pd = genpd_get_from_provider(&pd_args);
	of_node_put(pd_args.np);
	if (IS_ERR(pd)) {
		mutex_unlock(&gpd_list_lock);
		dev_dbg(dev, "%s() failed to find PM domain: %ld\n",
			__func__, PTR_ERR(pd));
		return driver_deferred_probe_check_state(base_dev);
	}

	dev_dbg(dev, "adding to PM domain %s\n", pd->name);

	ret = genpd_add_device(pd, dev, base_dev);
	mutex_unlock(&gpd_list_lock);

	if (ret < 0)
		return dev_err_probe(dev, ret, "failed to add to PM domain %s\n", pd->name);

	dev->pm_domain->detach = genpd_dev_pm_detach;
	dev->pm_domain->sync = genpd_dev_pm_sync;

	 
	pstate = of_get_required_opp_performance_state(dev->of_node, index);
	if (pstate < 0 && pstate != -ENODEV && pstate != -EOPNOTSUPP) {
		ret = pstate;
		goto err;
	} else if (pstate > 0) {
		ret = dev_pm_genpd_set_performance_state(dev, pstate);
		if (ret)
			goto err;
		dev_gpd_data(dev)->default_pstate = pstate;
	}

	if (power_on) {
		genpd_lock(pd);
		ret = genpd_power_on(pd, 0);
		genpd_unlock(pd);
	}

	if (ret) {
		 
		if (dev_gpd_data(dev)->default_pstate) {
			dev_pm_genpd_set_performance_state(dev, 0);
			dev_gpd_data(dev)->default_pstate = 0;
		}

		genpd_remove_device(pd, dev);
		return -EPROBE_DEFER;
	}

	return 1;

err:
	dev_err(dev, "failed to set required performance state for power-domain %s: %d\n",
		pd->name, ret);
	genpd_remove_device(pd, dev);
	return ret;
}

 
int genpd_dev_pm_attach(struct device *dev)
{
	if (!dev->of_node)
		return 0;

	 
	if (of_count_phandle_with_args(dev->of_node, "power-domains",
				       "#power-domain-cells") != 1)
		return 0;

	return __genpd_dev_pm_attach(dev, dev, 0, true);
}
EXPORT_SYMBOL_GPL(genpd_dev_pm_attach);

 
struct device *genpd_dev_pm_attach_by_id(struct device *dev,
					 unsigned int index)
{
	struct device *virt_dev;
	int num_domains;
	int ret;

	if (!dev->of_node)
		return NULL;

	 
	num_domains = of_count_phandle_with_args(dev->of_node, "power-domains",
						 "#power-domain-cells");
	if (index >= num_domains)
		return NULL;

	 
	virt_dev = kzalloc(sizeof(*virt_dev), GFP_KERNEL);
	if (!virt_dev)
		return ERR_PTR(-ENOMEM);

	dev_set_name(virt_dev, "genpd:%u:%s", index, dev_name(dev));
	virt_dev->bus = &genpd_bus_type;
	virt_dev->release = genpd_release_dev;
	virt_dev->of_node = of_node_get(dev->of_node);

	ret = device_register(virt_dev);
	if (ret) {
		put_device(virt_dev);
		return ERR_PTR(ret);
	}

	 
	ret = __genpd_dev_pm_attach(virt_dev, dev, index, false);
	if (ret < 1) {
		device_unregister(virt_dev);
		return ret ? ERR_PTR(ret) : NULL;
	}

	pm_runtime_enable(virt_dev);
	genpd_queue_power_off_work(dev_to_genpd(virt_dev));

	return virt_dev;
}
EXPORT_SYMBOL_GPL(genpd_dev_pm_attach_by_id);

 
struct device *genpd_dev_pm_attach_by_name(struct device *dev, const char *name)
{
	int index;

	if (!dev->of_node)
		return NULL;

	index = of_property_match_string(dev->of_node, "power-domain-names",
					 name);
	if (index < 0)
		return NULL;

	return genpd_dev_pm_attach_by_id(dev, index);
}

static const struct of_device_id idle_state_match[] = {
	{ .compatible = "domain-idle-state", },
	{ }
};

static int genpd_parse_state(struct genpd_power_state *genpd_state,
				    struct device_node *state_node)
{
	int err;
	u32 residency;
	u32 entry_latency, exit_latency;

	err = of_property_read_u32(state_node, "entry-latency-us",
						&entry_latency);
	if (err) {
		pr_debug(" * %pOF missing entry-latency-us property\n",
			 state_node);
		return -EINVAL;
	}

	err = of_property_read_u32(state_node, "exit-latency-us",
						&exit_latency);
	if (err) {
		pr_debug(" * %pOF missing exit-latency-us property\n",
			 state_node);
		return -EINVAL;
	}

	err = of_property_read_u32(state_node, "min-residency-us", &residency);
	if (!err)
		genpd_state->residency_ns = 1000LL * residency;

	genpd_state->power_on_latency_ns = 1000LL * exit_latency;
	genpd_state->power_off_latency_ns = 1000LL * entry_latency;
	genpd_state->fwnode = &state_node->fwnode;

	return 0;
}

static int genpd_iterate_idle_states(struct device_node *dn,
				     struct genpd_power_state *states)
{
	int ret;
	struct of_phandle_iterator it;
	struct device_node *np;
	int i = 0;

	ret = of_count_phandle_with_args(dn, "domain-idle-states", NULL);
	if (ret <= 0)
		return ret == -ENOENT ? 0 : ret;

	 
	of_for_each_phandle(&it, ret, dn, "domain-idle-states", NULL, 0) {
		np = it.node;
		if (!of_match_node(idle_state_match, np))
			continue;

		if (!of_device_is_available(np))
			continue;

		if (states) {
			ret = genpd_parse_state(&states[i], np);
			if (ret) {
				pr_err("Parsing idle state node %pOF failed with err %d\n",
				       np, ret);
				of_node_put(np);
				return ret;
			}
		}
		i++;
	}

	return i;
}

 
int of_genpd_parse_idle_states(struct device_node *dn,
			struct genpd_power_state **states, int *n)
{
	struct genpd_power_state *st;
	int ret;

	ret = genpd_iterate_idle_states(dn, NULL);
	if (ret < 0)
		return ret;

	if (!ret) {
		*states = NULL;
		*n = 0;
		return 0;
	}

	st = kcalloc(ret, sizeof(*st), GFP_KERNEL);
	if (!st)
		return -ENOMEM;

	ret = genpd_iterate_idle_states(dn, st);
	if (ret <= 0) {
		kfree(st);
		return ret < 0 ? ret : -EINVAL;
	}

	*states = st;
	*n = ret;

	return 0;
}
EXPORT_SYMBOL_GPL(of_genpd_parse_idle_states);

 
unsigned int pm_genpd_opp_to_performance_state(struct device *genpd_dev,
					       struct dev_pm_opp *opp)
{
	struct generic_pm_domain *genpd = NULL;
	int state;

	genpd = container_of(genpd_dev, struct generic_pm_domain, dev);

	if (unlikely(!genpd->opp_to_performance_state))
		return 0;

	genpd_lock(genpd);
	state = genpd->opp_to_performance_state(genpd, opp);
	genpd_unlock(genpd);

	return state;
}
EXPORT_SYMBOL_GPL(pm_genpd_opp_to_performance_state);

static int __init genpd_bus_init(void)
{
	return bus_register(&genpd_bus_type);
}
core_initcall(genpd_bus_init);

#endif  


 

#ifdef CONFIG_DEBUG_FS
 
static void rtpm_status_str(struct seq_file *s, struct device *dev)
{
	static const char * const status_lookup[] = {
		[RPM_ACTIVE] = "active",
		[RPM_RESUMING] = "resuming",
		[RPM_SUSPENDED] = "suspended",
		[RPM_SUSPENDING] = "suspending"
	};
	const char *p = "";

	if (dev->power.runtime_error)
		p = "error";
	else if (dev->power.disable_depth)
		p = "unsupported";
	else if (dev->power.runtime_status < ARRAY_SIZE(status_lookup))
		p = status_lookup[dev->power.runtime_status];
	else
		WARN_ON(1);

	seq_printf(s, "%-25s  ", p);
}

static void perf_status_str(struct seq_file *s, struct device *dev)
{
	struct generic_pm_domain_data *gpd_data;

	gpd_data = to_gpd_data(dev->power.subsys_data->domain_data);
	seq_put_decimal_ull(s, "", gpd_data->performance_state);
}

static int genpd_summary_one(struct seq_file *s,
			struct generic_pm_domain *genpd)
{
	static const char * const status_lookup[] = {
		[GENPD_STATE_ON] = "on",
		[GENPD_STATE_OFF] = "off"
	};
	struct pm_domain_data *pm_data;
	const char *kobj_path;
	struct gpd_link *link;
	char state[16];
	int ret;

	ret = genpd_lock_interruptible(genpd);
	if (ret)
		return -ERESTARTSYS;

	if (WARN_ON(genpd->status >= ARRAY_SIZE(status_lookup)))
		goto exit;
	if (!genpd_status_on(genpd))
		snprintf(state, sizeof(state), "%s-%u",
			 status_lookup[genpd->status], genpd->state_idx);
	else
		snprintf(state, sizeof(state), "%s",
			 status_lookup[genpd->status]);
	seq_printf(s, "%-30s  %-50s %u", genpd->name, state, genpd->performance_state);

	 
	list_for_each_entry(link, &genpd->parent_links, parent_node) {
		if (list_is_first(&link->parent_node, &genpd->parent_links))
			seq_printf(s, "\n%48s", " ");
		seq_printf(s, "%s", link->child->name);
		if (!list_is_last(&link->parent_node, &genpd->parent_links))
			seq_puts(s, ", ");
	}

	list_for_each_entry(pm_data, &genpd->dev_list, list_node) {
		kobj_path = kobject_get_path(&pm_data->dev->kobj,
				genpd_is_irq_safe(genpd) ?
				GFP_ATOMIC : GFP_KERNEL);
		if (kobj_path == NULL)
			continue;

		seq_printf(s, "\n    %-50s  ", kobj_path);
		rtpm_status_str(s, pm_data->dev);
		perf_status_str(s, pm_data->dev);
		kfree(kobj_path);
	}

	seq_puts(s, "\n");
exit:
	genpd_unlock(genpd);

	return 0;
}

static int summary_show(struct seq_file *s, void *data)
{
	struct generic_pm_domain *genpd;
	int ret = 0;

	seq_puts(s, "domain                          status          children                           performance\n");
	seq_puts(s, "    /device                                             runtime status\n");
	seq_puts(s, "----------------------------------------------------------------------------------------------\n");

	ret = mutex_lock_interruptible(&gpd_list_lock);
	if (ret)
		return -ERESTARTSYS;

	list_for_each_entry(genpd, &gpd_list, gpd_list_node) {
		ret = genpd_summary_one(s, genpd);
		if (ret)
			break;
	}
	mutex_unlock(&gpd_list_lock);

	return ret;
}

static int status_show(struct seq_file *s, void *data)
{
	static const char * const status_lookup[] = {
		[GENPD_STATE_ON] = "on",
		[GENPD_STATE_OFF] = "off"
	};

	struct generic_pm_domain *genpd = s->private;
	int ret = 0;

	ret = genpd_lock_interruptible(genpd);
	if (ret)
		return -ERESTARTSYS;

	if (WARN_ON_ONCE(genpd->status >= ARRAY_SIZE(status_lookup)))
		goto exit;

	if (genpd->status == GENPD_STATE_OFF)
		seq_printf(s, "%s-%u\n", status_lookup[genpd->status],
			genpd->state_idx);
	else
		seq_printf(s, "%s\n", status_lookup[genpd->status]);
exit:
	genpd_unlock(genpd);
	return ret;
}

static int sub_domains_show(struct seq_file *s, void *data)
{
	struct generic_pm_domain *genpd = s->private;
	struct gpd_link *link;
	int ret = 0;

	ret = genpd_lock_interruptible(genpd);
	if (ret)
		return -ERESTARTSYS;

	list_for_each_entry(link, &genpd->parent_links, parent_node)
		seq_printf(s, "%s\n", link->child->name);

	genpd_unlock(genpd);
	return ret;
}

static int idle_states_show(struct seq_file *s, void *data)
{
	struct generic_pm_domain *genpd = s->private;
	u64 now, delta, idle_time = 0;
	unsigned int i;
	int ret = 0;

	ret = genpd_lock_interruptible(genpd);
	if (ret)
		return -ERESTARTSYS;

	seq_puts(s, "State          Time Spent(ms) Usage          Rejected\n");

	for (i = 0; i < genpd->state_count; i++) {
		idle_time += genpd->states[i].idle_time;

		if (genpd->status == GENPD_STATE_OFF && genpd->state_idx == i) {
			now = ktime_get_mono_fast_ns();
			if (now > genpd->accounting_time) {
				delta = now - genpd->accounting_time;
				idle_time += delta;
			}
		}

		do_div(idle_time, NSEC_PER_MSEC);
		seq_printf(s, "S%-13i %-14llu %-14llu %llu\n", i, idle_time,
			   genpd->states[i].usage, genpd->states[i].rejected);
	}

	genpd_unlock(genpd);
	return ret;
}

static int active_time_show(struct seq_file *s, void *data)
{
	struct generic_pm_domain *genpd = s->private;
	u64 now, on_time, delta = 0;
	int ret = 0;

	ret = genpd_lock_interruptible(genpd);
	if (ret)
		return -ERESTARTSYS;

	if (genpd->status == GENPD_STATE_ON) {
		now = ktime_get_mono_fast_ns();
		if (now > genpd->accounting_time)
			delta = now - genpd->accounting_time;
	}

	on_time = genpd->on_time + delta;
	do_div(on_time, NSEC_PER_MSEC);
	seq_printf(s, "%llu ms\n", on_time);

	genpd_unlock(genpd);
	return ret;
}

static int total_idle_time_show(struct seq_file *s, void *data)
{
	struct generic_pm_domain *genpd = s->private;
	u64 now, delta, total = 0;
	unsigned int i;
	int ret = 0;

	ret = genpd_lock_interruptible(genpd);
	if (ret)
		return -ERESTARTSYS;

	for (i = 0; i < genpd->state_count; i++) {
		total += genpd->states[i].idle_time;

		if (genpd->status == GENPD_STATE_OFF && genpd->state_idx == i) {
			now = ktime_get_mono_fast_ns();
			if (now > genpd->accounting_time) {
				delta = now - genpd->accounting_time;
				total += delta;
			}
		}
	}

	do_div(total, NSEC_PER_MSEC);
	seq_printf(s, "%llu ms\n", total);

	genpd_unlock(genpd);
	return ret;
}


static int devices_show(struct seq_file *s, void *data)
{
	struct generic_pm_domain *genpd = s->private;
	struct pm_domain_data *pm_data;
	const char *kobj_path;
	int ret = 0;

	ret = genpd_lock_interruptible(genpd);
	if (ret)
		return -ERESTARTSYS;

	list_for_each_entry(pm_data, &genpd->dev_list, list_node) {
		kobj_path = kobject_get_path(&pm_data->dev->kobj,
				genpd_is_irq_safe(genpd) ?
				GFP_ATOMIC : GFP_KERNEL);
		if (kobj_path == NULL)
			continue;

		seq_printf(s, "%s\n", kobj_path);
		kfree(kobj_path);
	}

	genpd_unlock(genpd);
	return ret;
}

static int perf_state_show(struct seq_file *s, void *data)
{
	struct generic_pm_domain *genpd = s->private;

	if (genpd_lock_interruptible(genpd))
		return -ERESTARTSYS;

	seq_printf(s, "%u\n", genpd->performance_state);

	genpd_unlock(genpd);
	return 0;
}

DEFINE_SHOW_ATTRIBUTE(summary);
DEFINE_SHOW_ATTRIBUTE(status);
DEFINE_SHOW_ATTRIBUTE(sub_domains);
DEFINE_SHOW_ATTRIBUTE(idle_states);
DEFINE_SHOW_ATTRIBUTE(active_time);
DEFINE_SHOW_ATTRIBUTE(total_idle_time);
DEFINE_SHOW_ATTRIBUTE(devices);
DEFINE_SHOW_ATTRIBUTE(perf_state);

static void genpd_debug_add(struct generic_pm_domain *genpd)
{
	struct dentry *d;

	if (!genpd_debugfs_dir)
		return;

	d = debugfs_create_dir(genpd->name, genpd_debugfs_dir);

	debugfs_create_file("current_state", 0444,
			    d, genpd, &status_fops);
	debugfs_create_file("sub_domains", 0444,
			    d, genpd, &sub_domains_fops);
	debugfs_create_file("idle_states", 0444,
			    d, genpd, &idle_states_fops);
	debugfs_create_file("active_time", 0444,
			    d, genpd, &active_time_fops);
	debugfs_create_file("total_idle_time", 0444,
			    d, genpd, &total_idle_time_fops);
	debugfs_create_file("devices", 0444,
			    d, genpd, &devices_fops);
	if (genpd->set_performance_state)
		debugfs_create_file("perf_state", 0444,
				    d, genpd, &perf_state_fops);
}

static int __init genpd_debug_init(void)
{
	struct generic_pm_domain *genpd;

	genpd_debugfs_dir = debugfs_create_dir("pm_genpd", NULL);

	debugfs_create_file("pm_genpd_summary", S_IRUGO, genpd_debugfs_dir,
			    NULL, &summary_fops);

	list_for_each_entry(genpd, &gpd_list, gpd_list_node)
		genpd_debug_add(genpd);

	return 0;
}
late_initcall(genpd_debug_init);

static void __exit genpd_debug_exit(void)
{
	debugfs_remove_recursive(genpd_debugfs_dir);
}
__exitcall(genpd_debug_exit);
#endif  
