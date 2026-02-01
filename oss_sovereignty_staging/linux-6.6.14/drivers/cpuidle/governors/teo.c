
 

 

#include <linux/cpuidle.h>
#include <linux/jiffies.h>
#include <linux/kernel.h>
#include <linux/sched.h>
#include <linux/sched/clock.h>
#include <linux/sched/topology.h>
#include <linux/tick.h>

#include "gov.h"

 
#define UTIL_THRESHOLD_SHIFT 6

 
#define PULSE		1024
#define DECAY_SHIFT	3

 
#define NR_RECENT	9

 
struct teo_bin {
	unsigned int intercepts;
	unsigned int hits;
	unsigned int recent;
};

 
struct teo_cpu {
	s64 time_span_ns;
	s64 sleep_length_ns;
	struct teo_bin state_bins[CPUIDLE_STATE_MAX];
	unsigned int total;
	int next_recent_idx;
	int recent_idx[NR_RECENT];
	unsigned int tick_hits;
	unsigned long util_threshold;
};

static DEFINE_PER_CPU(struct teo_cpu, teo_cpus);

 
#ifdef CONFIG_SMP
static bool teo_cpu_is_utilized(int cpu, struct teo_cpu *cpu_data)
{
	return sched_cpu_util(cpu) > cpu_data->util_threshold;
}
#else
static bool teo_cpu_is_utilized(int cpu, struct teo_cpu *cpu_data)
{
	return false;
}
#endif

 
static void teo_update(struct cpuidle_driver *drv, struct cpuidle_device *dev)
{
	struct teo_cpu *cpu_data = per_cpu_ptr(&teo_cpus, dev->cpu);
	int i, idx_timer = 0, idx_duration = 0;
	s64 target_residency_ns;
	u64 measured_ns;

	if (cpu_data->time_span_ns >= cpu_data->sleep_length_ns) {
		 
		measured_ns = U64_MAX;
	} else {
		u64 lat_ns = drv->states[dev->last_state_idx].exit_latency_ns;

		 
		measured_ns = dev->last_residency_ns;
		 
		if (measured_ns >= lat_ns)
			measured_ns -= lat_ns / 2;
		else
			measured_ns /= 2;
	}

	cpu_data->total = 0;

	 
	for (i = 0; i < drv->state_count; i++) {
		struct teo_bin *bin = &cpu_data->state_bins[i];

		bin->hits -= bin->hits >> DECAY_SHIFT;
		bin->intercepts -= bin->intercepts >> DECAY_SHIFT;

		cpu_data->total += bin->hits + bin->intercepts;

		target_residency_ns = drv->states[i].target_residency_ns;

		if (target_residency_ns <= cpu_data->sleep_length_ns) {
			idx_timer = i;
			if (target_residency_ns <= measured_ns)
				idx_duration = i;
		}
	}

	i = cpu_data->next_recent_idx++;
	if (cpu_data->next_recent_idx >= NR_RECENT)
		cpu_data->next_recent_idx = 0;

	if (cpu_data->recent_idx[i] >= 0)
		cpu_data->state_bins[cpu_data->recent_idx[i]].recent--;

	 
	if (target_residency_ns < TICK_NSEC) {
		cpu_data->tick_hits -= cpu_data->tick_hits >> DECAY_SHIFT;

		cpu_data->total += cpu_data->tick_hits;

		if (TICK_NSEC <= cpu_data->sleep_length_ns) {
			idx_timer = drv->state_count;
			if (TICK_NSEC <= measured_ns) {
				cpu_data->tick_hits += PULSE;
				goto end;
			}
		}
	}

	 
	if (idx_timer == idx_duration) {
		cpu_data->state_bins[idx_timer].hits += PULSE;
		cpu_data->recent_idx[i] = -1;
	} else {
		cpu_data->state_bins[idx_duration].intercepts += PULSE;
		cpu_data->state_bins[idx_duration].recent++;
		cpu_data->recent_idx[i] = idx_duration;
	}

end:
	cpu_data->total += PULSE;
}

static bool teo_state_ok(int i, struct cpuidle_driver *drv)
{
	return !tick_nohz_tick_stopped() ||
		drv->states[i].target_residency_ns >= TICK_NSEC;
}

 
static int teo_find_shallower_state(struct cpuidle_driver *drv,
				    struct cpuidle_device *dev, int state_idx,
				    s64 duration_ns, bool no_poll)
{
	int i;

	for (i = state_idx - 1; i >= 0; i--) {
		if (dev->states_usage[i].disable ||
				(no_poll && drv->states[i].flags & CPUIDLE_FLAG_POLLING))
			continue;

		state_idx = i;
		if (drv->states[i].target_residency_ns <= duration_ns)
			break;
	}
	return state_idx;
}

 
static int teo_select(struct cpuidle_driver *drv, struct cpuidle_device *dev,
		      bool *stop_tick)
{
	struct teo_cpu *cpu_data = per_cpu_ptr(&teo_cpus, dev->cpu);
	s64 latency_req = cpuidle_governor_latency_req(dev->cpu);
	ktime_t delta_tick = TICK_NSEC / 2;
	unsigned int tick_intercept_sum = 0;
	unsigned int idx_intercept_sum = 0;
	unsigned int intercept_sum = 0;
	unsigned int idx_recent_sum = 0;
	unsigned int recent_sum = 0;
	unsigned int idx_hit_sum = 0;
	unsigned int hit_sum = 0;
	int constraint_idx = 0;
	int idx0 = 0, idx = -1;
	bool alt_intercepts, alt_recent;
	bool cpu_utilized;
	s64 duration_ns;
	int i;

	if (dev->last_state_idx >= 0) {
		teo_update(drv, dev);
		dev->last_state_idx = -1;
	}

	cpu_data->time_span_ns = local_clock();
	 
	cpu_data->sleep_length_ns = KTIME_MAX;

	 
	if (drv->state_count < 2) {
		idx = 0;
		goto out_tick;
	}

	if (!dev->states_usage[0].disable)
		idx = 0;

	cpu_utilized = teo_cpu_is_utilized(dev->cpu, cpu_data);
	 
	if (drv->state_count < 3 && cpu_utilized) {
		 
		if ((!idx && !(drv->states[0].flags & CPUIDLE_FLAG_POLLING) &&
		    teo_state_ok(0, drv)) || dev->states_usage[1].disable) {
			idx = 0;
			goto out_tick;
		}
		 
		idx = 1;
		duration_ns = drv->states[1].target_residency_ns;
		goto end;
	}

	 
	for (i = 1; i < drv->state_count; i++) {
		struct teo_bin *prev_bin = &cpu_data->state_bins[i-1];
		struct cpuidle_state *s = &drv->states[i];

		 
		intercept_sum += prev_bin->intercepts;
		hit_sum += prev_bin->hits;
		recent_sum += prev_bin->recent;

		if (dev->states_usage[i].disable)
			continue;

		if (idx < 0)
			idx0 = i;  

		idx = i;

		if (s->exit_latency_ns <= latency_req)
			constraint_idx = i;

		 
		idx_intercept_sum = intercept_sum;
		idx_hit_sum = hit_sum;
		idx_recent_sum = recent_sum;
	}

	 
	if (idx < 0) {
		idx = 0;  
		goto out_tick;
	}

	if (idx == idx0) {
		 
		duration_ns = drv->states[idx].target_residency_ns;
		goto end;
	}

	tick_intercept_sum = intercept_sum +
			cpu_data->state_bins[drv->state_count-1].intercepts;

	 
	alt_intercepts = 2 * idx_intercept_sum > cpu_data->total - idx_hit_sum;
	alt_recent = idx_recent_sum > NR_RECENT / 2;
	if (alt_recent || alt_intercepts) {
		int first_suitable_idx = idx;

		 
		intercept_sum = 0;
		recent_sum = 0;

		for (i = idx - 1; i >= 0; i--) {
			struct teo_bin *bin = &cpu_data->state_bins[i];

			intercept_sum += bin->intercepts;
			recent_sum += bin->recent;

			if ((!alt_recent || 2 * recent_sum > idx_recent_sum) &&
			    (!alt_intercepts ||
			     2 * intercept_sum > idx_intercept_sum)) {
				 
				if (teo_state_ok(i, drv) &&
				    !dev->states_usage[i].disable)
					idx = i;
				else
					idx = first_suitable_idx;

				break;
			}

			if (dev->states_usage[i].disable)
				continue;

			if (!teo_state_ok(i, drv)) {
				 
				if (first_suitable_idx != idx)
					continue;

				break;
			}

			first_suitable_idx = i;
		}
	}

	 
	if (idx > constraint_idx)
		idx = constraint_idx;

	 
	if (cpu_utilized) {
		i = teo_find_shallower_state(drv, dev, idx, KTIME_MAX, true);
		if (teo_state_ok(i, drv))
			idx = i;
	}

	 
	if (!idx)
		goto out_tick;

	 
	if ((drv->states[0].flags & CPUIDLE_FLAG_POLLING) &&
	    drv->states[idx].target_residency_ns < RESIDENCY_THRESHOLD_NS)
		goto out_tick;

	duration_ns = tick_nohz_get_sleep_length(&delta_tick);
	cpu_data->sleep_length_ns = duration_ns;

	 
	if (drv->states[idx].target_residency_ns > duration_ns) {
		i = teo_find_shallower_state(drv, dev, idx, duration_ns, false);
		if (teo_state_ok(i, drv))
			idx = i;
	}

	 
	if (drv->states[idx].target_residency_ns < TICK_NSEC &&
	    tick_intercept_sum > cpu_data->total / 2 + cpu_data->total / 8)
		duration_ns = TICK_NSEC / 2;

end:
	 
	if ((!(drv->states[idx].flags & CPUIDLE_FLAG_POLLING) &&
	    duration_ns >= TICK_NSEC) || tick_nohz_tick_stopped())
		return idx;

	 
	if (idx > idx0 &&
	    drv->states[idx].target_residency_ns > delta_tick)
		idx = teo_find_shallower_state(drv, dev, idx, delta_tick, false);

out_tick:
	*stop_tick = false;
	return idx;
}

 
static void teo_reflect(struct cpuidle_device *dev, int state)
{
	struct teo_cpu *cpu_data = per_cpu_ptr(&teo_cpus, dev->cpu);

	dev->last_state_idx = state;
	 
	if (dev->poll_time_limit ||
	    (tick_nohz_idle_got_tick() && cpu_data->sleep_length_ns > TICK_NSEC)) {
		dev->poll_time_limit = false;
		cpu_data->time_span_ns = cpu_data->sleep_length_ns;
	} else {
		cpu_data->time_span_ns = local_clock() - cpu_data->time_span_ns;
	}
}

 
static int teo_enable_device(struct cpuidle_driver *drv,
			     struct cpuidle_device *dev)
{
	struct teo_cpu *cpu_data = per_cpu_ptr(&teo_cpus, dev->cpu);
	unsigned long max_capacity = arch_scale_cpu_capacity(dev->cpu);
	int i;

	memset(cpu_data, 0, sizeof(*cpu_data));
	cpu_data->util_threshold = max_capacity >> UTIL_THRESHOLD_SHIFT;

	for (i = 0; i < NR_RECENT; i++)
		cpu_data->recent_idx[i] = -1;

	return 0;
}

static struct cpuidle_governor teo_governor = {
	.name =		"teo",
	.rating =	19,
	.enable =	teo_enable_device,
	.select =	teo_select,
	.reflect =	teo_reflect,
};

static int __init teo_governor_init(void)
{
	return cpuidle_register_governor(&teo_governor);
}

postcore_initcall(teo_governor_init);
