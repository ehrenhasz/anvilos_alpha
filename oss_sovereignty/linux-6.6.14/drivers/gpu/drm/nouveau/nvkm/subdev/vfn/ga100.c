 
#include "priv.h"

#include <nvif/class.h>

static const struct nvkm_intr_data
ga100_vfn_intrs[] = {
	{ NVKM_ENGINE_DISP    , 0, 4, 0x04000000, true },
	{ NVKM_SUBDEV_GPIO    , 0, 4, 0x00200000, true },
	{ NVKM_SUBDEV_I2C     , 0, 4, 0x00200000, true },
	{ NVKM_SUBDEV_PRIVRING, 0, 4, 0x40000000, true },
	{}
};

static const struct nvkm_vfn_func
ga100_vfn = {
	.intr = &tu102_vfn_intr,
	.intrs = ga100_vfn_intrs,
	.user = { 0x030000, 0x010000, { -1, -1, AMPERE_USERMODE_A } },
};

int
ga100_vfn_new(struct nvkm_device *device,
	      enum nvkm_subdev_type type, int inst, struct nvkm_vfn **pvfn)
{
	return nvkm_vfn_new_(&ga100_vfn, device, type, inst, 0xb80000, pvfn);
}
