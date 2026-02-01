 
#include <nvif/timer.h>
#include <nvif/device.h>

s64
nvif_timer_wait_test(struct nvif_timer_wait *wait)
{
	u64 time = nvif_device_time(wait->device);

	if (wait->reads == 0) {
		wait->time0 = time;
		wait->time1 = time;
	}

	if (wait->time1 == time) {
		if (WARN_ON(wait->reads++ == 16))
			return -ETIMEDOUT;
	} else {
		wait->time1 = time;
		wait->reads = 1;
	}

	if (wait->time1 - wait->time0 > wait->limit)
		return -ETIMEDOUT;

	return wait->time1 - wait->time0;
}

void
nvif_timer_wait_init(struct nvif_device *device, u64 nsec,
		     struct nvif_timer_wait *wait)
{
	wait->device = device;
	wait->limit = nsec;
	wait->reads = 0;
}
