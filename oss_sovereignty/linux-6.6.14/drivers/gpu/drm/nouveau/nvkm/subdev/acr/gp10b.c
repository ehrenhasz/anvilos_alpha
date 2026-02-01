 
#include "priv.h"

#if IS_ENABLED(CONFIG_ARCH_TEGRA_186_SOC)
MODULE_FIRMWARE("nvidia/gp10b/acr/bl.bin");
MODULE_FIRMWARE("nvidia/gp10b/acr/ucode_load.bin");
#endif

static const struct nvkm_acr_hsf_fwif
gp10b_acr_load_fwif[] = {
	{ 0, gm200_acr_hsfw_ctor, &gm20b_acr_load_0, NVKM_ACR_HSF_PMU, 0, 0x00000010 },
	{}
};

static const struct nvkm_acr_func
gp10b_acr = {
	.load = gp10b_acr_load_fwif,
	.wpr_parse = gm200_acr_wpr_parse,
	.wpr_layout = gm200_acr_wpr_layout,
	.wpr_alloc = gm20b_acr_wpr_alloc,
	.wpr_build = gm200_acr_wpr_build,
	.wpr_patch = gm200_acr_wpr_patch,
	.wpr_check = gm200_acr_wpr_check,
	.init = gm200_acr_init,
};

static const struct nvkm_acr_fwif
gp10b_acr_fwif[] = {
	{  0, gm20b_acr_load, &gp10b_acr },
	{ -1, gm200_acr_nofw, &gm200_acr },
	{}
};

int
gp10b_acr_new(struct nvkm_device *device, enum nvkm_subdev_type type, int inst,
	      struct nvkm_acr **pacr)
{
	return nvkm_acr_new_(gp10b_acr_fwif, device, type, inst, pacr);
}
