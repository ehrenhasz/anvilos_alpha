 
#include "priv.h"

#include <nvif/class.h>

void
g98_mspdec_init(struct nvkm_falcon *mspdec)
{
	struct nvkm_device *device = mspdec->engine.subdev.device;
	nvkm_wr32(device, 0x085010, 0x0000ffd2);
	nvkm_wr32(device, 0x08501c, 0x0000fff2);
}

static const struct nvkm_falcon_func
g98_mspdec = {
	.init = g98_mspdec_init,
	.sclass = {
		{ -1, -1, G98_MSPDEC },
		{}
	}
};

int
g98_mspdec_new(struct nvkm_device *device, enum nvkm_subdev_type type, int inst,
	       struct nvkm_engine **pengine)
{
	return nvkm_mspdec_new_(&g98_mspdec, device, type, inst, pengine);
}
