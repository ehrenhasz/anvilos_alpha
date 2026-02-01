 
#include "priv.h"

static const struct nvkm_timer_func
gk20a_timer = {
	.intr = nv04_timer_intr,
	.read = nv04_timer_read,
	.time = nv04_timer_time,
	.alarm_init = nv04_timer_alarm_init,
	.alarm_fini = nv04_timer_alarm_fini,
};

int
gk20a_timer_new(struct nvkm_device *device, enum nvkm_subdev_type type, int inst,
		struct nvkm_timer **ptmr)
{
	return nvkm_timer_new_(&gk20a_timer, device, type, inst, ptmr);
}
