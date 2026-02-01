 
#include "priv.h"

void
gp102_ltc_zbc_clear_stencil(struct nvkm_ltc *ltc, int i, const u32 stencil)
{
	struct nvkm_device *device = ltc->subdev.device;
	nvkm_mask(device, 0x17e338, 0x0000000f, i);
	nvkm_wr32(device, 0x17e204, stencil);
}

static const struct nvkm_ltc_func
gp102_ltc = {
	.oneinit = gp100_ltc_oneinit,
	.init = gp100_ltc_init,
	.intr = gp100_ltc_intr,
	.cbc_clear = gm107_ltc_cbc_clear,
	.cbc_wait = gm107_ltc_cbc_wait,
	.zbc_color = 16,
	.zbc_depth = 16,
	.zbc_clear_color = gm107_ltc_zbc_clear_color,
	.zbc_clear_depth = gm107_ltc_zbc_clear_depth,
	.zbc_clear_stencil = gp102_ltc_zbc_clear_stencil,
	.invalidate = gf100_ltc_invalidate,
	.flush = gf100_ltc_flush,
};

int
gp102_ltc_new(struct nvkm_device *device, enum nvkm_subdev_type type, int inst,
	      struct nvkm_ltc **pltc)
{
	return nvkm_ltc_new_(&gp102_ltc, device, type, inst, pltc);
}
