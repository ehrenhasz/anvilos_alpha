 
#include "priv.h"

static const struct nvkm_mc_func
gk20a_mc = {
	.init = nv50_mc_init,
	.intr = &gt215_mc_intr,
	.intrs = gk104_mc_intrs,
	.intr_nonstall = true,
	.device = &nv04_mc_device,
	.reset = gk104_mc_reset,
};

int
gk20a_mc_new(struct nvkm_device *device, enum nvkm_subdev_type type, int inst, struct nvkm_mc **pmc)
{
	return nvkm_mc_new_(&gk20a_mc, device, type, inst, pmc);
}
