 
#include "priv.h"

#include <nvif/class.h>

static const struct nvkm_falcon_func
gt215_mspdec = {
	.init = g98_mspdec_init,
	.sclass = {
		{ -1, -1, GT212_MSPDEC },
		{}
	}
};

int
gt215_mspdec_new(struct nvkm_device *device, enum nvkm_subdev_type type, int inst,
		 struct nvkm_engine **pengine)
{
	return nvkm_mspdec_new_(&gt215_mspdec, device, type, inst, pengine);
}
