
 

#include <linux/kernel.h>
#include <linux/cpuidle.h>
#include <linux/time.h>
#include <linux/ktime.h>
#include <linux/hrtimer.h>
#include <linux/tick.h>
#include <linux/sched.h>
#include <linux/sched/loadavg.h>
#include <linux/sched/stat.h>
#include <linux/math64.h>

#include "gov.h"

#define BUCKETS 12
#define INTERVAL_SHIFT 3
#define INTERVALS (1UL << INTERVAL_SHIFT)
#define RESOLUTION 1024
#define DECAY 8
#define MAX_INTERESTING (50000 * NSEC_PER_USEC)

 

struct menu_device {
	int             needs_update;
	int             tick_wakeup;

	u64		next_timer_ns;
	unsigned int	bucket;
	unsigned int	correction_factor[BUCKETS];
	unsigned int	intervals[INTERVALS];
	int		interval_ptr;
};

static inline int which_bucket(u64 duration_ns, unsigned int nr_iowaiters)
{
	int bucket = 0;

	 
	if (nr_iowaiters)
		bucket = BUCKETS/2;

	if (duration_ns < 10ULL * NSEC_PER_USEC)
		return bucket;
	if (duration_ns < 100ULL * NSEC_PER_USEC)
		return bucket + 1;
	if (duration_ns < 1000ULL * NSEC_PER_USEC)
		return bucket + 2;
	if (duration_ns < 10000ULL * NSEC_PER_USEC)
		return bucket + 3;
	if (duration_ns < 100000ULL * NSEC_PER_USEC)
		return bucket + 4;
	return bucket + 5;
}

 
static inline int performance_multiplier(unsigned int nr_iowaiters)
{
	 
	return 1 + 10 * nr_iowaiters;
}

static DEFINE_PER_CPU(struct menu_device, menu_devices);

static void menu_update(struct cpuidle_driver *drv, struct cpuidle_device *dev);

 
static unsigned int get_typical_interval(struct menu_device *data)
{
	int i, divisor;
	unsigned int min, max, thresh, avg;
	uint64_t sum, variance;

	thresh = INT_MAX;  

again:

	 
	min = UINT_MAX;
	max = 0;
	sum = 0;
	divisor = 0;
	for (i = 0; i < INTERVALS; i++) {
		unsigned int value = data->intervals[i];
		if (value <= thresh) {
			sum += value;
			divisor++;
			if (value > max)
				max = value;

			if (value < min)
				min = value;
		}
	}

	if (!max)
		return UINT_MAX;

	if (divisor == INTERVALS)
		avg = sum >> INTERVAL_SHIFT;
	else
		avg = div_u64(sum, divisor);

	 
	variance = 0;
	for (i = 0; i < INTERVALS; i++) {
		unsigned int value = data->intervals[i];
		if (value <= thresh) {
			int64_t diff = (int64_t)value - avg;
			variance += diff * diff;
		}
	}
	if (divisor == INTERVALS)
		variance >>= INTERVAL_SHIFT;
	else
		do_div(variance, divisor);

	 
	if (likely(variance <= U64_MAX/36)) {
		if ((((u64)avg*avg > variance*36) && (divisor * 4 >= INTERVALS * 3))
							|| variance <= 400) {
			return avg;
		}
	}

	 
	if ((divisor * 4) <= INTERVALS * 3)
		return UINT_MAX;

	thresh = max - 1;
	goto again;
}

 
static int menu_select(struct cpuidle_driver *drv, struct cpuidle_device *dev,
		       bool *stop_tick)
{
	struct menu_device *data = this_cpu_ptr(&menu_devices);
	s64 latency_req = cpuidle_governor_latency_req(dev->cpu);
	u64 predicted_ns;
	u64 interactivity_req;
	unsigned int nr_iowaiters;
	ktime_t delta, delta_tick;
	int i, idx;

	if (data->needs_update) {
		menu_update(drv, dev);
		data->needs_update = 0;
	}

	nr_iowaiters = nr_iowait_cpu(dev->cpu);

	 
	predicted_ns = get_typical_interval(data) * NSEC_PER_USEC;
	if (predicted_ns > RESIDENCY_THRESHOLD_NS) {
		unsigned int timer_us;

		 
		delta = tick_nohz_get_sleep_length(&delta_tick);
		if (unlikely(delta < 0)) {
			delta = 0;
			delta_tick = 0;
		}

		data->next_timer_ns = delta;
		data->bucket = which_bucket(data->next_timer_ns, nr_iowaiters);

		 
		timer_us = div_u64((RESOLUTION * DECAY * NSEC_PER_USEC) / 2 +
					data->next_timer_ns *
						data->correction_factor[data->bucket],
				   RESOLUTION * DECAY * NSEC_PER_USEC);
		 
		predicted_ns = min((u64)timer_us * NSEC_PER_USEC, predicted_ns);
	} else {
		 
		data->next_timer_ns = KTIME_MAX;
		delta_tick = TICK_NSEC / 2;
		data->bucket = which_bucket(KTIME_MAX, nr_iowaiters);
	}

	if (unlikely(drv->state_count <= 1 || latency_req == 0) ||
	    ((data->next_timer_ns < drv->states[1].target_residency_ns ||
	      latency_req < drv->states[1].exit_latency_ns) &&
	     !dev->states_usage[0].disable)) {
		 
		*stop_tick = !(drv->states[0].flags & CPUIDLE_FLAG_POLLING);
		return 0;
	}

	if (tick_nohz_tick_stopped()) {
		 
		if (predicted_ns < TICK_NSEC)
			predicted_ns = data->next_timer_ns;
	} else {
		 
		interactivity_req = div64_u64(predicted_ns,
					      performance_multiplier(nr_iowaiters));
		if (latency_req > interactivity_req)
			latency_req = interactivity_req;
	}

	 
	idx = -1;
	for (i = 0; i < drv->state_count; i++) {
		struct cpuidle_state *s = &drv->states[i];

		if (dev->states_usage[i].disable)
			continue;

		if (idx == -1)
			idx = i;  

		if (s->target_residency_ns > predicted_ns) {
			 
			if ((drv->states[idx].flags & CPUIDLE_FLAG_POLLING) &&
			    s->exit_latency_ns <= latency_req &&
			    s->target_residency_ns <= data->next_timer_ns) {
				predicted_ns = s->target_residency_ns;
				idx = i;
				break;
			}
			if (predicted_ns < TICK_NSEC)
				break;

			if (!tick_nohz_tick_stopped()) {
				 
				predicted_ns = drv->states[idx].target_residency_ns;
				break;
			}

			 
			if (drv->states[idx].target_residency_ns < TICK_NSEC &&
			    s->target_residency_ns <= delta_tick)
				idx = i;

			return idx;
		}
		if (s->exit_latency_ns > latency_req)
			break;

		idx = i;
	}

	if (idx == -1)
		idx = 0;  

	 
	if (((drv->states[idx].flags & CPUIDLE_FLAG_POLLING) ||
	     predicted_ns < TICK_NSEC) && !tick_nohz_tick_stopped()) {
		*stop_tick = false;

		if (idx > 0 && drv->states[idx].target_residency_ns > delta_tick) {
			 
			for (i = idx - 1; i >= 0; i--) {
				if (dev->states_usage[i].disable)
					continue;

				idx = i;
				if (drv->states[i].target_residency_ns <= delta_tick)
					break;
			}
		}
	}

	return idx;
}

 
static void menu_reflect(struct cpuidle_device *dev, int index)
{
	struct menu_device *data = this_cpu_ptr(&menu_devices);

	dev->last_state_idx = index;
	data->needs_update = 1;
	data->tick_wakeup = tick_nohz_idle_got_tick();
}

 
static void menu_update(struct cpuidle_driver *drv, struct cpuidle_device *dev)
{
	struct menu_device *data = this_cpu_ptr(&menu_devices);
	int last_idx = dev->last_state_idx;
	struct cpuidle_state *target = &drv->states[last_idx];
	u64 measured_ns;
	unsigned int new_factor;

	 

	if (data->tick_wakeup && data->next_timer_ns > TICK_NSEC) {
		 
		measured_ns = 9 * MAX_INTERESTING / 10;
	} else if ((drv->states[last_idx].flags & CPUIDLE_FLAG_POLLING) &&
		   dev->poll_time_limit) {
		 
		measured_ns = data->next_timer_ns;
	} else {
		 
		measured_ns = dev->last_residency_ns;

		 
		if (measured_ns > 2 * target->exit_latency_ns)
			measured_ns -= target->exit_latency_ns;
		else
			measured_ns /= 2;
	}

	 
	if (measured_ns > data->next_timer_ns)
		measured_ns = data->next_timer_ns;

	 
	new_factor = data->correction_factor[data->bucket];
	new_factor -= new_factor / DECAY;

	if (data->next_timer_ns > 0 && measured_ns < MAX_INTERESTING)
		new_factor += div64_u64(RESOLUTION * measured_ns,
					data->next_timer_ns);
	else
		 
		new_factor += RESOLUTION;

	 
	if (DECAY == 1 && unlikely(new_factor == 0))
		new_factor = 1;

	data->correction_factor[data->bucket] = new_factor;

	 
	data->intervals[data->interval_ptr++] = ktime_to_us(measured_ns);
	if (data->interval_ptr >= INTERVALS)
		data->interval_ptr = 0;
}

 
static int menu_enable_device(struct cpuidle_driver *drv,
				struct cpuidle_device *dev)
{
	struct menu_device *data = &per_cpu(menu_devices, dev->cpu);
	int i;

	memset(data, 0, sizeof(struct menu_device));

	 
	for(i = 0; i < BUCKETS; i++)
		data->correction_factor[i] = RESOLUTION * DECAY;

	return 0;
}

static struct cpuidle_governor menu_governor = {
	.name =		"menu",
	.rating =	20,
	.enable =	menu_enable_device,
	.select =	menu_select,
	.reflect =	menu_reflect,
};

 
static int __init init_menu(void)
{
	return cpuidle_register_governor(&menu_governor);
}

postcore_initcall(init_menu);
