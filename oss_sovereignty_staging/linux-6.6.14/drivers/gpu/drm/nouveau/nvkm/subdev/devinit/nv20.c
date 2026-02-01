 
#include "nv04.h"
#include "fbmem.h"

#include <subdev/bios.h>
#include <subdev/bios/init.h>

static void
nv20_devinit_meminit(struct nvkm_devinit *init)
{
	struct nvkm_subdev *subdev = &init->subdev;
	struct nvkm_device *device = subdev->device;
	uint32_t mask = (device->chipset >= 0x25 ? 0x300 : 0x900);
	uint32_t amount, off;
	struct io_mapping *fb;

	 
	fb = fbmem_init(device);
	if (!fb) {
		nvkm_error(subdev, "failed to map fb\n");
		return;
	}

	nvkm_wr32(device, NV10_PFB_REFCTRL, NV10_PFB_REFCTRL_VALID_1);

	 
	nvkm_mask(device, NV04_PFB_CFG0, 0, mask);

	amount = nvkm_rd32(device, 0x10020c);
	for (off = amount; off > 0x2000000; off -= 0x2000000)
		fbmem_poke(fb, off - 4, off);

	amount = nvkm_rd32(device, 0x10020c);
	if (amount != fbmem_peek(fb, amount - 4))
		 
		nvkm_mask(device, NV04_PFB_CFG0, mask, 0);

	fbmem_fini(fb);
}

static const struct nvkm_devinit_func
nv20_devinit = {
	.dtor = nv04_devinit_dtor,
	.preinit = nv04_devinit_preinit,
	.post = nv04_devinit_post,
	.meminit = nv20_devinit_meminit,
	.pll_set = nv04_devinit_pll_set,
};

int
nv20_devinit_new(struct nvkm_device *device, enum nvkm_subdev_type type, int inst,
		 struct nvkm_devinit **pinit)
{
	return nv04_devinit_new_(&nv20_devinit, device, type, inst, pinit);
}
