 
#include "priv.h"

static u32
gm107_fuse_read(struct nvkm_fuse *fuse, u32 addr)
{
	struct nvkm_device *device = fuse->subdev.device;
	return nvkm_rd32(device, 0x021100 + addr);
}

static const struct nvkm_fuse_func
gm107_fuse = {
	.read = gm107_fuse_read,
};

int
gm107_fuse_new(struct nvkm_device *device, enum nvkm_subdev_type type, int inst,
	       struct nvkm_fuse **pfuse)
{
	return nvkm_fuse_new_(&gm107_fuse, device, type, inst, pfuse);
}
