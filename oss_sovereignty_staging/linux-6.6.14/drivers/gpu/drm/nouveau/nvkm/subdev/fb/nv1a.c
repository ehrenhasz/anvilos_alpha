 
#include "priv.h"
#include "ram.h"

static const struct nvkm_fb_func
nv1a_fb = {
	.tile.regions = 8,
	.tile.init = nv10_fb_tile_init,
	.tile.fini = nv10_fb_tile_fini,
	.tile.prog = nv10_fb_tile_prog,
	.ram_new = nv1a_ram_new,
};

int
nv1a_fb_new(struct nvkm_device *device, enum nvkm_subdev_type type, int inst, struct nvkm_fb **pfb)
{
	return nvkm_fb_new_(&nv1a_fb, device, type, inst, pfb);
}
