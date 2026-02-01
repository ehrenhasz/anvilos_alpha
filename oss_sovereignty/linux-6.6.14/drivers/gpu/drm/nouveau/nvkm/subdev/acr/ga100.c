 
#include "priv.h"

void
ga100_acr_wpr_check(struct nvkm_acr *acr, u64 *start, u64 *limit)
{
	struct nvkm_device *device = acr->subdev.device;

	*start = (u64)(nvkm_rd32(device, 0x1fa81c) & 0xffffff00) << 8;
	*limit = (u64)(nvkm_rd32(device, 0x1fa820) & 0xffffff00) << 8;
	*limit = *limit + 0x20000;
}

int
ga100_acr_hsfw_ctor(struct nvkm_acr *acr, const char *bl, const char *fw,
		    const char *name, int ver, const struct nvkm_acr_hsf_fwif *fwif)
{
	struct nvkm_acr_hsfw *hsfw;

	if (!(hsfw = kzalloc(sizeof(*hsfw), GFP_KERNEL)))
		return -ENOMEM;

	hsfw->falcon_id = fwif->falcon_id;
	hsfw->boot_mbox0 = fwif->boot_mbox0;
	hsfw->intr_clear = fwif->intr_clear;
	list_add_tail(&hsfw->head, &acr->hsfw);

	return nvkm_falcon_fw_ctor_hs_v2(fwif->func, name, &acr->subdev, fw, ver, NULL, &hsfw->fw);
}
