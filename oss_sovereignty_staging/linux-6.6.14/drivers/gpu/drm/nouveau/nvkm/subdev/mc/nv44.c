 
#include "priv.h"

void
nv44_mc_init(struct nvkm_mc *mc)
{
	struct nvkm_device *device = mc->subdev.device;
	u32 tmp = nvkm_rd32(device, 0x10020c);

	nvkm_wr32(device, 0x000200, 0xffffffff);  

	nvkm_wr32(device, 0x001700, tmp);
	nvkm_wr32(device, 0x001704, 0);
	nvkm_wr32(device, 0x001708, 0);
	nvkm_wr32(device, 0x00170c, tmp);
}

static const struct nvkm_mc_func
nv44_mc = {
	.init = nv44_mc_init,
	.intr = &nv04_mc_intr,
	.intrs = nv17_mc_intrs,
	.device = &nv04_mc_device,
	.reset = nv17_mc_reset,
};

int
nv44_mc_new(struct nvkm_device *device, enum nvkm_subdev_type type, int inst, struct nvkm_mc **pmc)
{
	return nvkm_mc_new_(&nv44_mc, device, type, inst, pmc);
}
