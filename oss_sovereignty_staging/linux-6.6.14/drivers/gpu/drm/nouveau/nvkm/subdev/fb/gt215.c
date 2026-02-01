 
#include "nv50.h"
#include "ram.h"

static const struct nv50_fb_func
gt215_fb = {
	.ram_new = gt215_ram_new,
	.tags = nv20_fb_tags,
	.trap = 0x000d0fff,
};

int
gt215_fb_new(struct nvkm_device *device, enum nvkm_subdev_type type, int inst, struct nvkm_fb **pfb)
{
	return nv50_fb_new_(&gt215_fb, device, type, inst, pfb);
}
