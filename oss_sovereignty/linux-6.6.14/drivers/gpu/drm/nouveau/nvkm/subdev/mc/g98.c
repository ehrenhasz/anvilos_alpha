 
#include "priv.h"

static const struct nvkm_mc_map
g98_mc_reset[] = {
	{ 0x04008000, NVKM_ENGINE_MSVLD },
	{ 0x02004000, NVKM_ENGINE_SEC },
	{ 0x01020000, NVKM_ENGINE_MSPDEC },
	{ 0x00400002, NVKM_ENGINE_MSPPP },
	{ 0x00201000, NVKM_ENGINE_GR },
	{ 0x00000100, NVKM_ENGINE_FIFO },
	{}
};

static const struct nvkm_intr_data
g98_mc_intrs[] = {
	{ NVKM_ENGINE_DISP  , 0, 0, 0x04000000, true },
	{ NVKM_ENGINE_MSPDEC, 0, 0, 0x00020000, true },
	{ NVKM_ENGINE_MSVLD , 0, 0, 0x00008000, true },
	{ NVKM_ENGINE_SEC   , 0, 0, 0x00004000, true },
	{ NVKM_ENGINE_GR    , 0, 0, 0x00001000, true },
	{ NVKM_ENGINE_FIFO  , 0, 0, 0x00000100 },
	{ NVKM_ENGINE_MSPPP , 0, 0, 0x00000001, true },
	{ NVKM_SUBDEV_FB    , 0, 0, 0x0002d101, true },
	{ NVKM_SUBDEV_BUS   , 0, 0, 0x10000000, true },
	{ NVKM_SUBDEV_GPIO  , 0, 0, 0x00200000, true },
	{ NVKM_SUBDEV_I2C   , 0, 0, 0x00200000, true },
	{ NVKM_SUBDEV_TIMER , 0, 0, 0x00100000, true },
	{},
};

static const struct nvkm_mc_func
g98_mc = {
	.init = nv50_mc_init,
	.intr = &nv04_mc_intr,
	.intrs = g98_mc_intrs,
	.device = &nv04_mc_device,
	.reset = g98_mc_reset,
};

int
g98_mc_new(struct nvkm_device *device, enum nvkm_subdev_type type, int inst, struct nvkm_mc **pmc)
{
	return nvkm_mc_new_(&g98_mc, device, type, inst, pmc);
}
