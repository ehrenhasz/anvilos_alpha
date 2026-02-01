 
#include "priv.h"

#include <core/firmware.h>

static void *
nvkm_nvenc_dtor(struct nvkm_engine *engine)
{
	struct nvkm_nvenc *nvenc = nvkm_nvenc(engine);
	nvkm_falcon_dtor(&nvenc->falcon);
	return nvenc;
}

static const struct nvkm_engine_func
nvkm_nvenc = {
	.dtor = nvkm_nvenc_dtor,
};

int
nvkm_nvenc_new_(const struct nvkm_nvenc_fwif *fwif, struct nvkm_device *device,
		enum nvkm_subdev_type type, int inst, struct nvkm_nvenc **pnvenc)
{
	struct nvkm_nvenc *nvenc;
	int ret;

	if (!(nvenc = *pnvenc = kzalloc(sizeof(*nvenc), GFP_KERNEL)))
		return -ENOMEM;

	ret = nvkm_engine_ctor(&nvkm_nvenc, device, type, inst, true,
			       &nvenc->engine);
	if (ret)
		return ret;

	fwif = nvkm_firmware_load(&nvenc->engine.subdev, fwif, "Nvenc", nvenc);
	if (IS_ERR(fwif))
		return -ENODEV;

	nvenc->func = fwif->func;

	return nvkm_falcon_ctor(nvenc->func->flcn, &nvenc->engine.subdev,
				nvenc->engine.subdev.name, 0, &nvenc->falcon);
};
