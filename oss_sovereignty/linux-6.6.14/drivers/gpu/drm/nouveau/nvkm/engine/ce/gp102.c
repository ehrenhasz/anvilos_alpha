 
#include "priv.h"
#include <core/enum.h>

#include <nvif/class.h>

static const struct nvkm_engine_func
gp102_ce = {
	.intr = gp100_ce_intr,
	.sclass = {
		{ -1, -1, PASCAL_DMA_COPY_B },
		{ -1, -1, PASCAL_DMA_COPY_A },
		{}
	}
};

int
gp102_ce_new(struct nvkm_device *device, enum nvkm_subdev_type type, int inst,
	     struct nvkm_engine **pengine)
{
	return nvkm_engine_new_(&gp102_ce, device, type, inst, true, pengine);
}
