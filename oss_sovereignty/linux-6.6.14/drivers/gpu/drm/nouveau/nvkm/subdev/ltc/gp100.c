 
#include "priv.h"

void
gp100_ltc_intr(struct nvkm_ltc *ltc)
{
	struct nvkm_device *device = ltc->subdev.device;
	u32 mask;

	mask = nvkm_rd32(device, 0x0001c0);
	while (mask) {
		u32 s, c = __ffs(mask);
		for (s = 0; s < ltc->lts_nr; s++)
			gm107_ltc_intr_lts(ltc, c, s);
		mask &= ~(1 << c);
	}
}

int
gp100_ltc_oneinit(struct nvkm_ltc *ltc)
{
	struct nvkm_device *device = ltc->subdev.device;
	ltc->ltc_nr = nvkm_rd32(device, 0x12006c);
	ltc->lts_nr = nvkm_rd32(device, 0x17e280) >> 28;
	 
	return 0;
}

void
gp100_ltc_init(struct nvkm_ltc *ltc)
{
	 
}

static const struct nvkm_ltc_func
gp100_ltc = {
	.oneinit = gp100_ltc_oneinit,
	.init = gp100_ltc_init,
	.intr = gp100_ltc_intr,
	.cbc_clear = gm107_ltc_cbc_clear,
	.cbc_wait = gm107_ltc_cbc_wait,
	.zbc_color = 16,
	.zbc_depth = 16,
	.zbc_clear_color = gm107_ltc_zbc_clear_color,
	.zbc_clear_depth = gm107_ltc_zbc_clear_depth,
	.invalidate = gf100_ltc_invalidate,
	.flush = gf100_ltc_flush,
};

int
gp100_ltc_new(struct nvkm_device *device, enum nvkm_subdev_type type, int inst,
	      struct nvkm_ltc **pltc)
{
	return nvkm_ltc_new_(&gp100_ltc, device, type, inst, pltc);
}
