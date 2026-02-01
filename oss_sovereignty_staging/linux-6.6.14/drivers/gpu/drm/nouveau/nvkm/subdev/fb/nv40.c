 
#include "priv.h"
#include "ram.h"

void
nv40_fb_tile_comp(struct nvkm_fb *fb, int i, u32 size, u32 flags,
		  struct nvkm_fb_tile *tile)
{
	u32 tiles = DIV_ROUND_UP(size, 0x80);
	u32 tags  = round_up(tiles / fb->ram->parts, 0x100);
	if ( (flags & 2) &&
	    !nvkm_mm_head(&fb->tags.mm, 0, 1, tags, tags, 1, &tile->tag)) {
		tile->zcomp  = 0x28000000;  
		tile->zcomp |= ((tile->tag->offset           ) >> 8);
		tile->zcomp |= ((tile->tag->offset + tags - 1) >> 8) << 13;
#ifdef __BIG_ENDIAN
		tile->zcomp |= 0x40000000;
#endif
	}
}

static void
nv40_fb_init(struct nvkm_fb *fb)
{
	nvkm_mask(fb->subdev.device, 0x10033c, 0x00008000, 0x00000000);
}

static const struct nvkm_fb_func
nv40_fb = {
	.tags = nv20_fb_tags,
	.init = nv40_fb_init,
	.tile.regions = 8,
	.tile.init = nv30_fb_tile_init,
	.tile.comp = nv40_fb_tile_comp,
	.tile.fini = nv20_fb_tile_fini,
	.tile.prog = nv20_fb_tile_prog,
	.ram_new = nv40_ram_new,
};

int
nv40_fb_new(struct nvkm_device *device, enum nvkm_subdev_type type, int inst, struct nvkm_fb **pfb)
{
	return nvkm_fb_new_(&nv40_fb, device, type, inst, pfb);
}
