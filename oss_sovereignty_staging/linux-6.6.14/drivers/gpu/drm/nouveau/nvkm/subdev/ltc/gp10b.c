 

#include "priv.h"

static void
gp10b_ltc_init(struct nvkm_ltc *ltc)
{
	struct nvkm_device *device = ltc->subdev.device;
	struct iommu_fwspec *spec;

	nvkm_wr32(device, 0x17e27c, ltc->ltc_nr);
	nvkm_wr32(device, 0x17e000, ltc->ltc_nr);
	nvkm_wr32(device, 0x100800, ltc->ltc_nr);

	spec = dev_iommu_fwspec_get(device->dev);
	if (spec) {
		u32 sid = spec->ids[0] & 0xffff;

		 
		nvkm_wr32(device, 0x160000, sid << 2);
	}
}

static const struct nvkm_ltc_func
gp10b_ltc = {
	.oneinit = gp100_ltc_oneinit,
	.init = gp10b_ltc_init,
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
gp10b_ltc_new(struct nvkm_device *device, enum nvkm_subdev_type type, int inst,
	      struct nvkm_ltc **pltc)
{
	return nvkm_ltc_new_(&gp10b_ltc, device, type, inst, pltc);
}
