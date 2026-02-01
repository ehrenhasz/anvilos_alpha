 
#include <core/device.h>

#include "priv.h"
#include "gk104.h"

void
gk104_clkgate_enable(struct nvkm_therm *base)
{
	struct gk104_therm *therm = gk104_therm(base);
	struct nvkm_device *dev = therm->base.subdev.device;
	const struct gk104_clkgate_engine_info *order = therm->clkgate_order;
	int i;

	 
	for (i = 0; order[i].type != NVKM_SUBDEV_NR; i++) {
		if (!nvkm_device_subdev(dev, order[i].type, order[i].inst))
			continue;

		nvkm_mask(dev, 0x20200 + order[i].offset, 0xff00, 0x4500);
	}

	 
	nvkm_wr32(dev, 0x020288, therm->idle_filter->fecs);
	nvkm_wr32(dev, 0x02028c, therm->idle_filter->hubmmu);

	 
	for (i = 0; order[i].type != NVKM_SUBDEV_NR; i++) {
		if (!nvkm_device_subdev(dev, order[i].type, order[i].inst))
			continue;

		nvkm_mask(dev, 0x20200 + order[i].offset, 0x00ff, 0x0045);
	}
}

void
gk104_clkgate_fini(struct nvkm_therm *base, bool suspend)
{
	struct gk104_therm *therm = gk104_therm(base);
	struct nvkm_device *dev = therm->base.subdev.device;
	const struct gk104_clkgate_engine_info *order = therm->clkgate_order;
	int i;

	 
	for (i = 0; order[i].type != NVKM_SUBDEV_NR; i++) {
		if (!nvkm_device_subdev(dev, order[i].type, order[i].inst))
			continue;

		nvkm_mask(dev, 0x20200 + order[i].offset, 0xff, 0x54);
	}
}

const struct gk104_clkgate_engine_info gk104_clkgate_engine_info[] = {
	{ NVKM_ENGINE_GR,     0, 0x00 },
	{ NVKM_ENGINE_MSPDEC, 0, 0x04 },
	{ NVKM_ENGINE_MSPPP,  0, 0x08 },
	{ NVKM_ENGINE_MSVLD,  0, 0x0c },
	{ NVKM_ENGINE_CE,     0, 0x10 },
	{ NVKM_ENGINE_CE,     1, 0x14 },
	{ NVKM_ENGINE_MSENC,  0, 0x18 },
	{ NVKM_ENGINE_CE,     2, 0x1c },
	{ NVKM_SUBDEV_NR },
};

const struct gf100_idle_filter gk104_idle_filter = {
	.fecs = 0x00001000,
	.hubmmu = 0x00001000,
};

static const struct nvkm_therm_func
gk104_therm_func = {
	.init = gf119_therm_init,
	.fini = g84_therm_fini,
	.pwm_ctrl = gf119_fan_pwm_ctrl,
	.pwm_get = gf119_fan_pwm_get,
	.pwm_set = gf119_fan_pwm_set,
	.pwm_clock = gf119_fan_pwm_clock,
	.temp_get = g84_temp_get,
	.fan_sense = gt215_therm_fan_sense,
	.program_alarms = nvkm_therm_program_alarms_polling,
	.clkgate_init = gf100_clkgate_init,
	.clkgate_enable = gk104_clkgate_enable,
	.clkgate_fini = gk104_clkgate_fini,
};

static int
gk104_therm_new_(const struct nvkm_therm_func *func, struct nvkm_device *device,
		 enum nvkm_subdev_type type, int inst,
		 const struct gk104_clkgate_engine_info *clkgate_order,
		 const struct gf100_idle_filter *idle_filter,
		 struct nvkm_therm **ptherm)
{
	struct gk104_therm *therm = kzalloc(sizeof(*therm), GFP_KERNEL);

	if (!therm)
		return -ENOMEM;

	nvkm_therm_ctor(&therm->base, device, type, inst, func);
	*ptherm = &therm->base;
	therm->clkgate_order = clkgate_order;
	therm->idle_filter = idle_filter;
	return 0;
}

int
gk104_therm_new(struct nvkm_device *device, enum nvkm_subdev_type type, int inst, struct nvkm_therm **ptherm)
{
	return gk104_therm_new_(&gk104_therm_func, device, type, inst,
				gk104_clkgate_engine_info, &gk104_idle_filter,
				ptherm);
}
