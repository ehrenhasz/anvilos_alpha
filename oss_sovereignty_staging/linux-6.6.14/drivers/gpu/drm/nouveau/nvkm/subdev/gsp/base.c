 
#include "priv.h"
#include <core/falcon.h>
#include <core/firmware.h>
#include <subdev/acr.h>
#include <subdev/top.h>

static void *
nvkm_gsp_dtor(struct nvkm_subdev *subdev)
{
	struct nvkm_gsp *gsp = nvkm_gsp(subdev);
	nvkm_falcon_dtor(&gsp->falcon);
	return gsp;
}

static const struct nvkm_subdev_func
nvkm_gsp = {
	.dtor = nvkm_gsp_dtor,
};

int
nvkm_gsp_new_(const struct nvkm_gsp_fwif *fwif, struct nvkm_device *device,
	      enum nvkm_subdev_type type, int inst, struct nvkm_gsp **pgsp)
{
	struct nvkm_gsp *gsp;

	if (!(gsp = *pgsp = kzalloc(sizeof(*gsp), GFP_KERNEL)))
		return -ENOMEM;

	nvkm_subdev_ctor(&nvkm_gsp, device, type, inst, &gsp->subdev);

	fwif = nvkm_firmware_load(&gsp->subdev, fwif, "Gsp", gsp);
	if (IS_ERR(fwif))
		return PTR_ERR(fwif);

	gsp->func = fwif->func;

	return nvkm_falcon_ctor(gsp->func->flcn, &gsp->subdev, gsp->subdev.name, 0, &gsp->falcon);
}
