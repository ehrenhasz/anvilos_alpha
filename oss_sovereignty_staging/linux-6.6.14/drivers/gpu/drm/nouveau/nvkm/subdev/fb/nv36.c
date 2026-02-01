 
#include "priv.h"
#include "ram.h"

static void
nv36_fb_tile_comp(struct nvkm_fb *fb, int i, u32 size, u32 flags,
		  struct nvkm_fb_tile *tile)
{
	u32 tiles = DIV_ROUND_UP(size, 0x40);
	u32 tags  = round_up(tiles / fb->ram->parts, 0x40);
	if (!nvkm_mm_head(&fb->tags.mm, 0, 1, tags, tags, 1, &tile->tag)) {
		if (flags & 2) tile->zcomp |= 0x10000000;  
		else           tile->zcomp |= 0x20000000;  
		tile->zcomp |= ((tile->tag->offset           ) >> 6);
		tile->zcomp |= ((tile->tag->offset + tags - 1) >> 6) << 14;
#ifdef __BIG_ENDIAN
		tile->zcomp |= 0x80000000;
#endif
	}
}

static const struct nvkm_fb_func
nv36_fb = {
	.tags = nv20_fb_tags,
	.init = nv30_fb_init,
	.tile.regions = 8,
	.tile.init = nv30_fb_tile_init,
	.tile.comp = nv36_fb_tile_comp,
	.tile.fini = nv20_fb_tile_fini,
	.tile.prog = nv20_fb_tile_prog,
	.ram_new = nv20_ram_new,
};

int
nv36_fb_new(struct nvkm_device *device, enum nvkm_subdev_type type, int inst, struct nvkm_fb **pfb)
{
	return nvkm_fb_new_(&nv36_fb, device, type, inst, pfb);
}
