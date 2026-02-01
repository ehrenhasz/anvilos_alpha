 
#include "priv.h"

static const struct nvkm_intr_data
nv11_mc_intrs[] = {
	{ NVKM_ENGINE_DISP , 0, 0, 0x03010000, true },
	{ NVKM_ENGINE_GR   , 0, 0, 0x00001000, true },
	{ NVKM_ENGINE_FIFO , 0, 0, 0x00000100 },
	{ NVKM_SUBDEV_BUS  , 0, 0, 0x10000000, true },
	{ NVKM_SUBDEV_TIMER, 0, 0, 0x00100000, true },
	{}
};

static const struct nvkm_mc_func
nv11_mc = {
	.init = nv04_mc_init,
	.intr = &nv04_mc_intr,
	.intrs = nv11_mc_intrs,
	.device = &nv04_mc_device,
	.reset = nv04_mc_reset,
};

int
nv11_mc_new(struct nvkm_device *device, enum nvkm_subdev_type type, int inst, struct nvkm_mc **pmc)
{
	return nvkm_mc_new_(&nv11_mc, device, type, inst, pmc);
}
