
 

#define pr_fmt(fmt) "Power allocator: " fmt

#include <linux/slab.h>
#include <linux/thermal.h>

#define CREATE_TRACE_POINTS
#include "thermal_trace_ipa.h"

#include "thermal_core.h"

#define INVALID_TRIP -1

#define FRAC_BITS 10
#define int_to_frac(x) ((x) << FRAC_BITS)
#define frac_to_int(x) ((x) >> FRAC_BITS)

 
static inline s64 mul_frac(s64 x, s64 y)
{
	return (x * y) >> FRAC_BITS;
}

 
static inline s64 div_frac(s64 x, s64 y)
{
	return div_s64(x << FRAC_BITS, y);
}

 
struct power_allocator_params {
	bool allocated_tzp;
	s64 err_integral;
	s32 prev_err;
	int trip_switch_on;
	int trip_max_desired_temperature;
	u32 sustainable_power;
};

 
static u32 estimate_sustainable_power(struct thermal_zone_device *tz)
{
	u32 sustainable_power = 0;
	struct thermal_instance *instance;
	struct power_allocator_params *params = tz->governor_data;

	list_for_each_entry(instance, &tz->thermal_instances, tz_node) {
		struct thermal_cooling_device *cdev = instance->cdev;
		u32 min_power;

		if (instance->trip != params->trip_max_desired_temperature)
			continue;

		if (!cdev_is_power_actor(cdev))
			continue;

		if (cdev->ops->state2power(cdev, instance->upper, &min_power))
			continue;

		sustainable_power += min_power;
	}

	return sustainable_power;
}

 
static void estimate_pid_constants(struct thermal_zone_device *tz,
				   u32 sustainable_power, int trip_switch_on,
				   int control_temp)
{
	struct thermal_trip trip;
	u32 temperature_threshold = control_temp;
	int ret;
	s32 k_i;

	ret = __thermal_zone_get_trip(tz, trip_switch_on, &trip);
	if (!ret)
		temperature_threshold -= trip.temperature;

	 
	if (!temperature_threshold)
		return;

	tz->tzp->k_po = int_to_frac(sustainable_power) /
		temperature_threshold;

	tz->tzp->k_pu = int_to_frac(2 * sustainable_power) /
		temperature_threshold;

	k_i = tz->tzp->k_pu / 10;
	tz->tzp->k_i = k_i > 0 ? k_i : 1;

	 
}

 
static u32 get_sustainable_power(struct thermal_zone_device *tz,
				 struct power_allocator_params *params,
				 int control_temp)
{
	u32 sustainable_power;

	if (!tz->tzp->sustainable_power)
		sustainable_power = estimate_sustainable_power(tz);
	else
		sustainable_power = tz->tzp->sustainable_power;

	 
	if (sustainable_power != params->sustainable_power) {
		estimate_pid_constants(tz, sustainable_power,
				       params->trip_switch_on, control_temp);

		 
		tz->tzp->sustainable_power = sustainable_power;
		params->sustainable_power = sustainable_power;
	}

	return sustainable_power;
}

 
static u32 pid_controller(struct thermal_zone_device *tz,
			  int control_temp,
			  u32 max_allocatable_power)
{
	s64 p, i, d, power_range;
	s32 err, max_power_frac;
	u32 sustainable_power;
	struct power_allocator_params *params = tz->governor_data;

	max_power_frac = int_to_frac(max_allocatable_power);

	sustainable_power = get_sustainable_power(tz, params, control_temp);

	err = control_temp - tz->temperature;
	err = int_to_frac(err);

	 
	p = mul_frac(err < 0 ? tz->tzp->k_po : tz->tzp->k_pu, err);

	 
	i = mul_frac(tz->tzp->k_i, params->err_integral);

	if (err < int_to_frac(tz->tzp->integral_cutoff)) {
		s64 i_next = i + mul_frac(tz->tzp->k_i, err);

		if (abs(i_next) < max_power_frac) {
			i = i_next;
			params->err_integral += err;
		}
	}

	 
	d = mul_frac(tz->tzp->k_d, err - params->prev_err);
	d = div_frac(d, jiffies_to_msecs(tz->passive_delay_jiffies));
	params->prev_err = err;

	power_range = p + i + d;

	 
	power_range = sustainable_power + frac_to_int(power_range);

	power_range = clamp(power_range, (s64)0, (s64)max_allocatable_power);

	trace_thermal_power_allocator_pid(tz, frac_to_int(err),
					  frac_to_int(params->err_integral),
					  frac_to_int(p), frac_to_int(i),
					  frac_to_int(d), power_range);

	return power_range;
}

 
static int
power_actor_set_power(struct thermal_cooling_device *cdev,
		      struct thermal_instance *instance, u32 power)
{
	unsigned long state;
	int ret;

	ret = cdev->ops->power2state(cdev, power, &state);
	if (ret)
		return ret;

	instance->target = clamp_val(state, instance->lower, instance->upper);
	mutex_lock(&cdev->lock);
	__thermal_cdev_update(cdev);
	mutex_unlock(&cdev->lock);

	return 0;
}

 
static void divvy_up_power(u32 *req_power, u32 *max_power, int num_actors,
			   u32 total_req_power, u32 power_range,
			   u32 *granted_power, u32 *extra_actor_power)
{
	u32 extra_power, capped_extra_power;
	int i;

	 
	if (!total_req_power)
		total_req_power = 1;

	capped_extra_power = 0;
	extra_power = 0;
	for (i = 0; i < num_actors; i++) {
		u64 req_range = (u64)req_power[i] * power_range;

		granted_power[i] = DIV_ROUND_CLOSEST_ULL(req_range,
							 total_req_power);

		if (granted_power[i] > max_power[i]) {
			extra_power += granted_power[i] - max_power[i];
			granted_power[i] = max_power[i];
		}

		extra_actor_power[i] = max_power[i] - granted_power[i];
		capped_extra_power += extra_actor_power[i];
	}

	if (!extra_power)
		return;

	 
	extra_power = min(extra_power, capped_extra_power);
	if (capped_extra_power > 0)
		for (i = 0; i < num_actors; i++) {
			u64 extra_range = (u64)extra_actor_power[i] * extra_power;
			granted_power[i] += DIV_ROUND_CLOSEST_ULL(extra_range,
							 capped_extra_power);
		}
}

static int allocate_power(struct thermal_zone_device *tz,
			  int control_temp)
{
	struct thermal_instance *instance;
	struct power_allocator_params *params = tz->governor_data;
	u32 *req_power, *max_power, *granted_power, *extra_actor_power;
	u32 *weighted_req_power;
	u32 total_req_power, max_allocatable_power, total_weighted_req_power;
	u32 total_granted_power, power_range;
	int i, num_actors, total_weight, ret = 0;
	int trip_max_desired_temperature = params->trip_max_desired_temperature;

	num_actors = 0;
	total_weight = 0;
	list_for_each_entry(instance, &tz->thermal_instances, tz_node) {
		if ((instance->trip == trip_max_desired_temperature) &&
		    cdev_is_power_actor(instance->cdev)) {
			num_actors++;
			total_weight += instance->weight;
		}
	}

	if (!num_actors)
		return -ENODEV;

	 
	BUILD_BUG_ON(sizeof(*req_power) != sizeof(*max_power));
	BUILD_BUG_ON(sizeof(*req_power) != sizeof(*granted_power));
	BUILD_BUG_ON(sizeof(*req_power) != sizeof(*extra_actor_power));
	BUILD_BUG_ON(sizeof(*req_power) != sizeof(*weighted_req_power));
	req_power = kcalloc(num_actors * 5, sizeof(*req_power), GFP_KERNEL);
	if (!req_power)
		return -ENOMEM;

	max_power = &req_power[num_actors];
	granted_power = &req_power[2 * num_actors];
	extra_actor_power = &req_power[3 * num_actors];
	weighted_req_power = &req_power[4 * num_actors];

	i = 0;
	total_weighted_req_power = 0;
	total_req_power = 0;
	max_allocatable_power = 0;

	list_for_each_entry(instance, &tz->thermal_instances, tz_node) {
		int weight;
		struct thermal_cooling_device *cdev = instance->cdev;

		if (instance->trip != trip_max_desired_temperature)
			continue;

		if (!cdev_is_power_actor(cdev))
			continue;

		if (cdev->ops->get_requested_power(cdev, &req_power[i]))
			continue;

		if (!total_weight)
			weight = 1 << FRAC_BITS;
		else
			weight = instance->weight;

		weighted_req_power[i] = frac_to_int(weight * req_power[i]);

		if (cdev->ops->state2power(cdev, instance->lower,
					   &max_power[i]))
			continue;

		total_req_power += req_power[i];
		max_allocatable_power += max_power[i];
		total_weighted_req_power += weighted_req_power[i];

		i++;
	}

	power_range = pid_controller(tz, control_temp, max_allocatable_power);

	divvy_up_power(weighted_req_power, max_power, num_actors,
		       total_weighted_req_power, power_range, granted_power,
		       extra_actor_power);

	total_granted_power = 0;
	i = 0;
	list_for_each_entry(instance, &tz->thermal_instances, tz_node) {
		if (instance->trip != trip_max_desired_temperature)
			continue;

		if (!cdev_is_power_actor(instance->cdev))
			continue;

		power_actor_set_power(instance->cdev, instance,
				      granted_power[i]);
		total_granted_power += granted_power[i];

		i++;
	}

	trace_thermal_power_allocator(tz, req_power, total_req_power,
				      granted_power, total_granted_power,
				      num_actors, power_range,
				      max_allocatable_power, tz->temperature,
				      control_temp - tz->temperature);

	kfree(req_power);

	return ret;
}

 
static void get_governor_trips(struct thermal_zone_device *tz,
			       struct power_allocator_params *params)
{
	int i, last_active, last_passive;
	bool found_first_passive;

	found_first_passive = false;
	last_active = INVALID_TRIP;
	last_passive = INVALID_TRIP;

	for (i = 0; i < tz->num_trips; i++) {
		struct thermal_trip trip;
		int ret;

		ret = __thermal_zone_get_trip(tz, i, &trip);
		if (ret) {
			dev_warn(&tz->device,
				 "Failed to get trip point %d type: %d\n", i,
				 ret);
			continue;
		}

		if (trip.type == THERMAL_TRIP_PASSIVE) {
			if (!found_first_passive) {
				params->trip_switch_on = i;
				found_first_passive = true;
			} else  {
				last_passive = i;
			}
		} else if (trip.type == THERMAL_TRIP_ACTIVE) {
			last_active = i;
		} else {
			break;
		}
	}

	if (last_passive != INVALID_TRIP) {
		params->trip_max_desired_temperature = last_passive;
	} else if (found_first_passive) {
		params->trip_max_desired_temperature = params->trip_switch_on;
		params->trip_switch_on = INVALID_TRIP;
	} else {
		params->trip_switch_on = INVALID_TRIP;
		params->trip_max_desired_temperature = last_active;
	}
}

static void reset_pid_controller(struct power_allocator_params *params)
{
	params->err_integral = 0;
	params->prev_err = 0;
}

static void allow_maximum_power(struct thermal_zone_device *tz, bool update)
{
	struct thermal_instance *instance;
	struct power_allocator_params *params = tz->governor_data;
	u32 req_power;

	list_for_each_entry(instance, &tz->thermal_instances, tz_node) {
		struct thermal_cooling_device *cdev = instance->cdev;

		if ((instance->trip != params->trip_max_desired_temperature) ||
		    (!cdev_is_power_actor(instance->cdev)))
			continue;

		instance->target = 0;
		mutex_lock(&instance->cdev->lock);
		 
		cdev->ops->get_requested_power(cdev, &req_power);

		if (update)
			__thermal_cdev_update(instance->cdev);

		mutex_unlock(&instance->cdev->lock);
	}
}

 
static int check_power_actors(struct thermal_zone_device *tz)
{
	struct thermal_instance *instance;
	int ret = 0;

	list_for_each_entry(instance, &tz->thermal_instances, tz_node) {
		if (!cdev_is_power_actor(instance->cdev)) {
			dev_warn(&tz->device, "power_allocator: %s is not a power actor\n",
				 instance->cdev->type);
			ret = -EINVAL;
		}
	}

	return ret;
}

 
static int power_allocator_bind(struct thermal_zone_device *tz)
{
	int ret;
	struct power_allocator_params *params;
	struct thermal_trip trip;

	ret = check_power_actors(tz);
	if (ret)
		return ret;

	params = kzalloc(sizeof(*params), GFP_KERNEL);
	if (!params)
		return -ENOMEM;

	if (!tz->tzp) {
		tz->tzp = kzalloc(sizeof(*tz->tzp), GFP_KERNEL);
		if (!tz->tzp) {
			ret = -ENOMEM;
			goto free_params;
		}

		params->allocated_tzp = true;
	}

	if (!tz->tzp->sustainable_power)
		dev_warn(&tz->device, "power_allocator: sustainable_power will be estimated\n");

	get_governor_trips(tz, params);

	if (tz->num_trips > 0) {
		ret = __thermal_zone_get_trip(tz, params->trip_max_desired_temperature,
					      &trip);
		if (!ret)
			estimate_pid_constants(tz, tz->tzp->sustainable_power,
					       params->trip_switch_on,
					       trip.temperature);
	}

	reset_pid_controller(params);

	tz->governor_data = params;

	return 0;

free_params:
	kfree(params);

	return ret;
}

static void power_allocator_unbind(struct thermal_zone_device *tz)
{
	struct power_allocator_params *params = tz->governor_data;

	dev_dbg(&tz->device, "Unbinding from thermal zone %d\n", tz->id);

	if (params->allocated_tzp) {
		kfree(tz->tzp);
		tz->tzp = NULL;
	}

	kfree(tz->governor_data);
	tz->governor_data = NULL;
}

static int power_allocator_throttle(struct thermal_zone_device *tz, int trip_id)
{
	struct power_allocator_params *params = tz->governor_data;
	struct thermal_trip trip;
	int ret;
	bool update;

	lockdep_assert_held(&tz->lock);

	 
	if (trip_id != params->trip_max_desired_temperature)
		return 0;

	ret = __thermal_zone_get_trip(tz, params->trip_switch_on, &trip);
	if (!ret && (tz->temperature < trip.temperature)) {
		update = (tz->last_temperature >= trip.temperature);
		tz->passive = 0;
		reset_pid_controller(params);
		allow_maximum_power(tz, update);
		return 0;
	}

	tz->passive = 1;

	ret = __thermal_zone_get_trip(tz, params->trip_max_desired_temperature, &trip);
	if (ret) {
		dev_warn(&tz->device, "Failed to get the maximum desired temperature: %d\n",
			 ret);
		return ret;
	}

	return allocate_power(tz, trip.temperature);
}

static struct thermal_governor thermal_gov_power_allocator = {
	.name		= "power_allocator",
	.bind_to_tz	= power_allocator_bind,
	.unbind_from_tz	= power_allocator_unbind,
	.throttle	= power_allocator_throttle,
};
THERMAL_GOVERNOR_DECLARE(thermal_gov_power_allocator);
