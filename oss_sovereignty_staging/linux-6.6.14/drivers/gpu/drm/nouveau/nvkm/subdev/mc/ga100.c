 
#include "priv.h"

static void
ga100_mc_device_disable(struct nvkm_mc *mc, u32 mask)
{
	struct nvkm_device *device = mc->subdev.device;

	nvkm_mask(device, 0x000600, mask, 0x00000000);
	nvkm_rd32(device, 0x000600);
	nvkm_rd32(device, 0x000600);
}

static void
ga100_mc_device_enable(struct nvkm_mc *mc, u32 mask)
{
	struct nvkm_device *device = mc->subdev.device;

	nvkm_mask(device, 0x000600, mask, mask);
	nvkm_rd32(device, 0x000600);
	nvkm_rd32(device, 0x000600);
}

static bool
ga100_mc_device_enabled(struct nvkm_mc *mc, u32 mask)
{
	return (nvkm_rd32(mc->subdev.device, 0x000600) & mask) == mask;
}

static const struct nvkm_mc_device_func
ga100_mc_device = {
	.enabled = ga100_mc_device_enabled,
	.enable = ga100_mc_device_enable,
	.disable = ga100_mc_device_disable,
};

static void
ga100_mc_init(struct nvkm_mc *mc)
{
	struct nvkm_device *device = mc->subdev.device;

	nvkm_wr32(device, 0x000200, 0xffffffff);
	nvkm_wr32(device, 0x000600, 0xffffffff);
}

static const struct nvkm_mc_func
ga100_mc = {
	.init = ga100_mc_init,
	.device = &ga100_mc_device,
};

int
ga100_mc_new(struct nvkm_device *device, enum nvkm_subdev_type type, int inst, struct nvkm_mc **pmc)
{
	return nvkm_mc_new_(&ga100_mc, device, type, inst, pmc);
}
