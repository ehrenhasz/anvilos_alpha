 
#include "priv.h"

const struct nvkm_mc_map
gk104_mc_reset[] = {
	{ 0x00000100, NVKM_ENGINE_FIFO },
	{ 0x00002000, NVKM_SUBDEV_PMU, 0, true },
	{}
};

const struct nvkm_intr_data
gk104_mc_intrs[] = {
	{ NVKM_ENGINE_DISP    , 0, 0, 0x04000000, true },
	{ NVKM_ENGINE_FIFO    , 0, 0, 0x00000100 },
	{ NVKM_SUBDEV_PRIVRING, 0, 0, 0x40000000, true },
	{ NVKM_SUBDEV_BUS     , 0, 0, 0x10000000, true },
	{ NVKM_SUBDEV_FB      , 0, 0, 0x08002000, true },
	{ NVKM_SUBDEV_LTC     , 0, 0, 0x02000000, true },
	{ NVKM_SUBDEV_PMU     , 0, 0, 0x01000000, true },
	{ NVKM_SUBDEV_GPIO    , 0, 0, 0x00200000, true },
	{ NVKM_SUBDEV_I2C     , 0, 0, 0x00200000, true },
	{ NVKM_SUBDEV_TIMER   , 0, 0, 0x00100000, true },
	{ NVKM_SUBDEV_THERM   , 0, 0, 0x00040000, true },
	{ NVKM_SUBDEV_TOP     , 0, 0, 0x00001000 },
	{ NVKM_SUBDEV_TOP     , 0, 0, 0xffffefff, true },
	{},
};

static const struct nvkm_mc_func
gk104_mc = {
	.init = nv50_mc_init,
	.intr = &gt215_mc_intr,
	.intrs = gk104_mc_intrs,
	.intr_nonstall = true,
	.reset = gk104_mc_reset,
	.device = &nv04_mc_device,
	.unk260 = gf100_mc_unk260,
};

int
gk104_mc_new(struct nvkm_device *device, enum nvkm_subdev_type type, int inst, struct nvkm_mc **pmc)
{
	return nvkm_mc_new_(&gk104_mc, device, type, inst, pmc);
}
