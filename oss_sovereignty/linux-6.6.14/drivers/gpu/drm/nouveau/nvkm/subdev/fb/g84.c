 
#include "nv50.h"
#include "ram.h"

static const struct nv50_fb_func
g84_fb = {
	.ram_new = nv50_ram_new,
	.tags = nv20_fb_tags,
	.trap = 0x001d07ff,
};

int
g84_fb_new(struct nvkm_device *device, enum nvkm_subdev_type type, int inst, struct nvkm_fb **pfb)
{
	return nv50_fb_new_(&g84_fb, device, type, inst, pfb);
}
