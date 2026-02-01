 
#include "priv.h"

#include <core/gpuobj.h>
#include <core/object.h>
#include <subdev/timer.h>

#include <nvif/class.h>

 

static int
nv50_mpeg_cclass_bind(struct nvkm_object *object, struct nvkm_gpuobj *parent,
		      int align, struct nvkm_gpuobj **pgpuobj)
{
	int ret = nvkm_gpuobj_new(object->engine->subdev.device, 128 * 4,
				  align, true, parent, pgpuobj);
	if (ret == 0) {
		nvkm_kmap(*pgpuobj);
		nvkm_wo32(*pgpuobj, 0x70, 0x00801ec1);
		nvkm_wo32(*pgpuobj, 0x7c, 0x0000037c);
		nvkm_done(*pgpuobj);
	}
	return ret;
}

const struct nvkm_object_func
nv50_mpeg_cclass = {
	.bind = nv50_mpeg_cclass_bind,
};

 

void
nv50_mpeg_intr(struct nvkm_engine *mpeg)
{
	struct nvkm_subdev *subdev = &mpeg->subdev;
	struct nvkm_device *device = subdev->device;
	u32 stat = nvkm_rd32(device, 0x00b100);
	u32 type = nvkm_rd32(device, 0x00b230);
	u32 mthd = nvkm_rd32(device, 0x00b234);
	u32 data = nvkm_rd32(device, 0x00b238);
	u32 show = stat;

	if (stat & 0x01000000) {
		 
		if (type == 0x00000020 && mthd == 0x0000) {
			nvkm_wr32(device, 0x00b308, 0x00000100);
			show &= ~0x01000000;
		}
	}

	if (show) {
		nvkm_info(subdev, "%08x %08x %08x %08x\n",
			  stat, type, mthd, data);
	}

	nvkm_wr32(device, 0x00b100, stat);
	nvkm_wr32(device, 0x00b230, 0x00000001);
}

int
nv50_mpeg_init(struct nvkm_engine *mpeg)
{
	struct nvkm_subdev *subdev = &mpeg->subdev;
	struct nvkm_device *device = subdev->device;

	nvkm_wr32(device, 0x00b32c, 0x00000000);
	nvkm_wr32(device, 0x00b314, 0x00000100);
	nvkm_wr32(device, 0x00b0e0, 0x0000001a);

	nvkm_wr32(device, 0x00b220, 0x00000044);
	nvkm_wr32(device, 0x00b300, 0x00801ec1);
	nvkm_wr32(device, 0x00b390, 0x00000000);
	nvkm_wr32(device, 0x00b394, 0x00000000);
	nvkm_wr32(device, 0x00b398, 0x00000000);
	nvkm_mask(device, 0x00b32c, 0x00000001, 0x00000001);

	nvkm_wr32(device, 0x00b100, 0xffffffff);
	nvkm_wr32(device, 0x00b140, 0xffffffff);

	if (nvkm_msec(device, 2000,
		if (!(nvkm_rd32(device, 0x00b200) & 0x00000001))
			break;
	) < 0) {
		nvkm_error(subdev, "timeout %08x\n",
			   nvkm_rd32(device, 0x00b200));
		return -EBUSY;
	}

	return 0;
}

static const struct nvkm_engine_func
nv50_mpeg = {
	.init = nv50_mpeg_init,
	.intr = nv50_mpeg_intr,
	.cclass = &nv50_mpeg_cclass,
	.sclass = {
		{ -1, -1, NV31_MPEG, &nv31_mpeg_object },
		{}
	}
};

int
nv50_mpeg_new(struct nvkm_device *device, enum nvkm_subdev_type type, int inst,
	      struct nvkm_engine **pmpeg)
{
	return nvkm_engine_new_(&nv50_mpeg, device, type, inst, true, pmpeg);
}
