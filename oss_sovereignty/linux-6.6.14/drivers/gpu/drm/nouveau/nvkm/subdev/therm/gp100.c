 
#include "priv.h"

static int
gp100_temp_get(struct nvkm_therm *therm)
{
	struct nvkm_device *device = therm->subdev.device;
	struct nvkm_subdev *subdev = &therm->subdev;
	u32 tsensor = nvkm_rd32(device, 0x020460);
	u32 inttemp = (tsensor & 0x0001fff8);

	 
	if (tsensor & 0x40000000)
		nvkm_trace(subdev, "reading temperature from SHADOWed sensor\n");

	 
	if (tsensor & 0x20000000)
		return (inttemp >> 8);
	else
		return -ENODEV;
}

static const struct nvkm_therm_func
gp100_therm = {
	.temp_get = gp100_temp_get,
	.program_alarms = nvkm_therm_program_alarms_polling,
};

int
gp100_therm_new(struct nvkm_device *device, enum nvkm_subdev_type type, int inst,
		struct nvkm_therm **ptherm)
{
	return nvkm_therm_new_(&gp100_therm, device, type, inst, ptherm);
}
