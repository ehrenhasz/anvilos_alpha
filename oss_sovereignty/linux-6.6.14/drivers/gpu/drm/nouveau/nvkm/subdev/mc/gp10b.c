 

#include "priv.h"

static void
gp10b_mc_init(struct nvkm_mc *mc)
{
	struct nvkm_device *device = mc->subdev.device;
	nvkm_wr32(device, 0x000200, 0xffffffff);  
	nvkm_wr32(device, 0x00020c, 0xffffffff);  
}

static const struct nvkm_mc_func
gp10b_mc = {
	.init = gp10b_mc_init,
	.intr = &gp100_mc_intr,
	.intrs = gp100_mc_intrs,
	.intr_nonstall = true,
	.device = &nv04_mc_device,
	.reset = gk104_mc_reset,
};

int
gp10b_mc_new(struct nvkm_device *device, enum nvkm_subdev_type type, int inst, struct nvkm_mc **pmc)
{
	return nvkm_mc_new_(&gp10b_mc, device, type, inst, pmc);
}
