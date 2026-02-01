 
#include "priv.h"
#include "regsnv04.h"

static void
nv41_timer_init(struct nvkm_timer *tmr)
{
	struct nvkm_subdev *subdev = &tmr->subdev;
	struct nvkm_device *device = subdev->device;
	u32 f = device->crystal;
	u32 m = 1, n, d;

	 
	d = 1000000 / 32;
	n = f;

	while (n < (d * 2)) {
		n += (n / m);
		m++;
	}

	 
	while (((n % 5) == 0) && ((d % 5) == 0)) {
		n /= 5;
		d /= 5;
	}

	while (((n % 2) == 0) && ((d % 2) == 0)) {
		n /= 2;
		d /= 2;
	}

	while (n > 0xffff || d > 0xffff) {
		n >>= 1;
		d >>= 1;
	}

	nvkm_debug(subdev, "input frequency : %dHz\n", f);
	nvkm_debug(subdev, "input multiplier: %d\n", m);
	nvkm_debug(subdev, "numerator       : %08x\n", n);
	nvkm_debug(subdev, "denominator     : %08x\n", d);
	nvkm_debug(subdev, "timer frequency : %dHz\n", (f * m) * d / n);

	nvkm_wr32(device, 0x009220, m - 1);
	nvkm_wr32(device, NV04_PTIMER_NUMERATOR, n);
	nvkm_wr32(device, NV04_PTIMER_DENOMINATOR, d);
}

static const struct nvkm_timer_func
nv41_timer = {
	.init = nv41_timer_init,
	.intr = nv04_timer_intr,
	.read = nv04_timer_read,
	.time = nv04_timer_time,
	.alarm_init = nv04_timer_alarm_init,
	.alarm_fini = nv04_timer_alarm_fini,
};

int
nv41_timer_new(struct nvkm_device *device, enum nvkm_subdev_type type, int inst,
	       struct nvkm_timer **ptmr)
{
	return nvkm_timer_new_(&nv41_timer, device, type, inst, ptmr);
}
