 
#include "priv.h"

static const struct nvkm_subdev_func
gm200_privring = {
	.intr = gk104_privring_intr,
};

int
gm200_privring_new(struct nvkm_device *device, enum nvkm_subdev_type type, int inst,
		   struct nvkm_subdev **pprivring)
{
	return nvkm_subdev_new_(&gm200_privring, device, type, inst, pprivring);
}
