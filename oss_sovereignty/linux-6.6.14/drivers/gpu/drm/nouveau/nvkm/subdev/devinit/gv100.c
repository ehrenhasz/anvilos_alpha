 
#include "nv50.h"

#include <subdev/bios.h>
#include <subdev/bios/pll.h>
#include <subdev/clk/pll.h>

static int
gv100_devinit_pll_set(struct nvkm_devinit *init, u32 type, u32 freq)
{
	struct nvkm_subdev *subdev = &init->subdev;
	struct nvkm_device *device = subdev->device;
	struct nvbios_pll info;
	int head = type - PLL_VPLL0;
	int N, fN, M, P;
	int ret;

	ret = nvbios_pll_parse(device->bios, type, &info);
	if (ret)
		return ret;

	ret = gt215_pll_calc(subdev, &info, freq, &N, &fN, &M, &P);
	if (ret < 0)
		return ret;

	switch (info.type) {
	case PLL_VPLL0:
	case PLL_VPLL1:
	case PLL_VPLL2:
	case PLL_VPLL3:
		nvkm_wr32(device, 0x00ef10 + (head * 0x40), fN << 16);
		nvkm_wr32(device, 0x00ef04 + (head * 0x40), (P << 16) |
							    (N <<  8) |
							    (M <<  0));
		break;
	default:
		nvkm_warn(subdev, "%08x/%dKhz unimplemented\n", type, freq);
		ret = -EINVAL;
		break;
	}

	return ret;
}

static const struct nvkm_devinit_func
gv100_devinit = {
	.preinit = gf100_devinit_preinit,
	.init = nv50_devinit_init,
	.post = gm200_devinit_post,
	.pll_set = gv100_devinit_pll_set,
	.disable = gm107_devinit_disable,
};

int
gv100_devinit_new(struct nvkm_device *device, enum nvkm_subdev_type type, int inst,
		  struct nvkm_devinit **pinit)
{
	return nv50_devinit_new_(&gv100_devinit, device, type, inst, pinit);
}
