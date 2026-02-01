 
#include "nv50.h"
#include "ram.h"

static const struct nv50_fb_func
mcp77_fb = {
	.ram_new = mcp77_ram_new,
	.trap = 0x001d07ff,
};

int
mcp77_fb_new(struct nvkm_device *device, enum nvkm_subdev_type type, int inst, struct nvkm_fb **pfb)
{
	return nv50_fb_new_(&mcp77_fb, device, type, inst, pfb);
}
