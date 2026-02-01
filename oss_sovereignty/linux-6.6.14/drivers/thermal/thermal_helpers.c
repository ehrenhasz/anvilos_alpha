
 

#define pr_fmt(fmt) KBUILD_MODNAME ": " fmt

#include <linux/device.h>
#include <linux/err.h>
#include <linux/export.h>
#include <linux/slab.h>
#include <linux/string.h>
#include <linux/sysfs.h>

#include "thermal_core.h"
#include "thermal_trace.h"

int get_tz_trend(struct thermal_zone_device *tz, int trip_index)
{
	struct thermal_trip *trip = tz->trips ? &tz->trips[trip_index] : NULL;
	enum thermal_trend trend;

	if (tz->emul_temperature || !tz->ops->get_trend ||
	    tz->ops->get_trend(tz, trip, &trend)) {
		if (tz->temperature > tz->last_temperature)
			trend = THERMAL_TREND_RAISING;
		else if (tz->temperature < tz->last_temperature)
			trend = THERMAL_TREND_DROPPING;
		else
			trend = THERMAL_TREND_STABLE;
	}

	return trend;
}

struct thermal_instance *
get_thermal_instance(struct thermal_zone_device *tz,
		     struct thermal_cooling_device *cdev, int trip)
{
	struct thermal_instance *pos = NULL;
	struct thermal_instance *target_instance = NULL;

	mutex_lock(&tz->lock);
	mutex_lock(&cdev->lock);

	list_for_each_entry(pos, &tz->thermal_instances, tz_node) {
		if (pos->tz == tz && pos->trip == trip && pos->cdev == cdev) {
			target_instance = pos;
			break;
		}
	}

	mutex_unlock(&cdev->lock);
	mutex_unlock(&tz->lock);

	return target_instance;
}
EXPORT_SYMBOL(get_thermal_instance);

 
int __thermal_zone_get_temp(struct thermal_zone_device *tz, int *temp)
{
	int ret = -EINVAL;
	int count;
	int crit_temp = INT_MAX;
	struct thermal_trip trip;

	lockdep_assert_held(&tz->lock);

	ret = tz->ops->get_temp(tz, temp);

	if (IS_ENABLED(CONFIG_THERMAL_EMULATION) && tz->emul_temperature) {
		for (count = 0; count < tz->num_trips; count++) {
			ret = __thermal_zone_get_trip(tz, count, &trip);
			if (!ret && trip.type == THERMAL_TRIP_CRITICAL) {
				crit_temp = trip.temperature;
				break;
			}
		}

		 
		if (!ret && *temp < crit_temp)
			*temp = tz->emul_temperature;
	}

	if (ret)
		dev_dbg(&tz->device, "Failed to get temperature: %d\n", ret);

	return ret;
}

 
int thermal_zone_get_temp(struct thermal_zone_device *tz, int *temp)
{
	int ret;

	if (IS_ERR_OR_NULL(tz))
		return -EINVAL;

	mutex_lock(&tz->lock);

	if (!tz->ops->get_temp) {
		ret = -EINVAL;
		goto unlock;
	}

	if (device_is_registered(&tz->device))
		ret = __thermal_zone_get_temp(tz, temp);
	else
		ret = -ENODEV;

unlock:
	mutex_unlock(&tz->lock);

	return ret;
}
EXPORT_SYMBOL_GPL(thermal_zone_get_temp);

static void thermal_cdev_set_cur_state(struct thermal_cooling_device *cdev,
				       int target)
{
	if (cdev->ops->set_cur_state(cdev, target))
		return;

	thermal_notify_cdev_state_update(cdev->id, target);
	thermal_cooling_device_stats_update(cdev, target);
}

void __thermal_cdev_update(struct thermal_cooling_device *cdev)
{
	struct thermal_instance *instance;
	unsigned long target = 0;

	 
	list_for_each_entry(instance, &cdev->thermal_instances, cdev_node) {
		dev_dbg(&cdev->device, "zone%d->target=%lu\n",
			instance->tz->id, instance->target);
		if (instance->target == THERMAL_NO_TARGET)
			continue;
		if (instance->target > target)
			target = instance->target;
	}

	thermal_cdev_set_cur_state(cdev, target);

	trace_cdev_update(cdev, target);
	dev_dbg(&cdev->device, "set to state %lu\n", target);
}

 
void thermal_cdev_update(struct thermal_cooling_device *cdev)
{
	mutex_lock(&cdev->lock);
	if (!cdev->updated) {
		__thermal_cdev_update(cdev);
		cdev->updated = true;
	}
	mutex_unlock(&cdev->lock);
}

 
int thermal_zone_get_slope(struct thermal_zone_device *tz)
{
	if (tz && tz->tzp)
		return tz->tzp->slope;
	return 1;
}
EXPORT_SYMBOL_GPL(thermal_zone_get_slope);

 
int thermal_zone_get_offset(struct thermal_zone_device *tz)
{
	if (tz && tz->tzp)
		return tz->tzp->offset;
	return 0;
}
EXPORT_SYMBOL_GPL(thermal_zone_get_offset);
