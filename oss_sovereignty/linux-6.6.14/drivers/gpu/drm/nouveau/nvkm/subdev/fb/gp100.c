 
#include "gf100.h"
#include "ram.h"

#include <core/memory.h>

void
gp100_fb_init_unkn(struct nvkm_fb *base)
{
	struct nvkm_device *device = gf100_fb(base)->base.subdev.device;
	nvkm_wr32(device, 0x1fac80, nvkm_rd32(device, 0x100c80));
	nvkm_wr32(device, 0x1facc4, nvkm_rd32(device, 0x100cc4));
	nvkm_wr32(device, 0x1facc8, nvkm_rd32(device, 0x100cc8));
	nvkm_wr32(device, 0x1faccc, nvkm_rd32(device, 0x100ccc));
}

void
gp100_fb_init_remapper(struct nvkm_fb *fb)
{
	struct nvkm_device *device = fb->subdev.device;
	 
	nvkm_mask(device, 0x100c14, 0x00040000, 0x00000000);
}

static const struct nvkm_fb_func
gp100_fb = {
	.dtor = gf100_fb_dtor,
	.oneinit = gf100_fb_oneinit,
	.init = gm200_fb_init,
	.init_remapper = gp100_fb_init_remapper,
	.init_page = gm200_fb_init_page,
	.init_unkn = gp100_fb_init_unkn,
	.sysmem.flush_page_init = gf100_fb_sysmem_flush_page_init,
	.ram_new = gp100_ram_new,
};

int
gp100_fb_new(struct nvkm_device *device, enum nvkm_subdev_type type, int inst, struct nvkm_fb **pfb)
{
	return gf100_fb_new_(&gp100_fb, device, type, inst, pfb);
}
