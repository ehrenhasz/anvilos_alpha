 
#include "priv.h"
#include "ram.h"

static void
nv44_fb_tile_init(struct nvkm_fb *fb, int i, u32 addr, u32 size, u32 pitch,
		  u32 flags, struct nvkm_fb_tile *tile)
{
	tile->addr  = 0x00000001;  
	tile->addr |= addr;
	tile->limit = max(1u, addr + size) - 1;
	tile->pitch = pitch;
}

void
nv44_fb_tile_prog(struct nvkm_fb *fb, int i, struct nvkm_fb_tile *tile)
{
	struct nvkm_device *device = fb->subdev.device;
	nvkm_wr32(device, 0x100604 + (i * 0x10), tile->limit);
	nvkm_wr32(device, 0x100608 + (i * 0x10), tile->pitch);
	nvkm_wr32(device, 0x100600 + (i * 0x10), tile->addr);
	nvkm_rd32(device, 0x100600 + (i * 0x10));
}

void
nv44_fb_init(struct nvkm_fb *fb)
{
	struct nvkm_device *device = fb->subdev.device;
	nvkm_wr32(device, 0x100850, 0x80000000);
	nvkm_wr32(device, 0x100800, 0x00000001);
}

static const struct nvkm_fb_func
nv44_fb = {
	.init = nv44_fb_init,
	.tile.regions = 12,
	.tile.init = nv44_fb_tile_init,
	.tile.fini = nv20_fb_tile_fini,
	.tile.prog = nv44_fb_tile_prog,
	.ram_new = nv44_ram_new,
};

int
nv44_fb_new(struct nvkm_device *device, enum nvkm_subdev_type type, int inst, struct nvkm_fb **pfb)
{
	return nvkm_fb_new_(&nv44_fb, device, type, inst, pfb);
}
