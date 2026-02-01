 
 

#ifndef __LIMA_DEVFREQ_H__
#define __LIMA_DEVFREQ_H__

#include <linux/devfreq.h>
#include <linux/spinlock.h>
#include <linux/ktime.h>

struct devfreq;
struct thermal_cooling_device;

struct lima_device;

struct lima_devfreq {
	struct devfreq *devfreq;
	struct thermal_cooling_device *cooling;
	struct devfreq_simple_ondemand_data gov_data;

	ktime_t busy_time;
	ktime_t idle_time;
	ktime_t time_last_update;
	int busy_count;
	 
	spinlock_t lock;
};

int lima_devfreq_init(struct lima_device *ldev);
void lima_devfreq_fini(struct lima_device *ldev);

void lima_devfreq_record_busy(struct lima_devfreq *devfreq);
void lima_devfreq_record_idle(struct lima_devfreq *devfreq);

int lima_devfreq_resume(struct lima_devfreq *devfreq);
int lima_devfreq_suspend(struct lima_devfreq *devfreq);

#endif
