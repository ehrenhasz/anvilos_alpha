 
#include "priv.h"

static const struct nvkm_falcon_func
gm107_nvdec_flcn = {
	.disable = gm200_flcn_disable,
	.enable = gm200_flcn_enable,
	.reset_pmc = true,
	.reset_wait_mem_scrubbing = gm200_flcn_reset_wait_mem_scrubbing,
	.debug = 0xd00,
	.imem_pio = &gm200_flcn_imem_pio,
	.dmem_pio = &gm200_flcn_dmem_pio,
};

static const struct nvkm_nvdec_func
gm107_nvdec = {
	.flcn = &gm107_nvdec_flcn,
};

static int
gm107_nvdec_nofw(struct nvkm_nvdec *nvdec, int ver,
		 const struct nvkm_nvdec_fwif *fwif)
{
	return 0;
}

static const struct nvkm_nvdec_fwif
gm107_nvdec_fwif[] = {
	{ -1, gm107_nvdec_nofw, &gm107_nvdec },
	{}
};

int
gm107_nvdec_new(struct nvkm_device *device, enum nvkm_subdev_type type, int inst,
		struct nvkm_nvdec **pnvdec)
{
	return nvkm_nvdec_new_(gm107_nvdec_fwif, device, type, inst, 0, pnvdec);
}
