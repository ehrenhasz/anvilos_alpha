 
#include "priv.h"

static void
gk104_ltc_init(struct nvkm_ltc *ltc)
{
	struct nvkm_device *device = ltc->subdev.device;
	u32 lpg128 = !(nvkm_rd32(device, 0x100c80) & 0x00000001);

	nvkm_wr32(device, 0x17e8d8, ltc->ltc_nr);
	nvkm_wr32(device, 0x17e000, ltc->ltc_nr);
	nvkm_wr32(device, 0x17e8d4, ltc->tag_base);
	nvkm_mask(device, 0x17e8c0, 0x00000002, lpg128 ? 0x00000002 : 0x00000000);
}

static const struct nvkm_ltc_func
gk104_ltc = {
	.oneinit = gf100_ltc_oneinit,
	.init = gk104_ltc_init,
	.intr = gf100_ltc_intr,
	.cbc_clear = gf100_ltc_cbc_clear,
	.cbc_wait = gf100_ltc_cbc_wait,
	.zbc_color = 16,
	.zbc_depth = 16,
	.zbc_clear_color = gf100_ltc_zbc_clear_color,
	.zbc_clear_depth = gf100_ltc_zbc_clear_depth,
	.invalidate = gf100_ltc_invalidate,
	.flush = gf100_ltc_flush,
};

int
gk104_ltc_new(struct nvkm_device *device, enum nvkm_subdev_type type, int inst,
	      struct nvkm_ltc **pltc)
{
	return nvkm_ltc_new_(&gk104_ltc, device, type, inst, pltc);
}
