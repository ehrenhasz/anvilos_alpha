 
#include "priv.h"

u32
nvkm_fuse_read(struct nvkm_fuse *fuse, u32 addr)
{
	return fuse->func->read(fuse, addr);
}

static void *
nvkm_fuse_dtor(struct nvkm_subdev *subdev)
{
	return nvkm_fuse(subdev);
}

static const struct nvkm_subdev_func
nvkm_fuse = {
	.dtor = nvkm_fuse_dtor,
};

int
nvkm_fuse_new_(const struct nvkm_fuse_func *func, struct nvkm_device *device,
	       enum nvkm_subdev_type type, int inst, struct nvkm_fuse **pfuse)
{
	struct nvkm_fuse *fuse;
	if (!(fuse = *pfuse = kzalloc(sizeof(*fuse), GFP_KERNEL)))
		return -ENOMEM;
	nvkm_subdev_ctor(&nvkm_fuse, device, type, inst, &fuse->subdev);
	fuse->func = func;
	spin_lock_init(&fuse->lock);
	return 0;
}
