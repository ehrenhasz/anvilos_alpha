
 

#include <linux/thermal.h>
#include <linux/minmax.h>
#include "thermal_trace.h"

#include "thermal_core.h"

 
static unsigned long get_target_state(struct thermal_instance *instance,
				enum thermal_trend trend, bool throttle)
{
	struct thermal_cooling_device *cdev = instance->cdev;
	unsigned long cur_state;
	unsigned long next_target;

	 
	cdev->ops->get_cur_state(cdev, &cur_state);
	next_target = instance->target;
	dev_dbg(&cdev->device, "cur_state=%ld\n", cur_state);

	if (!instance->initialized) {
		if (throttle) {
			next_target = clamp((cur_state + 1), instance->lower, instance->upper);
		} else {
			next_target = THERMAL_NO_TARGET;
		}

		return next_target;
	}

	if (throttle) {
		if (trend == THERMAL_TREND_RAISING)
			next_target = clamp((cur_state + 1), instance->lower, instance->upper);
	} else {
		if (trend == THERMAL_TREND_DROPPING) {
			if (cur_state <= instance->lower)
				next_target = THERMAL_NO_TARGET;
			else
				next_target = clamp((cur_state - 1), instance->lower, instance->upper);
		}
	}

	return next_target;
}

static void update_passive_instance(struct thermal_zone_device *tz,
				enum thermal_trip_type type, int value)
{
	 
	if (type == THERMAL_TRIP_PASSIVE)
		tz->passive += value;
}

static void thermal_zone_trip_update(struct thermal_zone_device *tz, int trip_id)
{
	enum thermal_trend trend;
	struct thermal_instance *instance;
	struct thermal_trip trip;
	bool throttle = false;
	int old_target;

	__thermal_zone_get_trip(tz, trip_id, &trip);

	trend = get_tz_trend(tz, trip_id);

	if (tz->temperature >= trip.temperature) {
		throttle = true;
		trace_thermal_zone_trip(tz, trip_id, trip.type);
	}

	dev_dbg(&tz->device, "Trip%d[type=%d,temp=%d]:trend=%d,throttle=%d\n",
				trip_id, trip.type, trip.temperature, trend, throttle);

	list_for_each_entry(instance, &tz->thermal_instances, tz_node) {
		if (instance->trip != trip_id)
			continue;

		old_target = instance->target;
		instance->target = get_target_state(instance, trend, throttle);
		dev_dbg(&instance->cdev->device, "old_target=%d, target=%d\n",
					old_target, (int)instance->target);

		if (instance->initialized && old_target == instance->target)
			continue;

		 
		if (old_target == THERMAL_NO_TARGET &&
			instance->target != THERMAL_NO_TARGET)
			update_passive_instance(tz, trip.type, 1);
		 
		else if (old_target != THERMAL_NO_TARGET &&
			instance->target == THERMAL_NO_TARGET)
			update_passive_instance(tz, trip.type, -1);

		instance->initialized = true;
		mutex_lock(&instance->cdev->lock);
		instance->cdev->updated = false;  
		mutex_unlock(&instance->cdev->lock);
	}
}

 
static int step_wise_throttle(struct thermal_zone_device *tz, int trip)
{
	struct thermal_instance *instance;

	lockdep_assert_held(&tz->lock);

	thermal_zone_trip_update(tz, trip);

	list_for_each_entry(instance, &tz->thermal_instances, tz_node)
		thermal_cdev_update(instance->cdev);

	return 0;
}

static struct thermal_governor thermal_gov_step_wise = {
	.name		= "step_wise",
	.throttle	= step_wise_throttle,
};
THERMAL_GOVERNOR_DECLARE(thermal_gov_step_wise);
