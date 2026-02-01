 
#include "priv.h"

static const struct nvkm_therm_func
gm200_therm = {
	.init = g84_therm_init,
	.fini = g84_therm_fini,
	.temp_get = g84_temp_get,
	.program_alarms = nvkm_therm_program_alarms_polling,
};

int
gm200_therm_new(struct nvkm_device *device, enum nvkm_subdev_type type, int inst,
		struct nvkm_therm **ptherm)
{
	return nvkm_therm_new_(&gm200_therm, device, type, inst, ptherm);
}
