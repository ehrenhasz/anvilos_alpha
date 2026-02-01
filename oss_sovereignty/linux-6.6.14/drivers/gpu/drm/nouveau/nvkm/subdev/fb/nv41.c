 
#include "priv.h"
#include "ram.h"

void
nv41_fb_tile_prog(struct nvkm_fb *fb, int i, struct nvkm_fb_tile *tile)
{
	struct nvkm_device *device = fb->subdev.device;
	nvkm_wr32(device, 0x100604 + (i * 0x10), tile->limit);
	nvkm_wr32(device, 0x100608 + (i * 0x10), tile->pitch);
	nvkm_wr32(device, 0x100600 + (i * 0x10), tile->addr);
	nvkm_rd32(device, 0x100600 + (i * 0x10));
	nvkm_wr32(device, 0x100700 + (i * 0x04), tile->zcomp);
}

void
nv41_fb_init(struct nvkm_fb *fb)
{
	nvkm_wr32(fb->subdev.device, 0x100800, 0x00000001);
}

static const struct nvkm_fb_func
nv41_fb = {
	.tags = nv20_fb_tags,
	.init = nv41_fb_init,
	.tile.regions = 12,
	.tile.init = nv30_fb_tile_init,
	.tile.comp = nv40_fb_tile_comp,
	.tile.fini = nv20_fb_tile_fini,
	.tile.prog = nv41_fb_tile_prog,
	.ram_new = nv41_ram_new,
};

int
nv41_fb_new(struct nvkm_device *device, enum nvkm_subdev_type type, int inst, struct nvkm_fb **pfb)
{
	return nvkm_fb_new_(&nv41_fb, device, type, inst, pfb);
}
