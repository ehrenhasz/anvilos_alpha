 
#include "gf100.h"
#include "ram.h"

#include <engine/nvdec.h>

int
gp102_fb_vpr_scrub(struct nvkm_fb *fb)
{
	return nvkm_falcon_fw_boot(&fb->vpr_scrubber, &fb->subdev, true, NULL, NULL, 0, 0x00000000);
}

bool
gp102_fb_vpr_scrub_required(struct nvkm_fb *fb)
{
	struct nvkm_device *device = fb->subdev.device;
	nvkm_wr32(device, 0x100cd0, 0x2);
	return (nvkm_rd32(device, 0x100cd0) & 0x00000010) != 0;
}

u64
gp102_fb_vidmem_size(struct nvkm_fb *fb)
{
	const u32 data = nvkm_rd32(fb->subdev.device, 0x100ce0);
	const u32 lmag = (data & 0x000003f0) >> 4;
	const u32 lsca = (data & 0x0000000f);
	const u64 size = (u64)lmag << (lsca + 20);

	if (data & 0x40000000)
		return size / 16 * 15;

	return size;
}

int
gp102_fb_oneinit(struct nvkm_fb *fb)
{
	struct nvkm_subdev *subdev = &fb->subdev;

	nvkm_falcon_fw_ctor_hs(&gm200_flcn_fw, "mem-unlock", subdev, NULL, "nvdec/scrubber",
			       0, &subdev->device->nvdec[0]->falcon, &fb->vpr_scrubber);

	return gf100_fb_oneinit(fb);
}

static const struct nvkm_fb_func
gp102_fb = {
	.dtor = gf100_fb_dtor,
	.oneinit = gp102_fb_oneinit,
	.init = gm200_fb_init,
	.init_remapper = gp100_fb_init_remapper,
	.init_page = gm200_fb_init_page,
	.sysmem.flush_page_init = gf100_fb_sysmem_flush_page_init,
	.vidmem.size = gp102_fb_vidmem_size,
	.vpr.scrub_required = gp102_fb_vpr_scrub_required,
	.vpr.scrub = gp102_fb_vpr_scrub,
	.ram_new = gp102_ram_new,
};

int
gp102_fb_new(struct nvkm_device *device, enum nvkm_subdev_type type, int inst, struct nvkm_fb **pfb)
{
	return gf100_fb_new_(&gp102_fb, device, type, inst, pfb);
}

MODULE_FIRMWARE("nvidia/gp102/nvdec/scrubber.bin");
MODULE_FIRMWARE("nvidia/gp104/nvdec/scrubber.bin");
MODULE_FIRMWARE("nvidia/gp106/nvdec/scrubber.bin");
MODULE_FIRMWARE("nvidia/gp107/nvdec/scrubber.bin");
MODULE_FIRMWARE("nvidia/gp108/nvdec/scrubber.bin");
