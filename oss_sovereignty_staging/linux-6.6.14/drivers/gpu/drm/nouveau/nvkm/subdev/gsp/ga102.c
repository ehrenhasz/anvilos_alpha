 
#include "priv.h"

static const struct nvkm_falcon_func
ga102_gsp_flcn = {
	.disable = gm200_flcn_disable,
	.enable = gm200_flcn_enable,
	.select = ga102_flcn_select,
	.addr2 = 0x1000,
	.reset_eng = gp102_flcn_reset_eng,
	.reset_prep = ga102_flcn_reset_prep,
	.reset_wait_mem_scrubbing = ga102_flcn_reset_wait_mem_scrubbing,
	.imem_dma = &ga102_flcn_dma,
	.dmem_dma = &ga102_flcn_dma,
};

static const struct nvkm_gsp_func
ga102_gsp = {
	.flcn = &ga102_gsp_flcn,
};

static int
ga102_gsp_nofw(struct nvkm_gsp *gsp, int ver, const struct nvkm_gsp_fwif *fwif)
{
	return 0;
}

static struct nvkm_gsp_fwif
ga102_gsps[] = {
	{ -1, ga102_gsp_nofw, &ga102_gsp },
	{}
};

int
ga102_gsp_new(struct nvkm_device *device, enum nvkm_subdev_type type, int inst,
	      struct nvkm_gsp **pgsp)
{
	return nvkm_gsp_new_(ga102_gsps, device, type, inst, pgsp);
}
