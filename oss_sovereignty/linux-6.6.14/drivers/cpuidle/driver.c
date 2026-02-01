 

#include <linux/mutex.h>
#include <linux/module.h>
#include <linux/sched.h>
#include <linux/sched/idle.h>
#include <linux/cpuidle.h>
#include <linux/cpumask.h>
#include <linux/tick.h>
#include <linux/cpu.h>

#include "cpuidle.h"

DEFINE_SPINLOCK(cpuidle_driver_lock);

#ifdef CONFIG_CPU_IDLE_MULTIPLE_DRIVERS

static DEFINE_PER_CPU(struct cpuidle_driver *, cpuidle_drivers);

 
static struct cpuidle_driver *__cpuidle_get_cpu_driver(int cpu)
{
	return per_cpu(cpuidle_drivers, cpu);
}

 
static inline void __cpuidle_unset_driver(struct cpuidle_driver *drv)
{
	int cpu;

	for_each_cpu(cpu, drv->cpumask) {

		if (drv != __cpuidle_get_cpu_driver(cpu))
			continue;

		per_cpu(cpuidle_drivers, cpu) = NULL;
	}
}

 
static inline int __cpuidle_set_driver(struct cpuidle_driver *drv)
{
	int cpu;

	for_each_cpu(cpu, drv->cpumask) {
		struct cpuidle_driver *old_drv;

		old_drv = __cpuidle_get_cpu_driver(cpu);
		if (old_drv && old_drv != drv)
			return -EBUSY;
	}

	for_each_cpu(cpu, drv->cpumask)
		per_cpu(cpuidle_drivers, cpu) = drv;

	return 0;
}

#else

static struct cpuidle_driver *cpuidle_curr_driver;

 
static inline struct cpuidle_driver *__cpuidle_get_cpu_driver(int cpu)
{
	return cpuidle_curr_driver;
}

 
static inline int __cpuidle_set_driver(struct cpuidle_driver *drv)
{
	if (cpuidle_curr_driver)
		return -EBUSY;

	cpuidle_curr_driver = drv;

	return 0;
}

 
static inline void __cpuidle_unset_driver(struct cpuidle_driver *drv)
{
	if (drv == cpuidle_curr_driver)
		cpuidle_curr_driver = NULL;
}

#endif

 
static void cpuidle_setup_broadcast_timer(void *arg)
{
	if (arg)
		tick_broadcast_enable();
	else
		tick_broadcast_disable();
}

 
static void __cpuidle_driver_init(struct cpuidle_driver *drv)
{
	int i;

	 
	if (!drv->cpumask)
		drv->cpumask = (struct cpumask *)cpu_possible_mask;

	for (i = 0; i < drv->state_count; i++) {
		struct cpuidle_state *s = &drv->states[i];

		 
		if (s->flags & CPUIDLE_FLAG_TIMER_STOP)
			drv->bctimer = 1;

		 
		if (s->target_residency > 0)
			s->target_residency_ns = s->target_residency * NSEC_PER_USEC;
		else if (s->target_residency_ns < 0)
			s->target_residency_ns = 0;
		else
			s->target_residency = div_u64(s->target_residency_ns, NSEC_PER_USEC);

		if (s->exit_latency > 0)
			s->exit_latency_ns = s->exit_latency * NSEC_PER_USEC;
		else if (s->exit_latency_ns < 0)
			s->exit_latency_ns =  0;
		else
			s->exit_latency = div_u64(s->exit_latency_ns, NSEC_PER_USEC);
	}
}

 
static int __cpuidle_register_driver(struct cpuidle_driver *drv)
{
	int ret;

	if (!drv || !drv->state_count)
		return -EINVAL;

	ret = cpuidle_coupled_state_verify(drv);
	if (ret)
		return ret;

	if (cpuidle_disabled())
		return -ENODEV;

	__cpuidle_driver_init(drv);

	ret = __cpuidle_set_driver(drv);
	if (ret)
		return ret;

	if (drv->bctimer)
		on_each_cpu_mask(drv->cpumask, cpuidle_setup_broadcast_timer,
				 (void *)1, 1);

	return 0;
}

 
static void __cpuidle_unregister_driver(struct cpuidle_driver *drv)
{
	if (drv->bctimer) {
		drv->bctimer = 0;
		on_each_cpu_mask(drv->cpumask, cpuidle_setup_broadcast_timer,
				 NULL, 1);
	}

	__cpuidle_unset_driver(drv);
}

 
int cpuidle_register_driver(struct cpuidle_driver *drv)
{
	struct cpuidle_governor *gov;
	int ret;

	spin_lock(&cpuidle_driver_lock);
	ret = __cpuidle_register_driver(drv);
	spin_unlock(&cpuidle_driver_lock);

	if (!ret && !strlen(param_governor) && drv->governor &&
	    (cpuidle_get_driver() == drv)) {
		mutex_lock(&cpuidle_lock);
		gov = cpuidle_find_governor(drv->governor);
		if (gov) {
			cpuidle_prev_governor = cpuidle_curr_governor;
			if (cpuidle_switch_governor(gov) < 0)
				cpuidle_prev_governor = NULL;
		}
		mutex_unlock(&cpuidle_lock);
	}

	return ret;
}
EXPORT_SYMBOL_GPL(cpuidle_register_driver);

 
void cpuidle_unregister_driver(struct cpuidle_driver *drv)
{
	bool enabled = (cpuidle_get_driver() == drv);

	spin_lock(&cpuidle_driver_lock);
	__cpuidle_unregister_driver(drv);
	spin_unlock(&cpuidle_driver_lock);

	if (!enabled)
		return;

	mutex_lock(&cpuidle_lock);
	if (cpuidle_prev_governor) {
		if (!cpuidle_switch_governor(cpuidle_prev_governor))
			cpuidle_prev_governor = NULL;
	}
	mutex_unlock(&cpuidle_lock);
}
EXPORT_SYMBOL_GPL(cpuidle_unregister_driver);

 
struct cpuidle_driver *cpuidle_get_driver(void)
{
	struct cpuidle_driver *drv;
	int cpu;

	cpu = get_cpu();
	drv = __cpuidle_get_cpu_driver(cpu);
	put_cpu();

	return drv;
}
EXPORT_SYMBOL_GPL(cpuidle_get_driver);

 
struct cpuidle_driver *cpuidle_get_cpu_driver(struct cpuidle_device *dev)
{
	if (!dev)
		return NULL;

	return __cpuidle_get_cpu_driver(dev->cpu);
}
EXPORT_SYMBOL_GPL(cpuidle_get_cpu_driver);

 
void cpuidle_driver_state_disabled(struct cpuidle_driver *drv, int idx,
				 bool disable)
{
	unsigned int cpu;

	mutex_lock(&cpuidle_lock);

	spin_lock(&cpuidle_driver_lock);

	if (!drv->cpumask) {
		drv->states[idx].flags |= CPUIDLE_FLAG_UNUSABLE;
		goto unlock;
	}

	for_each_cpu(cpu, drv->cpumask) {
		struct cpuidle_device *dev = per_cpu(cpuidle_devices, cpu);

		if (!dev)
			continue;

		if (disable)
			dev->states_usage[idx].disable |= CPUIDLE_STATE_DISABLED_BY_DRIVER;
		else
			dev->states_usage[idx].disable &= ~CPUIDLE_STATE_DISABLED_BY_DRIVER;
	}

unlock:
	spin_unlock(&cpuidle_driver_lock);

	mutex_unlock(&cpuidle_lock);
}
