 
#include "priv.h"

static const struct nvkm_mc_map
gf100_mc_reset[] = {
	{ 0x00020000, NVKM_ENGINE_MSPDEC },
	{ 0x00008000, NVKM_ENGINE_MSVLD },
	{ 0x00002000, NVKM_SUBDEV_PMU, 0, true },
	{ 0x00001000, NVKM_ENGINE_GR },
	{ 0x00000100, NVKM_ENGINE_FIFO },
	{ 0x00000080, NVKM_ENGINE_CE, 1 },
	{ 0x00000040, NVKM_ENGINE_CE, 0 },
	{ 0x00000002, NVKM_ENGINE_MSPPP },
	{}
};

static const struct nvkm_intr_data
gf100_mc_intrs[] = {
	{ NVKM_ENGINE_DISP    , 0, 0, 0x04000000, true },
	{ NVKM_ENGINE_MSPDEC  , 0, 0, 0x00020000, true },
	{ NVKM_ENGINE_MSVLD   , 0, 0, 0x00008000, true },
	{ NVKM_ENGINE_GR      , 0, 0, 0x00001000 },
	{ NVKM_ENGINE_FIFO    , 0, 0, 0x00000100 },
	{ NVKM_ENGINE_CE      , 1, 0, 0x00000040, true },
	{ NVKM_ENGINE_CE      , 0, 0, 0x00000020, true },
	{ NVKM_ENGINE_MSPPP   , 0, 0, 0x00000001, true },
	{ NVKM_SUBDEV_PRIVRING, 0, 0, 0x40000000, true },
	{ NVKM_SUBDEV_BUS     , 0, 0, 0x10000000, true },
	{ NVKM_SUBDEV_FB      , 0, 0, 0x08002000, true },
	{ NVKM_SUBDEV_LTC     , 0, 0, 0x02000000, true },
	{ NVKM_SUBDEV_PMU     , 0, 0, 0x01000000, true },
	{ NVKM_SUBDEV_GPIO    , 0, 0, 0x00200000, true },
	{ NVKM_SUBDEV_I2C     , 0, 0, 0x00200000, true },
	{ NVKM_SUBDEV_TIMER   , 0, 0, 0x00100000, true },
	{ NVKM_SUBDEV_THERM   , 0, 0, 0x00040000, true },
	{},
};

void
gf100_mc_unk260(struct nvkm_mc *mc, u32 data)
{
	nvkm_wr32(mc->subdev.device, 0x000260, data);
}

static const struct nvkm_mc_func
gf100_mc = {
	.init = nv50_mc_init,
	.intr = &gt215_mc_intr,
	.intrs = gf100_mc_intrs,
	.intr_nonstall = true,
	.reset = gf100_mc_reset,
	.device = &nv04_mc_device,
	.unk260 = gf100_mc_unk260,
};

int
gf100_mc_new(struct nvkm_device *device, enum nvkm_subdev_type type, int inst, struct nvkm_mc **pmc)
{
	return nvkm_mc_new_(&gf100_mc, device, type, inst, pmc);
}
