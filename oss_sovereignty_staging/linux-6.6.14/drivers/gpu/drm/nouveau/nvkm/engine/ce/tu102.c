 
#include "priv.h"

#include <nvif/class.h>

static const struct nvkm_engine_func
tu102_ce = {
	.intr = gp100_ce_intr,
	.cclass = &gv100_ce_cclass,
	.sclass = {
		{ -1, -1, TURING_DMA_COPY_A },
		{}
	}
};

int
tu102_ce_new(struct nvkm_device *device, enum nvkm_subdev_type type, int inst,
	     struct nvkm_engine **pengine)
{
	return nvkm_engine_new_(&tu102_ce, device, type, inst, true, pengine);
}
