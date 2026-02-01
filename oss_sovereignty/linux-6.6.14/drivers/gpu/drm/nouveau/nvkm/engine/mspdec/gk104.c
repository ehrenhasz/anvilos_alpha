 
#include "priv.h"

#include <nvif/class.h>

static const struct nvkm_falcon_func
gk104_mspdec = {
	.init = gf100_mspdec_init,
	.sclass = {
		{ -1, -1, GK104_MSPDEC },
		{}
	}
};

int
gk104_mspdec_new(struct nvkm_device *device, enum nvkm_subdev_type type, int inst,
		 struct nvkm_engine **pengine)
{
	return nvkm_mspdec_new_(&gk104_mspdec, device, type, inst, pengine);
}
