 
#include "priv.h"

#include <nvif/class.h>

static const struct nvkm_engine_func
gm200_ce = {
	.intr = gk104_ce_intr,
	.sclass = {
		{ -1, -1, MAXWELL_DMA_COPY_A },
		{}
	}
};

int
gm200_ce_new(struct nvkm_device *device, enum nvkm_subdev_type type, int inst,
	     struct nvkm_engine **pengine)
{
	return nvkm_engine_new_(&gm200_ce, device, type, inst, true, pengine);
}
