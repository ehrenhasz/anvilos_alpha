 
#include "priv.h"

#include <nvif/class.h>

static const struct nvkm_falcon_func
mcp89_msvld = {
	.init = g98_msvld_init,
	.sclass = {
		{ -1, -1, IGT21A_MSVLD },
		{}
	}
};

int
mcp89_msvld_new(struct nvkm_device *device, enum nvkm_subdev_type type, int inst,
		struct nvkm_engine **pengine)
{
	return nvkm_msvld_new_(&mcp89_msvld, device, type, inst, pengine);
}
