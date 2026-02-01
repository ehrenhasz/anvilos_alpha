 
#include "priv.h"

#include <subdev/fb.h>
#include <subdev/timer.h>

static int
gm200_ltc_oneinit(struct nvkm_ltc *ltc)
{
	struct nvkm_device *device = ltc->subdev.device;

	ltc->ltc_nr = nvkm_rd32(device, 0x12006c);
	ltc->lts_nr = nvkm_rd32(device, 0x17e280) >> 28;

	return gf100_ltc_oneinit_tag_ram(ltc);
}
static void
gm200_ltc_init(struct nvkm_ltc *ltc)
{
	nvkm_wr32(ltc->subdev.device, 0x17e278, ltc->tag_base);
}

static const struct nvkm_ltc_func
gm200_ltc = {
	.oneinit = gm200_ltc_oneinit,
	.init = gm200_ltc_init,
	.intr = gm107_ltc_intr,
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
gm200_ltc_new(struct nvkm_device *device, enum nvkm_subdev_type type, int inst,
	      struct nvkm_ltc **pltc)
{
	return nvkm_ltc_new_(&gm200_ltc, device, type, inst, pltc);
}
