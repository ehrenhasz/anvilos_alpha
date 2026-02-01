 
#include "nv50.h"

#include <subdev/bios.h>
#include <subdev/bios/init.h>

static void
mcp89_devinit_disable(struct nvkm_devinit *init)
{
	struct nvkm_device *device = init->subdev.device;
	u32 r001540 = nvkm_rd32(device, 0x001540);
	u32 r00154c = nvkm_rd32(device, 0x00154c);

	if (!(r001540 & 0x40000000)) {
		nvkm_subdev_disable(device, NVKM_ENGINE_MSPDEC, 0);
		nvkm_subdev_disable(device, NVKM_ENGINE_MSPPP, 0);
	}

	if (!(r00154c & 0x00000004))
		nvkm_subdev_disable(device, NVKM_ENGINE_DISP, 0);
	if (!(r00154c & 0x00000020))
		nvkm_subdev_disable(device, NVKM_ENGINE_MSVLD, 0);
	if (!(r00154c & 0x00000040))
		nvkm_subdev_disable(device, NVKM_ENGINE_VIC, 0);
	if (!(r00154c & 0x00000200))
		nvkm_subdev_disable(device, NVKM_ENGINE_CE, 0);
}

static const struct nvkm_devinit_func
mcp89_devinit = {
	.preinit = nv50_devinit_preinit,
	.init = nv50_devinit_init,
	.post = nv04_devinit_post,
	.pll_set = gt215_devinit_pll_set,
	.disable = mcp89_devinit_disable,
};

int
mcp89_devinit_new(struct nvkm_device *device, enum nvkm_subdev_type type, int inst,
		  struct nvkm_devinit **pinit)
{
	return nv50_devinit_new_(&mcp89_devinit, device, type, inst, pinit);
}
