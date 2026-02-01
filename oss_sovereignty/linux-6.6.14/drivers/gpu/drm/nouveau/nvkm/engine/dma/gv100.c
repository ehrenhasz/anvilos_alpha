 
#include "priv.h"
#include "user.h"

static const struct nvkm_dma_func
gv100_dma = {
	.class_new = gv100_dmaobj_new,
};

int
gv100_dma_new(struct nvkm_device *device, enum nvkm_subdev_type type, int inst,
	      struct nvkm_dma **pdma)
{
	return nvkm_dma_new_(&gv100_dma, device, type, inst, pdma);
}
