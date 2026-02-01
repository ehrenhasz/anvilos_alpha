 
#include "gf100.h"
#include "ram.h"

int
gv100_fb_init_page(struct nvkm_fb *fb)
{
	return (fb->page == 16) ? 0 : -EINVAL;
}

static const struct nvkm_fb_func
gv100_fb = {
	.dtor = gf100_fb_dtor,
	.oneinit = gp102_fb_oneinit,
	.init = gm200_fb_init,
	.init_page = gv100_fb_init_page,
	.init_unkn = gp100_fb_init_unkn,
	.sysmem.flush_page_init = gf100_fb_sysmem_flush_page_init,
	.vidmem.size = gp102_fb_vidmem_size,
	.vpr.scrub_required = gp102_fb_vpr_scrub_required,
	.vpr.scrub = gp102_fb_vpr_scrub,
	.ram_new = gp102_ram_new,
	.default_bigpage = 16,
};

int
gv100_fb_new(struct nvkm_device *device, enum nvkm_subdev_type type, int inst, struct nvkm_fb **pfb)
{
	return gf100_fb_new_(&gv100_fb, device, type, inst, pfb);
}

MODULE_FIRMWARE("nvidia/gv100/nvdec/scrubber.bin");
