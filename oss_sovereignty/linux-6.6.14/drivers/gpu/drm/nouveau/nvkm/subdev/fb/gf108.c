 
#include "gf100.h"
#include "ram.h"

static const struct nvkm_fb_func
gf108_fb = {
	.dtor = gf100_fb_dtor,
	.oneinit = gf100_fb_oneinit,
	.init = gf100_fb_init,
	.init_page = gf100_fb_init_page,
	.intr = gf100_fb_intr,
	.sysmem.flush_page_init = gf100_fb_sysmem_flush_page_init,
	.ram_new = gf108_ram_new,
	.default_bigpage = 17,
};

int
gf108_fb_new(struct nvkm_device *device, enum nvkm_subdev_type type, int inst, struct nvkm_fb **pfb)
{
	return gf100_fb_new_(&gf108_fb, device, type, inst, pfb);
}
