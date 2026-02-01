 
#include "priv.h"
#include "fuc/gt215.fuc3.h"

#include <core/client.h>
#include <core/enum.h>
#include <core/gpuobj.h>
#include <engine/fifo.h>

#include <nvif/class.h>

static const struct nvkm_enum
gt215_ce_isr_error_name[] = {
	{ 0x0001, "ILLEGAL_MTHD" },
	{ 0x0002, "INVALID_ENUM" },
	{ 0x0003, "INVALID_BITFIELD" },
	{}
};

void
gt215_ce_intr(struct nvkm_falcon *ce, struct nvkm_chan *chan)
{
	struct nvkm_subdev *subdev = &ce->engine.subdev;
	struct nvkm_device *device = subdev->device;
	const u32 base = subdev->inst * 0x1000;
	u32 ssta = nvkm_rd32(device, 0x104040 + base) & 0x0000ffff;
	u32 addr = nvkm_rd32(device, 0x104040 + base) >> 16;
	u32 mthd = (addr & 0x07ff) << 2;
	u32 subc = (addr & 0x3800) >> 11;
	u32 data = nvkm_rd32(device, 0x104044 + base);
	const struct nvkm_enum *en =
		nvkm_enum_find(gt215_ce_isr_error_name, ssta);

	nvkm_error(subdev, "DISPATCH_ERROR %04x [%s] ch %d [%010llx %s] "
			   "subc %d mthd %04x data %08x\n", ssta,
		   en ? en->name : "", chan ? chan->id : -1,
		   chan ? chan->inst->addr : 0,
		   chan ? chan->name : "unknown",
		   subc, mthd, data);
}

static const struct nvkm_falcon_func
gt215_ce = {
	.code.data = gt215_ce_code,
	.code.size = sizeof(gt215_ce_code),
	.data.data = gt215_ce_data,
	.data.size = sizeof(gt215_ce_data),
	.intr = gt215_ce_intr,
	.sclass = {
		{ -1, -1, GT212_DMA },
		{}
	}
};

int
gt215_ce_new(struct nvkm_device *device, enum nvkm_subdev_type type, int inst,
	     struct nvkm_engine **pengine)
{
	return nvkm_falcon_new_(&gt215_ce, device, type, -1,
				(device->chipset != 0xaf), 0x104000, pengine);
}
