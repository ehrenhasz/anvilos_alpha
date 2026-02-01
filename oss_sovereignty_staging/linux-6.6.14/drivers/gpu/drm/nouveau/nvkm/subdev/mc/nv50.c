 
#include "priv.h"

static const struct nvkm_intr_data
nv50_mc_intrs[] = {
	{ NVKM_ENGINE_DISP , 0, 0, 0x04000000, true },
	{ NVKM_ENGINE_GR   , 0, 0, 0x00001000, true },
	{ NVKM_ENGINE_FIFO , 0, 0, 0x00000100 },
	{ NVKM_ENGINE_MPEG , 0, 0, 0x00000001, true },
	{ NVKM_SUBDEV_FB   , 0, 0, 0x00001101, true },
	{ NVKM_SUBDEV_BUS  , 0, 0, 0x10000000, true },
	{ NVKM_SUBDEV_GPIO , 0, 0, 0x00200000, true },
	{ NVKM_SUBDEV_I2C  , 0, 0, 0x00200000, true },
	{ NVKM_SUBDEV_TIMER, 0, 0, 0x00100000, true },
	{},
};

void
nv50_mc_init(struct nvkm_mc *mc)
{
	struct nvkm_device *device = mc->subdev.device;
	nvkm_wr32(device, 0x000200, 0xffffffff);  
}

static const struct nvkm_mc_func
nv50_mc = {
	.init = nv50_mc_init,
	.intr = &nv04_mc_intr,
	.intrs = nv50_mc_intrs,
	.device = &nv04_mc_device,
	.reset = nv17_mc_reset,
};

int
nv50_mc_new(struct nvkm_device *device, enum nvkm_subdev_type type, int inst, struct nvkm_mc **pmc)
{
	return nvkm_mc_new_(&nv50_mc, device, type, inst, pmc);
}
