 
#include "priv.h"

#include <nvif/class.h>

static void
gf100_msppp_init(struct nvkm_falcon *msppp)
{
	struct nvkm_device *device = msppp->engine.subdev.device;
	nvkm_wr32(device, 0x086010, 0x0000fff2);
	nvkm_wr32(device, 0x08601c, 0x0000fff2);
}

static const struct nvkm_falcon_func
gf100_msppp = {
	.init = gf100_msppp_init,
	.sclass = {
		{ -1, -1, GF100_MSPPP },
		{}
	}
};

int
gf100_msppp_new(struct nvkm_device *device, enum nvkm_subdev_type type, int inst,
		struct nvkm_engine **pengine)
{
	return nvkm_msppp_new_(&gf100_msppp, device, type, inst, pengine);
}
