 
#include "priv.h"
#include <core/firmware.h>

static void *
nvkm_nvdec_dtor(struct nvkm_engine *engine)
{
	struct nvkm_nvdec *nvdec = nvkm_nvdec(engine);
	nvkm_falcon_dtor(&nvdec->falcon);
	return nvdec;
}

static const struct nvkm_engine_func
nvkm_nvdec = {
	.dtor = nvkm_nvdec_dtor,
};

int
nvkm_nvdec_new_(const struct nvkm_nvdec_fwif *fwif, struct nvkm_device *device,
		enum nvkm_subdev_type type, int inst, u32 addr, struct nvkm_nvdec **pnvdec)
{
	struct nvkm_nvdec *nvdec;
	int ret;

	if (!(nvdec = *pnvdec = kzalloc(sizeof(*nvdec), GFP_KERNEL)))
		return -ENOMEM;

	ret = nvkm_engine_ctor(&nvkm_nvdec, device, type, inst, true,
			       &nvdec->engine);
	if (ret)
		return ret;

	fwif = nvkm_firmware_load(&nvdec->engine.subdev, fwif, "Nvdec", nvdec);
	if (IS_ERR(fwif))
		return -ENODEV;

	nvdec->func = fwif->func;

	return nvkm_falcon_ctor(nvdec->func->flcn, &nvdec->engine.subdev,
				nvdec->engine.subdev.name, addr, &nvdec->falcon);
};
