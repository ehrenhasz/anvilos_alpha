 

#include "linux/percpu-defs.h"
#include <linux/clockchips.h>
#include <linux/kernel.h>
#include <linux/mutex.h>
#include <linux/sched.h>
#include <linux/sched/clock.h>
#include <linux/sched/idle.h>
#include <linux/notifier.h>
#include <linux/pm_qos.h>
#include <linux/cpu.h>
#include <linux/cpuidle.h>
#include <linux/ktime.h>
#include <linux/hrtimer.h>
#include <linux/module.h>
#include <linux/suspend.h>
#include <linux/tick.h>
#include <linux/mmu_context.h>
#include <linux/context_tracking.h>
#include <trace/events/power.h>

#include "cpuidle.h"

DEFINE_PER_CPU(struct cpuidle_device *, cpuidle_devices);
DEFINE_PER_CPU(struct cpuidle_device, cpuidle_dev);

DEFINE_MUTEX(cpuidle_lock);
LIST_HEAD(cpuidle_detected_devices);

static int enabled_devices;
static int off __read_mostly;
static int initialized __read_mostly;

int cpuidle_disabled(void)
{
	return off;
}
void disable_cpuidle(void)
{
	off = 1;
}

bool cpuidle_not_available(struct cpuidle_driver *drv,
			   struct cpuidle_device *dev)
{
	return off || !initialized || !drv || !dev || !dev->enabled;
}

 
int cpuidle_play_dead(void)
{
	struct cpuidle_device *dev = __this_cpu_read(cpuidle_devices);
	struct cpuidle_driver *drv = cpuidle_get_cpu_driver(dev);
	int i;

	if (!drv)
		return -ENODEV;

	 
	for (i = drv->state_count - 1; i >= 0; i--)
		if (drv->states[i].enter_dead)
			return drv->states[i].enter_dead(dev, i);

	return -ENODEV;
}

static int find_deepest_state(struct cpuidle_driver *drv,
			      struct cpuidle_device *dev,
			      u64 max_latency_ns,
			      unsigned int forbidden_flags,
			      bool s2idle)
{
	u64 latency_req = 0;
	int i, ret = 0;

	for (i = 1; i < drv->state_count; i++) {
		struct cpuidle_state *s = &drv->states[i];

		if (dev->states_usage[i].disable ||
		    s->exit_latency_ns <= latency_req ||
		    s->exit_latency_ns > max_latency_ns ||
		    (s->flags & forbidden_flags) ||
		    (s2idle && !s->enter_s2idle))
			continue;

		latency_req = s->exit_latency_ns;
		ret = i;
	}
	return ret;
}

 
void cpuidle_use_deepest_state(u64 latency_limit_ns)
{
	struct cpuidle_device *dev;

	preempt_disable();
	dev = cpuidle_get_device();
	if (dev)
		dev->forced_idle_latency_limit_ns = latency_limit_ns;
	preempt_enable();
}

 
int cpuidle_find_deepest_state(struct cpuidle_driver *drv,
			       struct cpuidle_device *dev,
			       u64 latency_limit_ns)
{
	return find_deepest_state(drv, dev, latency_limit_ns, 0, false);
}

#ifdef CONFIG_SUSPEND
static noinstr void enter_s2idle_proper(struct cpuidle_driver *drv,
					 struct cpuidle_device *dev, int index)
{
	struct cpuidle_state *target_state = &drv->states[index];
	ktime_t time_start, time_end;

	instrumentation_begin();

	time_start = ns_to_ktime(local_clock_noinstr());

	tick_freeze();
	 
	stop_critical_timings();
	if (!(target_state->flags & CPUIDLE_FLAG_RCU_IDLE)) {
		ct_cpuidle_enter();
		 
		instrumentation_begin();
	}
	target_state->enter_s2idle(dev, drv, index);
	if (WARN_ON_ONCE(!irqs_disabled()))
		raw_local_irq_disable();
	if (!(target_state->flags & CPUIDLE_FLAG_RCU_IDLE)) {
		instrumentation_end();
		ct_cpuidle_exit();
	}
	tick_unfreeze();
	start_critical_timings();

	time_end = ns_to_ktime(local_clock_noinstr());

	dev->states_usage[index].s2idle_time += ktime_us_delta(time_end, time_start);
	dev->states_usage[index].s2idle_usage++;
	instrumentation_end();
}

 
int cpuidle_enter_s2idle(struct cpuidle_driver *drv, struct cpuidle_device *dev)
{
	int index;

	 
	index = find_deepest_state(drv, dev, U64_MAX, 0, true);
	if (index > 0) {
		enter_s2idle_proper(drv, dev, index);
		local_irq_enable();
	}
	return index;
}
#endif  

 
noinstr int cpuidle_enter_state(struct cpuidle_device *dev,
				 struct cpuidle_driver *drv,
				 int index)
{
	int entered_state;

	struct cpuidle_state *target_state = &drv->states[index];
	bool broadcast = !!(target_state->flags & CPUIDLE_FLAG_TIMER_STOP);
	ktime_t time_start, time_end;

	instrumentation_begin();

	 
	if (broadcast && tick_broadcast_enter()) {
		index = find_deepest_state(drv, dev, target_state->exit_latency_ns,
					   CPUIDLE_FLAG_TIMER_STOP, false);
		if (index < 0) {
			default_idle_call();
			return -EBUSY;
		}
		target_state = &drv->states[index];
		broadcast = false;
	}

	if (target_state->flags & CPUIDLE_FLAG_TLB_FLUSHED)
		leave_mm(dev->cpu);

	 
	sched_idle_set_state(target_state);

	trace_cpu_idle(index, dev->cpu);
	time_start = ns_to_ktime(local_clock_noinstr());

	stop_critical_timings();
	if (!(target_state->flags & CPUIDLE_FLAG_RCU_IDLE)) {
		ct_cpuidle_enter();
		 
		instrumentation_begin();
	}

	 
	entered_state = target_state->enter(dev, drv, index);

	if (WARN_ONCE(!irqs_disabled(), "%ps leaked IRQ state", target_state->enter))
		raw_local_irq_disable();

	if (!(target_state->flags & CPUIDLE_FLAG_RCU_IDLE)) {
		instrumentation_end();
		ct_cpuidle_exit();
	}
	start_critical_timings();

	sched_clock_idle_wakeup_event();
	time_end = ns_to_ktime(local_clock_noinstr());
	trace_cpu_idle(PWR_EVENT_EXIT, dev->cpu);

	 
	sched_idle_set_state(NULL);

	if (broadcast)
		tick_broadcast_exit();

	if (!cpuidle_state_is_coupled(drv, index))
		local_irq_enable();

	if (entered_state >= 0) {
		s64 diff, delay = drv->states[entered_state].exit_latency_ns;
		int i;

		 
		diff = ktime_sub(time_end, time_start);

		dev->last_residency_ns = diff;
		dev->states_usage[entered_state].time_ns += diff;
		dev->states_usage[entered_state].usage++;

		if (diff < drv->states[entered_state].target_residency_ns) {
			for (i = entered_state - 1; i >= 0; i--) {
				if (dev->states_usage[i].disable)
					continue;

				 
				dev->states_usage[entered_state].above++;
				trace_cpu_idle_miss(dev->cpu, entered_state, false);
				break;
			}
		} else if (diff > delay) {
			for (i = entered_state + 1; i < drv->state_count; i++) {
				if (dev->states_usage[i].disable)
					continue;

				 
				if (diff - delay >= drv->states[i].target_residency_ns) {
					dev->states_usage[entered_state].below++;
					trace_cpu_idle_miss(dev->cpu, entered_state, true);
				}

				break;
			}
		}
	} else {
		dev->last_residency_ns = 0;
		dev->states_usage[index].rejected++;
	}

	instrumentation_end();

	return entered_state;
}

 
int cpuidle_select(struct cpuidle_driver *drv, struct cpuidle_device *dev,
		   bool *stop_tick)
{
	return cpuidle_curr_governor->select(drv, dev, stop_tick);
}

 
int cpuidle_enter(struct cpuidle_driver *drv, struct cpuidle_device *dev,
		  int index)
{
	int ret = 0;

	 
	WRITE_ONCE(dev->next_hrtimer, tick_nohz_get_next_hrtimer());

	if (cpuidle_state_is_coupled(drv, index))
		ret = cpuidle_enter_state_coupled(dev, drv, index);
	else
		ret = cpuidle_enter_state(dev, drv, index);

	WRITE_ONCE(dev->next_hrtimer, 0);
	return ret;
}

 
void cpuidle_reflect(struct cpuidle_device *dev, int index)
{
	if (cpuidle_curr_governor->reflect && index >= 0)
		cpuidle_curr_governor->reflect(dev, index);
}

 
#define CPUIDLE_POLL_MIN 10000
#define CPUIDLE_POLL_MAX (TICK_NSEC / 16)

 
__cpuidle u64 cpuidle_poll_time(struct cpuidle_driver *drv,
		      struct cpuidle_device *dev)
{
	int i;
	u64 limit_ns;

	BUILD_BUG_ON(CPUIDLE_POLL_MIN > CPUIDLE_POLL_MAX);

	if (dev->poll_limit_ns)
		return dev->poll_limit_ns;

	limit_ns = CPUIDLE_POLL_MAX;
	for (i = 1; i < drv->state_count; i++) {
		u64 state_limit;

		if (dev->states_usage[i].disable)
			continue;

		state_limit = drv->states[i].target_residency_ns;
		if (state_limit < CPUIDLE_POLL_MIN)
			continue;

		limit_ns = min_t(u64, state_limit, CPUIDLE_POLL_MAX);
		break;
	}

	dev->poll_limit_ns = limit_ns;

	return dev->poll_limit_ns;
}

 
void cpuidle_install_idle_handler(void)
{
	if (enabled_devices) {
		 
		smp_wmb();
		initialized = 1;
	}
}

 
void cpuidle_uninstall_idle_handler(void)
{
	if (enabled_devices) {
		initialized = 0;
		wake_up_all_idle_cpus();
	}

	 
	synchronize_rcu();
}

 
void cpuidle_pause_and_lock(void)
{
	mutex_lock(&cpuidle_lock);
	cpuidle_uninstall_idle_handler();
}

EXPORT_SYMBOL_GPL(cpuidle_pause_and_lock);

 
void cpuidle_resume_and_unlock(void)
{
	cpuidle_install_idle_handler();
	mutex_unlock(&cpuidle_lock);
}

EXPORT_SYMBOL_GPL(cpuidle_resume_and_unlock);

 
void cpuidle_pause(void)
{
	mutex_lock(&cpuidle_lock);
	cpuidle_uninstall_idle_handler();
	mutex_unlock(&cpuidle_lock);
}

 
void cpuidle_resume(void)
{
	mutex_lock(&cpuidle_lock);
	cpuidle_install_idle_handler();
	mutex_unlock(&cpuidle_lock);
}

 
int cpuidle_enable_device(struct cpuidle_device *dev)
{
	int ret;
	struct cpuidle_driver *drv;

	if (!dev)
		return -EINVAL;

	if (dev->enabled)
		return 0;

	if (!cpuidle_curr_governor)
		return -EIO;

	drv = cpuidle_get_cpu_driver(dev);

	if (!drv)
		return -EIO;

	if (!dev->registered)
		return -EINVAL;

	ret = cpuidle_add_device_sysfs(dev);
	if (ret)
		return ret;

	if (cpuidle_curr_governor->enable) {
		ret = cpuidle_curr_governor->enable(drv, dev);
		if (ret)
			goto fail_sysfs;
	}

	smp_wmb();

	dev->enabled = 1;

	enabled_devices++;
	return 0;

fail_sysfs:
	cpuidle_remove_device_sysfs(dev);

	return ret;
}

EXPORT_SYMBOL_GPL(cpuidle_enable_device);

 
void cpuidle_disable_device(struct cpuidle_device *dev)
{
	struct cpuidle_driver *drv = cpuidle_get_cpu_driver(dev);

	if (!dev || !dev->enabled)
		return;

	if (!drv || !cpuidle_curr_governor)
		return;

	dev->enabled = 0;

	if (cpuidle_curr_governor->disable)
		cpuidle_curr_governor->disable(drv, dev);

	cpuidle_remove_device_sysfs(dev);
	enabled_devices--;
}

EXPORT_SYMBOL_GPL(cpuidle_disable_device);

static void __cpuidle_unregister_device(struct cpuidle_device *dev)
{
	struct cpuidle_driver *drv = cpuidle_get_cpu_driver(dev);

	list_del(&dev->device_list);
	per_cpu(cpuidle_devices, dev->cpu) = NULL;
	module_put(drv->owner);

	dev->registered = 0;
}

static void __cpuidle_device_init(struct cpuidle_device *dev)
{
	memset(dev->states_usage, 0, sizeof(dev->states_usage));
	dev->last_residency_ns = 0;
	dev->next_hrtimer = 0;
}

 
static int __cpuidle_register_device(struct cpuidle_device *dev)
{
	struct cpuidle_driver *drv = cpuidle_get_cpu_driver(dev);
	int i, ret;

	if (!try_module_get(drv->owner))
		return -EINVAL;

	for (i = 0; i < drv->state_count; i++) {
		if (drv->states[i].flags & CPUIDLE_FLAG_UNUSABLE)
			dev->states_usage[i].disable |= CPUIDLE_STATE_DISABLED_BY_DRIVER;

		if (drv->states[i].flags & CPUIDLE_FLAG_OFF)
			dev->states_usage[i].disable |= CPUIDLE_STATE_DISABLED_BY_USER;
	}

	per_cpu(cpuidle_devices, dev->cpu) = dev;
	list_add(&dev->device_list, &cpuidle_detected_devices);

	ret = cpuidle_coupled_register_device(dev);
	if (ret)
		__cpuidle_unregister_device(dev);
	else
		dev->registered = 1;

	return ret;
}

 
int cpuidle_register_device(struct cpuidle_device *dev)
{
	int ret = -EBUSY;

	if (!dev)
		return -EINVAL;

	mutex_lock(&cpuidle_lock);

	if (dev->registered)
		goto out_unlock;

	__cpuidle_device_init(dev);

	ret = __cpuidle_register_device(dev);
	if (ret)
		goto out_unlock;

	ret = cpuidle_add_sysfs(dev);
	if (ret)
		goto out_unregister;

	ret = cpuidle_enable_device(dev);
	if (ret)
		goto out_sysfs;

	cpuidle_install_idle_handler();

out_unlock:
	mutex_unlock(&cpuidle_lock);

	return ret;

out_sysfs:
	cpuidle_remove_sysfs(dev);
out_unregister:
	__cpuidle_unregister_device(dev);
	goto out_unlock;
}

EXPORT_SYMBOL_GPL(cpuidle_register_device);

 
void cpuidle_unregister_device(struct cpuidle_device *dev)
{
	if (!dev || dev->registered == 0)
		return;

	cpuidle_pause_and_lock();

	cpuidle_disable_device(dev);

	cpuidle_remove_sysfs(dev);

	__cpuidle_unregister_device(dev);

	cpuidle_coupled_unregister_device(dev);

	cpuidle_resume_and_unlock();
}

EXPORT_SYMBOL_GPL(cpuidle_unregister_device);

 
void cpuidle_unregister(struct cpuidle_driver *drv)
{
	int cpu;
	struct cpuidle_device *device;

	for_each_cpu(cpu, drv->cpumask) {
		device = &per_cpu(cpuidle_dev, cpu);
		cpuidle_unregister_device(device);
	}

	cpuidle_unregister_driver(drv);
}
EXPORT_SYMBOL_GPL(cpuidle_unregister);

 
int cpuidle_register(struct cpuidle_driver *drv,
		     const struct cpumask *const coupled_cpus)
{
	int ret, cpu;
	struct cpuidle_device *device;

	ret = cpuidle_register_driver(drv);
	if (ret) {
		pr_err("failed to register cpuidle driver\n");
		return ret;
	}

	for_each_cpu(cpu, drv->cpumask) {
		device = &per_cpu(cpuidle_dev, cpu);
		device->cpu = cpu;

#ifdef CONFIG_ARCH_NEEDS_CPU_IDLE_COUPLED
		 
		if (coupled_cpus)
			device->coupled_cpus = *coupled_cpus;
#endif
		ret = cpuidle_register_device(device);
		if (!ret)
			continue;

		pr_err("Failed to register cpuidle device for cpu%d\n", cpu);

		cpuidle_unregister(drv);
		break;
	}

	return ret;
}
EXPORT_SYMBOL_GPL(cpuidle_register);

 
static int __init cpuidle_init(void)
{
	if (cpuidle_disabled())
		return -ENODEV;

	return cpuidle_add_interface();
}

module_param(off, int, 0444);
module_param_string(governor, param_governor, CPUIDLE_NAME_LEN, 0444);
core_initcall(cpuidle_init);
