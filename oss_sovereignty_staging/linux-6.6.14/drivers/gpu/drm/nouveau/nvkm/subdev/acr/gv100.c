 
#include "priv.h"

MODULE_FIRMWARE("nvidia/gv100/acr/unload_bl.bin");
MODULE_FIRMWARE("nvidia/gv100/acr/ucode_unload.bin");

static const struct nvkm_acr_hsf_fwif
gv100_acr_unload_fwif[] = {
	{ 0, gm200_acr_hsfw_ctor, &gp108_acr_hsfw_0, NVKM_ACR_HSF_PMU, 0, 0x00000000 },
	{}
};

MODULE_FIRMWARE("nvidia/gv100/acr/bl.bin");
MODULE_FIRMWARE("nvidia/gv100/acr/ucode_load.bin");

static const struct nvkm_acr_hsf_fwif
gv100_acr_load_fwif[] = {
	{ 0, gm200_acr_hsfw_ctor, &gp108_acr_load_0, NVKM_ACR_HSF_SEC2, 0, 0x00000010 },
	{}
};

static const struct nvkm_acr_func
gv100_acr = {
	.load = gv100_acr_load_fwif,
	.unload = gv100_acr_unload_fwif,
	.wpr_parse = gp102_acr_wpr_parse,
	.wpr_layout = gp102_acr_wpr_layout,
	.wpr_alloc = gp102_acr_wpr_alloc,
	.wpr_build = gp102_acr_wpr_build,
	.wpr_patch = gp102_acr_wpr_patch,
	.wpr_check = gm200_acr_wpr_check,
	.init = gm200_acr_init,
};

static const struct nvkm_acr_fwif
gv100_acr_fwif[] = {
	{  0, gp102_acr_load, &gv100_acr },
	{ -1, gm200_acr_nofw, &gm200_acr },
	{}
};

int
gv100_acr_new(struct nvkm_device *device, enum nvkm_subdev_type type, int inst,
	      struct nvkm_acr **pacr)
{
	return nvkm_acr_new_(gv100_acr_fwif, device, type, inst, pacr);
}
