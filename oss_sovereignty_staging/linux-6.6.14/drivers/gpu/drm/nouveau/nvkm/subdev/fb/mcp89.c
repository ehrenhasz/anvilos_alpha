 
#include "nv50.h"
#include "ram.h"

static const struct nv50_fb_func
mcp89_fb = {
	.ram_new = mcp77_ram_new,
	.trap = 0x089d1fff,
};

int
mcp89_fb_new(struct nvkm_device *device, enum nvkm_subdev_type type, int inst, struct nvkm_fb **pfb)
{
	return nv50_fb_new_(&mcp89_fb, device, type, inst, pfb);
}
