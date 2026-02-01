 
 
#ifndef __THERMAL_HWMON_H__
#define __THERMAL_HWMON_H__

#include <linux/thermal.h>

#ifdef CONFIG_THERMAL_HWMON
int thermal_add_hwmon_sysfs(struct thermal_zone_device *tz);
int devm_thermal_add_hwmon_sysfs(struct device *dev, struct thermal_zone_device *tz);
void thermal_remove_hwmon_sysfs(struct thermal_zone_device *tz);
#else
static inline int
thermal_add_hwmon_sysfs(struct thermal_zone_device *tz)
{
	return 0;
}

static inline int
devm_thermal_add_hwmon_sysfs(struct device *dev, struct thermal_zone_device *tz)
{
	return 0;
}

static inline void
thermal_remove_hwmon_sysfs(struct thermal_zone_device *tz)
{
}
#endif

#endif  
