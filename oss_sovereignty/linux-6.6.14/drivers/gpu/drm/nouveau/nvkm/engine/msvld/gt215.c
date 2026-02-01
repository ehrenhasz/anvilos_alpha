 
#include "priv.h"

#include <nvif/class.h>

static const struct nvkm_falcon_func
gt215_msvld = {
	.init = g98_msvld_init,
	.sclass = {
		{ -1, -1, GT212_MSVLD },
		{}
	}
};

int
gt215_msvld_new(struct nvkm_device *device, enum nvkm_subdev_type type, int inst,
		struct nvkm_engine **pengine)
{
	return nvkm_msvld_new_(&gt215_msvld, device, type, inst, pengine);
}
