 
#include "priv.h"

#include <nvif/class.h>

static const struct nvkm_engine_func
gm107_ce = {
	.intr = gk104_ce_intr,
	.sclass = {
		{ -1, -1, KEPLER_DMA_COPY_A },
		{ -1, -1, MAXWELL_DMA_COPY_A },
		{}
	}
};

int
gm107_ce_new(struct nvkm_device *device, enum nvkm_subdev_type type, int inst,
	     struct nvkm_engine **pengine)
{
	return nvkm_engine_new_(&gm107_ce, device, type, inst, true, pengine);
}
