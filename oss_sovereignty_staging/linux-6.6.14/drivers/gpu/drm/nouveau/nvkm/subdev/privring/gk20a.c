 
#include <subdev/privring.h>
#include <subdev/timer.h>

static void
gk20a_privring_init_privring_ring(struct nvkm_subdev *privring)
{
	struct nvkm_device *device = privring->device;
	nvkm_mask(device, 0x137250, 0x3f, 0);

	nvkm_mask(device, 0x000200, 0x20, 0);
	udelay(20);
	nvkm_mask(device, 0x000200, 0x20, 0x20);

	nvkm_wr32(device, 0x12004c, 0x4);
	nvkm_wr32(device, 0x122204, 0x2);
	nvkm_rd32(device, 0x122204);

	 
	nvkm_wr32(device, 0x122354, 0x800);
	nvkm_wr32(device, 0x128328, 0x800);
	nvkm_wr32(device, 0x124320, 0x800);
}

static void
gk20a_privring_intr(struct nvkm_subdev *privring)
{
	struct nvkm_device *device = privring->device;
	u32 status0 = nvkm_rd32(device, 0x120058);

	if (status0 & 0x7) {
		nvkm_debug(privring, "resetting privring ring\n");
		gk20a_privring_init_privring_ring(privring);
	}

	 
	nvkm_mask(device, 0x12004c, 0x2, 0x2);
	nvkm_msec(device, 2000,
		if (!(nvkm_rd32(device, 0x12004c) & 0x0000003f))
			break;
	);
}

static int
gk20a_privring_init(struct nvkm_subdev *privring)
{
	gk20a_privring_init_privring_ring(privring);
	return 0;
}

static const struct nvkm_subdev_func
gk20a_privring = {
	.init = gk20a_privring_init,
	.intr = gk20a_privring_intr,
};

int
gk20a_privring_new(struct nvkm_device *device, enum nvkm_subdev_type type, int inst,
		   struct nvkm_subdev **pprivring)
{
	return nvkm_subdev_new_(&gk20a_privring, device, type, inst, pprivring);
}
