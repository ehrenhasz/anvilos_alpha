 
#include <subdev/privring.h>

#include "priv.h"

static int
gp10b_privring_init(struct nvkm_subdev *privring)
{
	struct nvkm_device *device = privring->device;

	nvkm_wr32(device, 0x1200a8, 0x0);

	 
	nvkm_wr32(device, 0x12004c, 0x4);
	nvkm_wr32(device, 0x122204, 0x2);
	nvkm_rd32(device, 0x122204);

	 
	nvkm_wr32(device, 0x009080, 0x800186a0);

	return 0;
}

static const struct nvkm_subdev_func
gp10b_privring = {
	.init = gp10b_privring_init,
	.intr = gk104_privring_intr,
};

int
gp10b_privring_new(struct nvkm_device *device, enum nvkm_subdev_type type, int inst,
		   struct nvkm_subdev **pprivring)
{
	return nvkm_subdev_new_(&gp10b_privring, device, type, inst, pprivring);
}
