 
#include "priv.h"

#include <nvif/class.h>

void
gf100_msvld_init(struct nvkm_falcon *msvld)
{
	struct nvkm_device *device = msvld->engine.subdev.device;
	nvkm_wr32(device, 0x084010, 0x0000fff2);
	nvkm_wr32(device, 0x08401c, 0x0000fff2);
}

static const struct nvkm_falcon_func
gf100_msvld = {
	.init = gf100_msvld_init,
	.sclass = {
		{ -1, -1, GF100_MSVLD },
		{}
	}
};

int
gf100_msvld_new(struct nvkm_device *device, enum nvkm_subdev_type type, int inst,
		struct nvkm_engine **pengine)
{
	return nvkm_msvld_new_(&gf100_msvld, device, type, inst, pengine);
}
