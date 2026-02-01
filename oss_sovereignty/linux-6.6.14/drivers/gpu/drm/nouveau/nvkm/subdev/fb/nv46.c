 
#include "priv.h"
#include "ram.h"

void
nv46_fb_tile_init(struct nvkm_fb *fb, int i, u32 addr, u32 size, u32 pitch,
		  u32 flags, struct nvkm_fb_tile *tile)
{
	 
	if (!(flags & 4)) tile->addr = (0 << 3);
	else              tile->addr = (1 << 3);

	tile->addr |= 0x00000001;  
	tile->addr |= addr;
	tile->limit = max(1u, addr + size) - 1;
	tile->pitch = pitch;
}

static const struct nvkm_fb_func
nv46_fb = {
	.init = nv44_fb_init,
	.tile.regions = 15,
	.tile.init = nv46_fb_tile_init,
	.tile.fini = nv20_fb_tile_fini,
	.tile.prog = nv44_fb_tile_prog,
	.ram_new = nv44_ram_new,
};

int
nv46_fb_new(struct nvkm_device *device, enum nvkm_subdev_type type, int inst, struct nvkm_fb **pfb)
{
	return nvkm_fb_new_(&nv46_fb, device, type, inst, pfb);
}
