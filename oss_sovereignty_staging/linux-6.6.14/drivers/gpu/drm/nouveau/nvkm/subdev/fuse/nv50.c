 
#include "priv.h"

static u32
nv50_fuse_read(struct nvkm_fuse *fuse, u32 addr)
{
	struct nvkm_device *device = fuse->subdev.device;
	unsigned long flags;
	u32 fuse_enable, val;

	 
	spin_lock_irqsave(&fuse->lock, flags);
	fuse_enable = nvkm_mask(device, 0x001084, 0x800, 0x800);
	val = nvkm_rd32(device, 0x021000 + addr);
	nvkm_wr32(device, 0x001084, fuse_enable);
	spin_unlock_irqrestore(&fuse->lock, flags);
	return val;
}

static const struct nvkm_fuse_func
nv50_fuse = {
	.read = &nv50_fuse_read,
};

int
nv50_fuse_new(struct nvkm_device *device, enum nvkm_subdev_type type, int inst,
	      struct nvkm_fuse **pfuse)
{
	return nvkm_fuse_new_(&nv50_fuse, device, type, inst, pfuse);
}
