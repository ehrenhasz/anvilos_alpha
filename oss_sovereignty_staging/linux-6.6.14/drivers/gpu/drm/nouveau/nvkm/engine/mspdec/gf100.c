 
#include "priv.h"

#include <nvif/class.h>

void
gf100_mspdec_init(struct nvkm_falcon *mspdec)
{
	struct nvkm_device *device = mspdec->engine.subdev.device;
	nvkm_wr32(device, 0x085010, 0x0000fff2);
	nvkm_wr32(device, 0x08501c, 0x0000fff2);
}

static const struct nvkm_falcon_func
gf100_mspdec = {
	.init = gf100_mspdec_init,
	.sclass = {
		{ -1, -1, GF100_MSPDEC },
		{}
	}
};

int
gf100_mspdec_new(struct nvkm_device *device, enum nvkm_subdev_type type, int inst,
		 struct nvkm_engine **pengine)
{
	return nvkm_mspdec_new_(&gf100_mspdec, device, type, inst, pengine);
}
