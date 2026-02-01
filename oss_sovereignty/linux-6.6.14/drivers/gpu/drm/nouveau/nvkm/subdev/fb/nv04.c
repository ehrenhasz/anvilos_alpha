 
#include "priv.h"
#include "ram.h"
#include "regsnv04.h"

static void
nv04_fb_init(struct nvkm_fb *fb)
{
	struct nvkm_device *device = fb->subdev.device;

	 
	nvkm_wr32(device, NV04_PFB_CFG0, 0x1114);
}

static const struct nvkm_fb_func
nv04_fb = {
	.init = nv04_fb_init,
	.ram_new = nv04_ram_new,
};

int
nv04_fb_new(struct nvkm_device *device, enum nvkm_subdev_type type, int inst, struct nvkm_fb **pfb)
{
	return nvkm_fb_new_(&nv04_fb, device, type, inst, pfb);
}
