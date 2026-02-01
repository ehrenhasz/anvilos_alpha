 
#include "priv.h"

static u32
gf100_fuse_read(struct nvkm_fuse *fuse, u32 addr)
{
	struct nvkm_device *device = fuse->subdev.device;
	unsigned long flags;
	u32 fuse_enable, unk, val;

	 
	spin_lock_irqsave(&fuse->lock, flags);
	fuse_enable = nvkm_mask(device, 0x022400, 0x800, 0x800);
	unk = nvkm_mask(device, 0x021000, 0x1, 0x1);
	val = nvkm_rd32(device, 0x021100 + addr);
	nvkm_wr32(device, 0x021000, unk);
	nvkm_wr32(device, 0x022400, fuse_enable);
	spin_unlock_irqrestore(&fuse->lock, flags);
	return val;
}

static const struct nvkm_fuse_func
gf100_fuse = {
	.read = gf100_fuse_read,
};

int
gf100_fuse_new(struct nvkm_device *device, enum nvkm_subdev_type type, int inst,
	       struct nvkm_fuse **pfuse)
{
	return nvkm_fuse_new_(&gf100_fuse, device, type, inst, pfuse);
}
