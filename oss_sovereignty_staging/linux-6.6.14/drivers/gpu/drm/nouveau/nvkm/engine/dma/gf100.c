 
#include "priv.h"
#include "user.h"

static const struct nvkm_dma_func
gf100_dma = {
	.class_new = gf100_dmaobj_new,
};

int
gf100_dma_new(struct nvkm_device *device, enum nvkm_subdev_type type, int inst,
	      struct nvkm_dma **pdma)
{
	return nvkm_dma_new_(&gf100_dma, device, type, inst, pdma);
}
