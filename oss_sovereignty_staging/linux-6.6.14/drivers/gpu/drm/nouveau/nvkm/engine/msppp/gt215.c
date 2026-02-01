 
#include "priv.h"

#include <nvif/class.h>

static const struct nvkm_falcon_func
gt215_msppp = {
	.init = g98_msppp_init,
	.sclass = {
		{ -1, -1, GT212_MSPPP },
		{}
	}
};

int
gt215_msppp_new(struct nvkm_device *device, enum nvkm_subdev_type type, int inst,
		struct nvkm_engine **pengine)
{
	return nvkm_msppp_new_(&gt215_msppp, device, type, inst, pengine);
}
