 
#include "priv.h"
#include "ram.h"

static void
nv25_fb_tile_comp(struct nvkm_fb *fb, int i, u32 size, u32 flags,
		  struct nvkm_fb_tile *tile)
{
	u32 tiles = DIV_ROUND_UP(size, 0x40);
	u32 tags  = round_up(tiles / fb->ram->parts, 0x40);
	if (!nvkm_mm_head(&fb->tags.mm, 0, 1, tags, tags, 1, &tile->tag)) {
		if (!(flags & 2)) tile->zcomp = 0x00100000;  
		else              tile->zcomp = 0x00200000;  
		tile->zcomp |= tile->tag->offset;
#ifdef __BIG_ENDIAN
		tile->zcomp |= 0x01000000;
#endif
	}
}

static const struct nvkm_fb_func
nv25_fb = {
	.tags = nv20_fb_tags,
	.tile.regions = 8,
	.tile.init = nv20_fb_tile_init,
	.tile.comp = nv25_fb_tile_comp,
	.tile.fini = nv20_fb_tile_fini,
	.tile.prog = nv20_fb_tile_prog,
	.ram_new = nv20_ram_new,
};

int
nv25_fb_new(struct nvkm_device *device, enum nvkm_subdev_type type, int inst, struct nvkm_fb **pfb)
{
	return nvkm_fb_new_(&nv25_fb, device, type, inst, pfb);
}
