 
#define gv100_dmaobj(p) container_of((p), struct gv100_dmaobj, base)
#include "user.h"

#include <core/client.h>
#include <core/gpuobj.h>
#include <subdev/fb.h>

#include <nvif/cl0002.h>
#include <nvif/unpack.h>

struct gv100_dmaobj {
	struct nvkm_dmaobj base;
	u32 flags0;
};

static int
gv100_dmaobj_bind(struct nvkm_dmaobj *base, struct nvkm_gpuobj *parent,
		  int align, struct nvkm_gpuobj **pgpuobj)
{
	struct gv100_dmaobj *dmaobj = gv100_dmaobj(base);
	struct nvkm_device *device = dmaobj->base.dma->engine.subdev.device;
	u64 start = dmaobj->base.start >> 8;
	u64 limit = dmaobj->base.limit >> 8;
	int ret;

	ret = nvkm_gpuobj_new(device, 24, align, false, parent, pgpuobj);
	if (ret == 0) {
		nvkm_kmap(*pgpuobj);
		nvkm_wo32(*pgpuobj, 0x00, dmaobj->flags0);
		nvkm_wo32(*pgpuobj, 0x04, lower_32_bits(start));
		nvkm_wo32(*pgpuobj, 0x08, upper_32_bits(start));
		nvkm_wo32(*pgpuobj, 0x0c, lower_32_bits(limit));
		nvkm_wo32(*pgpuobj, 0x10, upper_32_bits(limit));
		nvkm_done(*pgpuobj);
	}

	return ret;
}

static const struct nvkm_dmaobj_func
gv100_dmaobj_func = {
	.bind = gv100_dmaobj_bind,
};

int
gv100_dmaobj_new(struct nvkm_dma *dma, const struct nvkm_oclass *oclass,
		 void *data, u32 size, struct nvkm_dmaobj **pdmaobj)
{
	union {
		struct gf119_dma_v0 v0;
	} *args;
	struct nvkm_object *parent = oclass->parent;
	struct gv100_dmaobj *dmaobj;
	u32 kind, page;
	int ret;

	if (!(dmaobj = kzalloc(sizeof(*dmaobj), GFP_KERNEL)))
		return -ENOMEM;
	*pdmaobj = &dmaobj->base;

	ret = nvkm_dmaobj_ctor(&gv100_dmaobj_func, dma, oclass,
			       &data, &size, &dmaobj->base);
	if (ret)
		return ret;

	ret  = -ENOSYS;
	args = data;

	nvif_ioctl(parent, "create gv100 dma size %d\n", size);
	if (!(ret = nvif_unpack(ret, &data, &size, args->v0, 0, 0, false))) {
		nvif_ioctl(parent,
			   "create gv100 dma vers %d page %d kind %02x\n",
			   args->v0.version, args->v0.page, args->v0.kind);
		kind = args->v0.kind != 0;
		page = args->v0.page != 0;
	} else
	if (size == 0) {
		kind = 0;
		page = GF119_DMA_V0_PAGE_SP;
	} else
		return ret;

	if (kind)
		dmaobj->flags0 |= 0x00100000;
	if (page)
		dmaobj->flags0 |= 0x00000040;
	dmaobj->flags0 |= 0x00000004;  

	switch (dmaobj->base.target) {
	case NV_MEM_TARGET_VRAM       : dmaobj->flags0 |= 0x00000001; break;
	case NV_MEM_TARGET_PCI        : dmaobj->flags0 |= 0x00000002; break;
	case NV_MEM_TARGET_PCI_NOSNOOP: dmaobj->flags0 |= 0x00000003; break;
	default:
		return -EINVAL;
	}

	return 0;
}
