 
#include "nv04.h"
#include "fbmem.h"

#include <subdev/bios.h>
#include <subdev/bios/init.h>

static void
nv10_devinit_meminit(struct nvkm_devinit *init)
{
	struct nvkm_subdev *subdev = &init->subdev;
	struct nvkm_device *device = subdev->device;
	static const int mem_width[] = { 0x10, 0x00, 0x20 };
	int mem_width_count;
	uint32_t patt = 0xdeadbeef;
	struct io_mapping *fb;
	int i, j, k;

	if (device->card_type >= NV_11 && device->chipset >= 0x17)
		mem_width_count = 3;
	else
		mem_width_count = 2;

	 
	fb = fbmem_init(device);
	if (!fb) {
		nvkm_error(subdev, "failed to map fb\n");
		return;
	}

	nvkm_wr32(device, NV10_PFB_REFCTRL, NV10_PFB_REFCTRL_VALID_1);

	 
	for (i = 0; i < mem_width_count; i++) {
		nvkm_mask(device, NV04_PFB_CFG0, 0x30, mem_width[i]);

		for (j = 0; j < 4; j++) {
			for (k = 0; k < 4; k++)
				fbmem_poke(fb, 0x1c, 0);

			fbmem_poke(fb, 0x1c, patt);
			fbmem_poke(fb, 0x3c, 0);

			if (fbmem_peek(fb, 0x1c) == patt)
				goto mem_width_found;
		}
	}

mem_width_found:
	patt <<= 1;

	 
	for (i = 0; i < 4; i++) {
		int off = nvkm_rd32(device, 0x10020c) - 0x100000;

		fbmem_poke(fb, off, patt);
		fbmem_poke(fb, 0, 0);

		fbmem_peek(fb, 0);
		fbmem_peek(fb, 0);
		fbmem_peek(fb, 0);
		fbmem_peek(fb, 0);

		if (fbmem_peek(fb, off) == patt)
			goto amount_found;
	}

	 
	nvkm_mask(device, NV04_PFB_CFG0, 0x1000, 0);

amount_found:
	fbmem_fini(fb);
}

static const struct nvkm_devinit_func
nv10_devinit = {
	.dtor = nv04_devinit_dtor,
	.preinit = nv04_devinit_preinit,
	.post = nv04_devinit_post,
	.meminit = nv10_devinit_meminit,
	.pll_set = nv04_devinit_pll_set,
};

int
nv10_devinit_new(struct nvkm_device *device, enum nvkm_subdev_type type, int inst,
		 struct nvkm_devinit **pinit)
{
	return nv04_devinit_new_(&nv10_devinit, device, type, inst, pinit);
}
