 
#include "priv.h"

#include <core/gpuobj.h>
#include <core/object.h>

#include <nvif/class.h>

static int
gv100_ce_cclass_bind(struct nvkm_object *object, struct nvkm_gpuobj *parent, int align,
		     struct nvkm_gpuobj **pgpuobj)
{
	struct nvkm_device *device = object->engine->subdev.device;
	u32 size;

	 
	size = nvkm_rd32(device, 0x104028);  
	size = 27 * 5 * (((9 + 1 + 3) * hweight32(size)) + 2);
	size = roundup(size, PAGE_SIZE);

	return nvkm_gpuobj_new(device, size, align, true, parent, pgpuobj);
}

const struct nvkm_object_func
gv100_ce_cclass = {
	.bind = gv100_ce_cclass_bind,
};

static const struct nvkm_engine_func
gv100_ce = {
	.intr = gp100_ce_intr,
	.cclass = &gv100_ce_cclass,
	.sclass = {
		{ -1, -1, VOLTA_DMA_COPY_A },
		{}
	}
};

int
gv100_ce_new(struct nvkm_device *device, enum nvkm_subdev_type type, int inst,
	     struct nvkm_engine **pengine)
{
	return nvkm_engine_new_(&gv100_ce, device, type, inst, true, pengine);
}
