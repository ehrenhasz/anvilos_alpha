 
#include "priv.h"
#include "user.h"

static const struct nvkm_dma_func
nv50_dma = {
	.class_new = nv50_dmaobj_new,
};

int
nv50_dma_new(struct nvkm_device *device, enum nvkm_subdev_type type, int inst,
	     struct nvkm_dma **pdma)
{
	return nvkm_dma_new_(&nv50_dma, device, type, inst, pdma);
}
