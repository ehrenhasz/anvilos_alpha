


#include <linux/mutex.h>
#include <linux/types.h>

#ifndef __PECI_HWMON_COMMON_H
#define __PECI_HWMON_COMMON_H

#define PECI_HWMON_UPDATE_INTERVAL	HZ


struct peci_sensor_state {
	bool valid;
	unsigned long last_updated;
	struct mutex lock; 
};



struct peci_sensor_data {
	s32 value;
	struct peci_sensor_state state;
};



static inline bool peci_sensor_need_update(struct peci_sensor_state *state)
{
	return !state->valid ||
	       time_after(jiffies, state->last_updated + PECI_HWMON_UPDATE_INTERVAL);
}


static inline void peci_sensor_mark_updated(struct peci_sensor_state *state)
{
	state->valid = true;
	state->last_updated = jiffies;
}

#endif 
