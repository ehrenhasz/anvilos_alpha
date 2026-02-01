 
 

#ifndef ADF_GEN4_TIMER_H_
#define ADF_GEN4_TIMER_H_

#include <linux/ktime.h>
#include <linux/workqueue.h>

struct adf_accel_dev;

struct adf_timer {
	struct adf_accel_dev *accel_dev;
	struct delayed_work work_ctx;
	ktime_t initial_ktime;
};

int adf_gen4_timer_start(struct adf_accel_dev *accel_dev);
void adf_gen4_timer_stop(struct adf_accel_dev *accel_dev);

#endif  
