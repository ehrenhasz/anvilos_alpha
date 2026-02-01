 
#include "priv.h"

#include <core/firmware.h>
#include <core/memory.h>
#include <subdev/gsp.h>
#include <subdev/pmu.h>
#include <engine/sec2.h>

#include <nvfw/acr.h>

int
tu102_acr_init(struct nvkm_acr *acr)
{
	int ret = nvkm_acr_hsfw_boot(acr, "AHESASC");
	if (ret)
		return ret;

	return nvkm_acr_hsfw_boot(acr, "ASB");
}

static int
tu102_acr_wpr_build(struct nvkm_acr *acr, struct nvkm_acr_lsf *rtos)
{
	struct nvkm_acr_lsfw *lsfw;
	u32 offset = 0;
	int ret;

	 
	nvkm_wo32(acr->wpr, 0x200, 0xffffffff);

	 
	list_for_each_entry(lsfw, &acr->lsfw, head) {
		struct lsf_signature_v1 *sig = (void *)lsfw->sig->data;
		struct wpr_header_v1 hdr = {
			.falcon_id = lsfw->id,
			.lsb_offset = lsfw->offset.lsb,
			.bootstrap_owner = NVKM_ACR_LSF_GSPLITE,
			.lazy_bootstrap = 1,
			.bin_version = sig->version,
			.status = WPR_HEADER_V1_STATUS_COPY,
		};

		 
		nvkm_wobj(acr->wpr, offset, &hdr, sizeof(hdr));
		offset += sizeof(hdr);

		 
		ret = gp102_acr_wpr_build_lsb(acr, lsfw);
		if (ret)
			return ret;

		 
		nvkm_wobj(acr->wpr, lsfw->offset.img,
				    lsfw->img.data,
				    lsfw->img.size);

		 
		lsfw->func->bld_write(acr, lsfw->offset.bld, lsfw);
	}

	 
	nvkm_wo32(acr->wpr, offset, WPR_HEADER_V1_FALCON_ID_INVALID);
	return 0;
}

static int
tu102_acr_hsfw_nofw(struct nvkm_acr *acr, const char *bl, const char *fw,
		    const char *name, int version,
		    const struct nvkm_acr_hsf_fwif *fwif)
{
	return 0;
}

MODULE_FIRMWARE("nvidia/tu102/acr/unload_bl.bin");
MODULE_FIRMWARE("nvidia/tu102/acr/ucode_unload.bin");

MODULE_FIRMWARE("nvidia/tu104/acr/unload_bl.bin");
MODULE_FIRMWARE("nvidia/tu104/acr/ucode_unload.bin");

MODULE_FIRMWARE("nvidia/tu106/acr/unload_bl.bin");
MODULE_FIRMWARE("nvidia/tu106/acr/ucode_unload.bin");

MODULE_FIRMWARE("nvidia/tu116/acr/unload_bl.bin");
MODULE_FIRMWARE("nvidia/tu116/acr/ucode_unload.bin");

MODULE_FIRMWARE("nvidia/tu117/acr/unload_bl.bin");
MODULE_FIRMWARE("nvidia/tu117/acr/ucode_unload.bin");

static const struct nvkm_acr_hsf_fwif
tu102_acr_unload_fwif[] = {
	{  0, gm200_acr_hsfw_ctor, &gp108_acr_hsfw_0, NVKM_ACR_HSF_PMU, 0, 0x00000000 },
	{ -1, tu102_acr_hsfw_nofw },
	{}
};

MODULE_FIRMWARE("nvidia/tu102/acr/ucode_asb.bin");
MODULE_FIRMWARE("nvidia/tu104/acr/ucode_asb.bin");
MODULE_FIRMWARE("nvidia/tu106/acr/ucode_asb.bin");
MODULE_FIRMWARE("nvidia/tu116/acr/ucode_asb.bin");
MODULE_FIRMWARE("nvidia/tu117/acr/ucode_asb.bin");

static const struct nvkm_acr_hsf_fwif
tu102_acr_asb_fwif[] = {
	{  0, gm200_acr_hsfw_ctor, &gp108_acr_hsfw_0, NVKM_ACR_HSF_GSP, 0, 0x00000000 },
	{ -1, tu102_acr_hsfw_nofw },
	{}
};

MODULE_FIRMWARE("nvidia/tu102/acr/bl.bin");
MODULE_FIRMWARE("nvidia/tu102/acr/ucode_ahesasc.bin");

MODULE_FIRMWARE("nvidia/tu104/acr/bl.bin");
MODULE_FIRMWARE("nvidia/tu104/acr/ucode_ahesasc.bin");

MODULE_FIRMWARE("nvidia/tu106/acr/bl.bin");
MODULE_FIRMWARE("nvidia/tu106/acr/ucode_ahesasc.bin");

MODULE_FIRMWARE("nvidia/tu116/acr/bl.bin");
MODULE_FIRMWARE("nvidia/tu116/acr/ucode_ahesasc.bin");

MODULE_FIRMWARE("nvidia/tu117/acr/bl.bin");
MODULE_FIRMWARE("nvidia/tu117/acr/ucode_ahesasc.bin");

static const struct nvkm_acr_hsf_fwif
tu102_acr_ahesasc_fwif[] = {
	{  0, gm200_acr_hsfw_ctor, &gp108_acr_load_0, NVKM_ACR_HSF_SEC2, 0, 0x00000000 },
	{ -1, tu102_acr_hsfw_nofw },
	{}
};

static const struct nvkm_acr_func
tu102_acr = {
	.ahesasc = tu102_acr_ahesasc_fwif,
	.asb = tu102_acr_asb_fwif,
	.unload = tu102_acr_unload_fwif,
	.wpr_parse = gp102_acr_wpr_parse,
	.wpr_layout = gp102_acr_wpr_layout,
	.wpr_alloc = gp102_acr_wpr_alloc,
	.wpr_patch = gp102_acr_wpr_patch,
	.wpr_build = tu102_acr_wpr_build,
	.wpr_check = gm200_acr_wpr_check,
	.init = tu102_acr_init,
};

static int
tu102_acr_load(struct nvkm_acr *acr, int version,
	       const struct nvkm_acr_fwif *fwif)
{
	struct nvkm_subdev *subdev = &acr->subdev;
	const struct nvkm_acr_hsf_fwif *hsfwif;

	hsfwif = nvkm_firmware_load(subdev, fwif->func->ahesasc, "AcrAHESASC",
				    acr, "acr/bl", "acr/ucode_ahesasc",
				    "AHESASC");
	if (IS_ERR(hsfwif))
		return PTR_ERR(hsfwif);

	hsfwif = nvkm_firmware_load(subdev, fwif->func->asb, "AcrASB",
				    acr, "acr/bl", "acr/ucode_asb", "ASB");
	if (IS_ERR(hsfwif))
		return PTR_ERR(hsfwif);

	hsfwif = nvkm_firmware_load(subdev, fwif->func->unload, "AcrUnload",
				    acr, "acr/unload_bl", "acr/ucode_unload",
				    "unload");
	if (IS_ERR(hsfwif))
		return PTR_ERR(hsfwif);

	return 0;
}

static const struct nvkm_acr_fwif
tu102_acr_fwif[] = {
	{  0, tu102_acr_load, &tu102_acr },
	{ -1, gm200_acr_nofw, &gm200_acr },
	{}
};

int
tu102_acr_new(struct nvkm_device *device, enum nvkm_subdev_type type, int inst,
	      struct nvkm_acr **pacr)
{
	return nvkm_acr_new_(tu102_acr_fwif, device, type, inst, pacr);
}
