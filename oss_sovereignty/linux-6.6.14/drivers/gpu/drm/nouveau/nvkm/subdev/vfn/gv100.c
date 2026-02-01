 
#include "priv.h"

#include <nvif/class.h>

static const struct nvkm_vfn_func
gv100_vfn = {
	.user = { 0x810000, 0x010000, { -1, -1, VOLTA_USERMODE_A } },
};

int
gv100_vfn_new(struct nvkm_device *device,
	      enum nvkm_subdev_type type, int inst, struct nvkm_vfn **pvfn)
{
	return nvkm_vfn_new_(&gv100_vfn, device, type, inst, 0, pvfn);
}
