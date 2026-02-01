 

#include "priv.h"

static const struct nvkm_falcon_func
gm107_nvenc_flcn = {
};

static const struct nvkm_nvenc_func
gm107_nvenc = {
	.flcn = &gm107_nvenc_flcn,
};

static int
gm107_nvenc_nofw(struct nvkm_nvenc *nvenc, int ver,
		 const struct nvkm_nvenc_fwif *fwif)
{
	return 0;
}

static const struct nvkm_nvenc_fwif
gm107_nvenc_fwif[] = {
	{ -1, gm107_nvenc_nofw, &gm107_nvenc },
	{}
};

int
gm107_nvenc_new(struct nvkm_device *device, enum nvkm_subdev_type type, int inst,
		struct nvkm_nvenc **pnvenc)
{
	return nvkm_nvenc_new_(gm107_nvenc_fwif, device, type, inst, pnvenc);
}
