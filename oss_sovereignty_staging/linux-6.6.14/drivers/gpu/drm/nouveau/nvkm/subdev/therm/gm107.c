 
#include "priv.h"

static int
gm107_fan_pwm_ctrl(struct nvkm_therm *therm, int line, bool enable)
{
	 
	return 0;
}

static int
gm107_fan_pwm_get(struct nvkm_therm *therm, int line, u32 *divs, u32 *duty)
{
	struct nvkm_device *device = therm->subdev.device;
	*divs = nvkm_rd32(device, 0x10eb20) & 0x1fff;
	*duty = nvkm_rd32(device, 0x10eb24) & 0x1fff;
	return 0;
}

static int
gm107_fan_pwm_set(struct nvkm_therm *therm, int line, u32 divs, u32 duty)
{
	struct nvkm_device *device = therm->subdev.device;
	nvkm_mask(device, 0x10eb10, 0x1fff, divs);  
	nvkm_wr32(device, 0x10eb14, duty | 0x80000000);
	return 0;
}

static int
gm107_fan_pwm_clock(struct nvkm_therm *therm, int line)
{
	return therm->subdev.device->crystal * 1000;
}

static const struct nvkm_therm_func
gm107_therm = {
	.init = gf119_therm_init,
	.fini = g84_therm_fini,
	.pwm_ctrl = gm107_fan_pwm_ctrl,
	.pwm_get = gm107_fan_pwm_get,
	.pwm_set = gm107_fan_pwm_set,
	.pwm_clock = gm107_fan_pwm_clock,
	.temp_get = g84_temp_get,
	.fan_sense = gt215_therm_fan_sense,
	.program_alarms = nvkm_therm_program_alarms_polling,
};

int
gm107_therm_new(struct nvkm_device *device, enum nvkm_subdev_type type, int inst,
		struct nvkm_therm **ptherm)
{
	return nvkm_therm_new_(&gm107_therm, device, type, inst, ptherm);
}
