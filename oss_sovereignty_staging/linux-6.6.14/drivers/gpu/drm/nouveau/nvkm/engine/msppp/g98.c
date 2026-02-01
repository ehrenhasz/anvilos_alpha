 
#include "priv.h"

#include <nvif/class.h>

void
g98_msppp_init(struct nvkm_falcon *msppp)
{
	struct nvkm_device *device = msppp->engine.subdev.device;
	nvkm_wr32(device, 0x086010, 0x0000ffd2);
	nvkm_wr32(device, 0x08601c, 0x0000fff2);
}

static const struct nvkm_falcon_func
g98_msppp = {
	.init = g98_msppp_init,
	.sclass = {
		{ -1, -1, G98_MSPPP },
		{}
	}
};

int
g98_msppp_new(struct nvkm_device *device, enum nvkm_subdev_type type, int inst,
	      struct nvkm_engine **pengine)
{
	return nvkm_msppp_new_(&g98_msppp, device, type, inst, pengine);
}
