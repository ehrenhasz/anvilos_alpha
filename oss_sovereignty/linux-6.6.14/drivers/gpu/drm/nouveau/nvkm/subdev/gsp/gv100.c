 
#include "priv.h"

static const struct nvkm_falcon_func
gv100_gsp_flcn = {
	.disable = gm200_flcn_disable,
	.enable = gm200_flcn_enable,
	.reset_eng = gp102_flcn_reset_eng,
	.reset_wait_mem_scrubbing = gm200_flcn_reset_wait_mem_scrubbing,
	.bind_inst = gm200_flcn_bind_inst,
	.bind_stat = gm200_flcn_bind_stat,
	.bind_intr = true,
	.imem_pio = &gm200_flcn_imem_pio,
	.dmem_pio = &gm200_flcn_dmem_pio,
};

static const struct nvkm_gsp_func
gv100_gsp = {
	.flcn = &gv100_gsp_flcn,
};

static int
gv100_gsp_nofw(struct nvkm_gsp *gsp, int ver, const struct nvkm_gsp_fwif *fwif)
{
	return 0;
}

static struct nvkm_gsp_fwif
gv100_gsps[] = {
	{ -1, gv100_gsp_nofw, &gv100_gsp },
	{}
};

int
gv100_gsp_new(struct nvkm_device *device, enum nvkm_subdev_type type, int inst,
	      struct nvkm_gsp **pgsp)
{
	return nvkm_gsp_new_(gv100_gsps, device, type, inst, pgsp);
}
