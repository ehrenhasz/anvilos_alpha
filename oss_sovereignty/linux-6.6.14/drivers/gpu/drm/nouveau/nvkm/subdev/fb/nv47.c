 
#include "priv.h"
#include "ram.h"

static const struct nvkm_fb_func
nv47_fb = {
	.tags = nv20_fb_tags,
	.init = nv41_fb_init,
	.tile.regions = 15,
	.tile.init = nv30_fb_tile_init,
	.tile.comp = nv40_fb_tile_comp,
	.tile.fini = nv20_fb_tile_fini,
	.tile.prog = nv41_fb_tile_prog,
	.ram_new = nv41_ram_new,
};

int
nv47_fb_new(struct nvkm_device *device, enum nvkm_subdev_type type, int inst, struct nvkm_fb **pfb)
{
	return nvkm_fb_new_(&nv47_fb, device, type, inst, pfb);
}
