 
#include "priv.h"
#include "ram.h"

static const struct nvkm_fb_func
nv4e_fb = {
	.init = nv44_fb_init,
	.tile.regions = 12,
	.tile.init = nv46_fb_tile_init,
	.tile.fini = nv20_fb_tile_fini,
	.tile.prog = nv44_fb_tile_prog,
	.ram_new = nv44_ram_new,
};

int
nv4e_fb_new(struct nvkm_device *device, enum nvkm_subdev_type type, int inst, struct nvkm_fb **pfb)
{
	return nvkm_fb_new_(&nv4e_fb, device, type, inst, pfb);
}
