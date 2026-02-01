 
#include "priv.h"

int
gf100_iccsense_new(struct nvkm_device *device, enum nvkm_subdev_type type, int inst,
		   struct nvkm_iccsense **piccsense)
{
	return nvkm_iccsense_new_(device, type, inst, piccsense);
}
