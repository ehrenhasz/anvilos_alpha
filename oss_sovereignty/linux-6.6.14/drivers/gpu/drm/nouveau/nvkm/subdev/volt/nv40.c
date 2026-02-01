 
#include "priv.h"

static const struct nvkm_volt_func
nv40_volt = {
	.vid_get = nvkm_voltgpio_get,
	.vid_set = nvkm_voltgpio_set,
};

int
nv40_volt_new(struct nvkm_device *device, enum nvkm_subdev_type type, int inst,
	      struct nvkm_volt **pvolt)
{
	struct nvkm_volt *volt;
	int ret;

	ret = nvkm_volt_new_(&nv40_volt, device, type, inst, &volt);
	*pvolt = volt;
	if (ret)
		return ret;

	return nvkm_voltgpio_init(volt);
}
