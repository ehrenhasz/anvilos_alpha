 
#include "priv.h"
#include "user.h"

static const struct nvkm_dma_func
gf119_dma = {
	.class_new = gf119_dmaobj_new,
};

int
gf119_dma_new(struct nvkm_device *device, enum nvkm_subdev_type type, int inst,
	      struct nvkm_dma **pdma)
{
	return nvkm_dma_new_(&gf119_dma, device, type, inst, pdma);
}
