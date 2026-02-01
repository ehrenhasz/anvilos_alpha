 
#include "gf100.h"
#include "ram.h"

#include <engine/nvdec.h>

static u64
ga102_fb_vidmem_size(struct nvkm_fb *fb)
{
	return (u64)nvkm_rd32(fb->subdev.device, 0x1183a4) << 20;
}

static int
ga102_fb_oneinit(struct nvkm_fb *fb)
{
	struct nvkm_subdev *subdev = &fb->subdev;

	nvkm_falcon_fw_ctor_hs_v2(&ga102_flcn_fw, "mem-unlock", subdev, "nvdec/scrubber",
				  0, &subdev->device->nvdec[0]->falcon, &fb->vpr_scrubber);

	return gf100_fb_oneinit(fb);
}

static const struct nvkm_fb_func
ga102_fb = {
	.dtor = gf100_fb_dtor,
	.oneinit = ga102_fb_oneinit,
	.init = gm200_fb_init,
	.init_page = gv100_fb_init_page,
	.init_unkn = gp100_fb_init_unkn,
	.sysmem.flush_page_init = gf100_fb_sysmem_flush_page_init,
	.vidmem.size = ga102_fb_vidmem_size,
	.ram_new = gp102_ram_new,
	.default_bigpage = 16,
	.vpr.scrub_required = tu102_fb_vpr_scrub_required,
	.vpr.scrub = gp102_fb_vpr_scrub,
};

int
ga102_fb_new(struct nvkm_device *device, enum nvkm_subdev_type type, int inst, struct nvkm_fb **pfb)
{
	return gf100_fb_new_(&ga102_fb, device, type, inst, pfb);
}

MODULE_FIRMWARE("nvidia/ga102/nvdec/scrubber.bin");
MODULE_FIRMWARE("nvidia/ga103/nvdec/scrubber.bin");
MODULE_FIRMWARE("nvidia/ga104/nvdec/scrubber.bin");
MODULE_FIRMWARE("nvidia/ga106/nvdec/scrubber.bin");
MODULE_FIRMWARE("nvidia/ga107/nvdec/scrubber.bin");
