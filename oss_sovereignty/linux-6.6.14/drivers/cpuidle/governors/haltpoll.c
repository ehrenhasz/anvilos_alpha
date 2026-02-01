
 

#include <linux/kernel.h>
#include <linux/cpuidle.h>
#include <linux/time.h>
#include <linux/ktime.h>
#include <linux/hrtimer.h>
#include <linux/tick.h>
#include <linux/sched.h>
#include <linux/module.h>
#include <linux/kvm_para.h>
#include <trace/events/power.h>

static unsigned int guest_halt_poll_ns __read_mostly = 200000;
module_param(guest_halt_poll_ns, uint, 0644);

 
static unsigned int guest_halt_poll_shrink __read_mostly = 2;
module_param(guest_halt_poll_shrink, uint, 0644);

 
static unsigned int guest_halt_poll_grow __read_mostly = 2;
module_param(guest_halt_poll_grow, uint, 0644);

 
static unsigned int guest_halt_poll_grow_start __read_mostly = 50000;
module_param(guest_halt_poll_grow_start, uint, 0644);

 
static bool guest_halt_poll_allow_shrink __read_mostly = true;
module_param(guest_halt_poll_allow_shrink, bool, 0644);

 
static int haltpoll_select(struct cpuidle_driver *drv,
			   struct cpuidle_device *dev,
			   bool *stop_tick)
{
	s64 latency_req = cpuidle_governor_latency_req(dev->cpu);

	if (!drv->state_count || latency_req == 0) {
		*stop_tick = false;
		return 0;
	}

	if (dev->poll_limit_ns == 0)
		return 1;

	 
	if (dev->last_state_idx == 0) {
		 
		if (dev->poll_time_limit == true)
			return 1;

		*stop_tick = false;
		 
		return 0;
	}

	*stop_tick = false;
	 
	return 0;
}

static void adjust_poll_limit(struct cpuidle_device *dev, u64 block_ns)
{
	unsigned int val;

	 
	if (block_ns > dev->poll_limit_ns && block_ns <= guest_halt_poll_ns) {
		val = dev->poll_limit_ns * guest_halt_poll_grow;

		if (val < guest_halt_poll_grow_start)
			val = guest_halt_poll_grow_start;
		if (val > guest_halt_poll_ns)
			val = guest_halt_poll_ns;

		trace_guest_halt_poll_ns_grow(val, dev->poll_limit_ns);
		dev->poll_limit_ns = val;
	} else if (block_ns > guest_halt_poll_ns &&
		   guest_halt_poll_allow_shrink) {
		unsigned int shrink = guest_halt_poll_shrink;

		val = dev->poll_limit_ns;
		if (shrink == 0)
			val = 0;
		else
			val /= shrink;
		trace_guest_halt_poll_ns_shrink(val, dev->poll_limit_ns);
		dev->poll_limit_ns = val;
	}
}

 
static void haltpoll_reflect(struct cpuidle_device *dev, int index)
{
	dev->last_state_idx = index;

	if (index != 0)
		adjust_poll_limit(dev, dev->last_residency_ns);
}

 
static int haltpoll_enable_device(struct cpuidle_driver *drv,
				  struct cpuidle_device *dev)
{
	dev->poll_limit_ns = 0;

	return 0;
}

static struct cpuidle_governor haltpoll_governor = {
	.name =			"haltpoll",
	.rating =		9,
	.enable =		haltpoll_enable_device,
	.select =		haltpoll_select,
	.reflect =		haltpoll_reflect,
};

static int __init init_haltpoll(void)
{
	if (kvm_para_available())
		return cpuidle_register_governor(&haltpoll_governor);

	return 0;
}

postcore_initcall(init_haltpoll);
