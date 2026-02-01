 
#include "priv.h"

#include <nvif/class.h>

static const struct nvkm_engine_func
g84_mpeg = {
	.init = nv50_mpeg_init,
	.intr = nv50_mpeg_intr,
	.cclass = &nv50_mpeg_cclass,
	.sclass = {
		{ -1, -1, G82_MPEG, &nv31_mpeg_object },
		{}
	}
};

int
g84_mpeg_new(struct nvkm_device *device, enum nvkm_subdev_type type, int inst,
	     struct nvkm_engine **pmpeg)
{
	return nvkm_engine_new_(&g84_mpeg, device, type, inst, true, pmpeg);
}
