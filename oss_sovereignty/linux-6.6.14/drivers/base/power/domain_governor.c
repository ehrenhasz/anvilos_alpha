
 
#include <linux/kernel.h>
#include <linux/pm_domain.h>
#include <linux/pm_qos.h>
#include <linux/hrtimer.h>
#include <linux/cpuidle.h>
#include <linux/cpumask.h>
#include <linux/ktime.h>

static int dev_update_qos_constraint(struct device *dev, void *data)
{
	s64 *constraint_ns_p = data;
	s64 constraint_ns;

	if (dev->power.subsys_data && dev->power.subsys_data->domain_data) {
		struct gpd_timing_data *td = dev_gpd_data(dev)->td;

		 
		constraint_ns = td ? td->effective_constraint_ns :
				PM_QOS_RESUME_LATENCY_NO_CONSTRAINT_NS;
	} else {
		 
		constraint_ns = dev_pm_qos_read_value(dev, DEV_PM_QOS_RESUME_LATENCY);
		constraint_ns *= NSEC_PER_USEC;
	}

	if (constraint_ns < *constraint_ns_p)
		*constraint_ns_p = constraint_ns;

	return 0;
}

 
static bool default_suspend_ok(struct device *dev)
{
	struct gpd_timing_data *td = dev_gpd_data(dev)->td;
	unsigned long flags;
	s64 constraint_ns;

	dev_dbg(dev, "%s()\n", __func__);

	spin_lock_irqsave(&dev->power.lock, flags);

	if (!td->constraint_changed) {
		bool ret = td->cached_suspend_ok;

		spin_unlock_irqrestore(&dev->power.lock, flags);
		return ret;
	}
	td->constraint_changed = false;
	td->cached_suspend_ok = false;
	td->effective_constraint_ns = 0;
	constraint_ns = __dev_pm_qos_resume_latency(dev);

	spin_unlock_irqrestore(&dev->power.lock, flags);

	if (constraint_ns == 0)
		return false;

	constraint_ns *= NSEC_PER_USEC;
	 
	if (!dev->power.ignore_children)
		device_for_each_child(dev, &constraint_ns,
				      dev_update_qos_constraint);

	if (constraint_ns == PM_QOS_RESUME_LATENCY_NO_CONSTRAINT_NS) {
		 
		td->effective_constraint_ns = PM_QOS_RESUME_LATENCY_NO_CONSTRAINT_NS;
		td->cached_suspend_ok = true;
	} else if (constraint_ns == 0) {
		 
		return false;
	} else {
		constraint_ns -= td->suspend_latency_ns +
				td->resume_latency_ns;
		 
		if (constraint_ns <= 0)
			return false;

		td->effective_constraint_ns = constraint_ns;
		td->cached_suspend_ok = true;
	}

	 
	return td->cached_suspend_ok;
}

static void update_domain_next_wakeup(struct generic_pm_domain *genpd, ktime_t now)
{
	ktime_t domain_wakeup = KTIME_MAX;
	ktime_t next_wakeup;
	struct pm_domain_data *pdd;
	struct gpd_link *link;

	if (!(genpd->flags & GENPD_FLAG_MIN_RESIDENCY))
		return;

	 
	list_for_each_entry(pdd, &genpd->dev_list, list_node) {
		next_wakeup = to_gpd_data(pdd)->td->next_wakeup;
		if (next_wakeup != KTIME_MAX && !ktime_before(next_wakeup, now))
			if (ktime_before(next_wakeup, domain_wakeup))
				domain_wakeup = next_wakeup;
	}

	list_for_each_entry(link, &genpd->parent_links, parent_node) {
		struct genpd_governor_data *cgd = link->child->gd;

		next_wakeup = cgd ? cgd->next_wakeup : KTIME_MAX;
		if (next_wakeup != KTIME_MAX && !ktime_before(next_wakeup, now))
			if (ktime_before(next_wakeup, domain_wakeup))
				domain_wakeup = next_wakeup;
	}

	genpd->gd->next_wakeup = domain_wakeup;
}

static bool next_wakeup_allows_state(struct generic_pm_domain *genpd,
				     unsigned int state, ktime_t now)
{
	ktime_t domain_wakeup = genpd->gd->next_wakeup;
	s64 idle_time_ns, min_sleep_ns;

	min_sleep_ns = genpd->states[state].power_off_latency_ns +
		       genpd->states[state].residency_ns;

	idle_time_ns = ktime_to_ns(ktime_sub(domain_wakeup, now));

	return idle_time_ns >= min_sleep_ns;
}

static bool __default_power_down_ok(struct dev_pm_domain *pd,
				     unsigned int state)
{
	struct generic_pm_domain *genpd = pd_to_genpd(pd);
	struct gpd_link *link;
	struct pm_domain_data *pdd;
	s64 min_off_time_ns;
	s64 off_on_time_ns;

	off_on_time_ns = genpd->states[state].power_off_latency_ns +
		genpd->states[state].power_on_latency_ns;

	min_off_time_ns = -1;
	 
	list_for_each_entry(link, &genpd->parent_links, parent_node) {
		struct genpd_governor_data *cgd = link->child->gd;

		s64 sd_max_off_ns = cgd ? cgd->max_off_time_ns : -1;

		if (sd_max_off_ns < 0)
			continue;

		 
		if (sd_max_off_ns <= off_on_time_ns)
			return false;

		if (min_off_time_ns > sd_max_off_ns || min_off_time_ns < 0)
			min_off_time_ns = sd_max_off_ns;
	}

	 
	list_for_each_entry(pdd, &genpd->dev_list, list_node) {
		struct gpd_timing_data *td;
		s64 constraint_ns;

		 
		td = to_gpd_data(pdd)->td;
		constraint_ns = td->effective_constraint_ns;
		 
		if (constraint_ns == PM_QOS_RESUME_LATENCY_NO_CONSTRAINT_NS)
			continue;

		if (constraint_ns <= off_on_time_ns)
			return false;

		if (min_off_time_ns > constraint_ns || min_off_time_ns < 0)
			min_off_time_ns = constraint_ns;
	}

	 
	if (min_off_time_ns < 0)
		return true;

	 
	genpd->gd->max_off_time_ns = min_off_time_ns -
		genpd->states[state].power_on_latency_ns;
	return true;
}

 
static bool _default_power_down_ok(struct dev_pm_domain *pd, ktime_t now)
{
	struct generic_pm_domain *genpd = pd_to_genpd(pd);
	struct genpd_governor_data *gd = genpd->gd;
	int state_idx = genpd->state_count - 1;
	struct gpd_link *link;

	 
	update_domain_next_wakeup(genpd, now);
	if ((genpd->flags & GENPD_FLAG_MIN_RESIDENCY) && (gd->next_wakeup != KTIME_MAX)) {
		 
		while (state_idx >= 0) {
			if (next_wakeup_allows_state(genpd, state_idx, now)) {
				gd->max_off_time_changed = true;
				break;
			}
			state_idx--;
		}

		if (state_idx < 0) {
			state_idx = 0;
			gd->cached_power_down_ok = false;
			goto done;
		}
	}

	if (!gd->max_off_time_changed) {
		genpd->state_idx = gd->cached_power_down_state_idx;
		return gd->cached_power_down_ok;
	}

	 
	list_for_each_entry(link, &genpd->child_links, child_node) {
		struct genpd_governor_data *pgd = link->parent->gd;

		if (pgd)
			pgd->max_off_time_changed = true;
	}

	gd->max_off_time_ns = -1;
	gd->max_off_time_changed = false;
	gd->cached_power_down_ok = true;

	 
	while (!__default_power_down_ok(pd, state_idx)) {
		if (state_idx == 0) {
			gd->cached_power_down_ok = false;
			break;
		}
		state_idx--;
	}

done:
	genpd->state_idx = state_idx;
	gd->cached_power_down_state_idx = genpd->state_idx;
	return gd->cached_power_down_ok;
}

static bool default_power_down_ok(struct dev_pm_domain *pd)
{
	return _default_power_down_ok(pd, ktime_get());
}

#ifdef CONFIG_CPU_IDLE
static bool cpu_power_down_ok(struct dev_pm_domain *pd)
{
	struct generic_pm_domain *genpd = pd_to_genpd(pd);
	struct cpuidle_device *dev;
	ktime_t domain_wakeup, next_hrtimer;
	ktime_t now = ktime_get();
	s64 idle_duration_ns;
	int cpu, i;

	 
	if (!_default_power_down_ok(pd, now))
		return false;

	if (!(genpd->flags & GENPD_FLAG_CPU_DOMAIN))
		return true;

	 
	domain_wakeup = ktime_set(KTIME_SEC_MAX, 0);
	for_each_cpu_and(cpu, genpd->cpus, cpu_online_mask) {
		dev = per_cpu(cpuidle_devices, cpu);
		if (dev) {
			next_hrtimer = READ_ONCE(dev->next_hrtimer);
			if (ktime_before(next_hrtimer, domain_wakeup))
				domain_wakeup = next_hrtimer;
		}
	}

	 
	idle_duration_ns = ktime_to_ns(ktime_sub(domain_wakeup, now));
	if (idle_duration_ns <= 0)
		return false;

	 
	genpd->gd->next_hrtimer = domain_wakeup;

	 
	i = genpd->state_idx;
	do {
		if (idle_duration_ns >= (genpd->states[i].residency_ns +
		    genpd->states[i].power_off_latency_ns)) {
			genpd->state_idx = i;
			return true;
		}
	} while (--i >= 0);

	return false;
}

struct dev_power_governor pm_domain_cpu_gov = {
	.suspend_ok = default_suspend_ok,
	.power_down_ok = cpu_power_down_ok,
};
#endif

struct dev_power_governor simple_qos_governor = {
	.suspend_ok = default_suspend_ok,
	.power_down_ok = default_power_down_ok,
};

 
struct dev_power_governor pm_domain_always_on_gov = {
	.suspend_ok = default_suspend_ok,
};
