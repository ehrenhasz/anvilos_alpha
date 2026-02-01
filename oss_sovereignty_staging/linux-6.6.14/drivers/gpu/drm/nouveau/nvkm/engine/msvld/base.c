 
#include "priv.h"

int
nvkm_msvld_new_(const struct nvkm_falcon_func *func, struct nvkm_device *device,
		enum nvkm_subdev_type type, int inst, struct nvkm_engine **pengine)
{
	return nvkm_falcon_new_(func, device, type, inst, true, 0x084000, pengine);
}
