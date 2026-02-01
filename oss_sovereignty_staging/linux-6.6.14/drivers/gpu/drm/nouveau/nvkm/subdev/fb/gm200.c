 
#include "gf100.h"
#include "ram.h"

#include <core/memory.h>

int
gm200_fb_init_page(struct nvkm_fb *fb)
{
	struct nvkm_device *device = fb->subdev.device;
	switch (fb->page) {
	case 16: nvkm_mask(device, 0x100c80, 0x00001801, 0x00001001); break;
	case 17: nvkm_mask(device, 0x100c80, 0x00001801, 0x00000000); break;
	case  0: nvkm_mask(device, 0x100c80, 0x00001800, 0x00001800); break;
	default:
		return -EINVAL;
	}
	return 0;
}

void
gm200_fb_init(struct nvkm_fb *base)
{
	struct gf100_fb *fb = gf100_fb(base);
	struct nvkm_device *device = fb->base.subdev.device;

	nvkm_wr32(device, 0x100cc8, nvkm_memory_addr(fb->base.mmu_wr) >> 8);
	nvkm_wr32(device, 0x100ccc, nvkm_memory_addr(fb->base.mmu_rd) >> 8);
	nvkm_mask(device, 0x100cc4, 0x00060000,
		  min(nvkm_memory_size(fb->base.mmu_rd) >> 16, (u64)2) << 17);
}

static const struct nvkm_fb_func
gm200_fb = {
	.dtor = gf100_fb_dtor,
	.oneinit = gf100_fb_oneinit,
	.init = gm200_fb_init,
	.init_page = gm200_fb_init_page,
	.intr = gf100_fb_intr,
	.sysmem.flush_page_init = gf100_fb_sysmem_flush_page_init,
	.ram_new = gm200_ram_new,
	.default_bigpage = 0  ,
};

int
gm200_fb_new(struct nvkm_device *device, enum nvkm_subdev_type type, int inst, struct nvkm_fb **pfb)
{
	return gf100_fb_new_(&gm200_fb, device, type, inst, pfb);
}
