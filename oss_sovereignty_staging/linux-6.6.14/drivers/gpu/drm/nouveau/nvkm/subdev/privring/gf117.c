 
#include "priv.h"

static int
gf117_privring_init(struct nvkm_subdev *privring)
{
	struct nvkm_device *device = privring->device;
	nvkm_mask(device, 0x122310, 0x0003ffff, 0x00000800);
	nvkm_mask(device, 0x122348, 0x0003ffff, 0x00000100);
	nvkm_mask(device, 0x1223b0, 0x0003ffff, 0x00000fff);
	return 0;
}

static const struct nvkm_subdev_func
gf117_privring = {
	.init = gf117_privring_init,
	.intr = gf100_privring_intr,
};

int
gf117_privring_new(struct nvkm_device *device, enum nvkm_subdev_type type, int inst,
		   struct nvkm_subdev **pprivring)
{
	return nvkm_subdev_new_(&gf117_privring, device, type, inst, pprivring);
}
