 
#include "nv50.h"

#include <subdev/bios.h>
#include <subdev/bios/init.h>

void
gm107_devinit_disable(struct nvkm_devinit *init)
{
	struct nvkm_device *device = init->subdev.device;
	u32 r021c00 = nvkm_rd32(device, 0x021c00);
	u32 r021c04 = nvkm_rd32(device, 0x021c04);

	if (r021c00 & 0x00000001)
		nvkm_subdev_disable(device, NVKM_ENGINE_CE, 0);
	if (r021c00 & 0x00000004)
		nvkm_subdev_disable(device, NVKM_ENGINE_CE, 2);
	if (r021c04 & 0x00000001)
		nvkm_subdev_disable(device, NVKM_ENGINE_DISP, 0);
}

static const struct nvkm_devinit_func
gm107_devinit = {
	.preinit = gf100_devinit_preinit,
	.init = nv50_devinit_init,
	.post = nv04_devinit_post,
	.pll_set = gf100_devinit_pll_set,
	.disable = gm107_devinit_disable,
};

int
gm107_devinit_new(struct nvkm_device *device, enum nvkm_subdev_type type, int inst,
		  struct nvkm_devinit **pinit)
{
	return nv50_devinit_new_(&gm107_devinit, device, type, inst, pinit);
}
