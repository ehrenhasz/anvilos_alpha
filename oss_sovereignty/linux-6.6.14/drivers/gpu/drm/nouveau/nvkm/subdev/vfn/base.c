 
#include "priv.h"

static void *
nvkm_vfn_dtor(struct nvkm_subdev *subdev)
{
	return nvkm_vfn(subdev);
}

static const struct nvkm_subdev_func
nvkm_vfn = {
	.dtor = nvkm_vfn_dtor,
};

int
nvkm_vfn_new_(const struct nvkm_vfn_func *func, struct nvkm_device *device,
	      enum nvkm_subdev_type type, int inst, u32 addr, struct nvkm_vfn **pvfn)
{
	struct nvkm_vfn *vfn;
	int ret;

	if (!(vfn = *pvfn = kzalloc(sizeof(*vfn), GFP_KERNEL)))
		return -ENOMEM;

	nvkm_subdev_ctor(&nvkm_vfn, device, type, inst, &vfn->subdev);
	vfn->func = func;
	vfn->addr.priv = addr;
	vfn->addr.user = vfn->addr.priv + func->user.addr;

	if (vfn->func->intr) {
		ret = nvkm_intr_add(vfn->func->intr, vfn->func->intrs,
				    &vfn->subdev, 8, &vfn->intr);
		if (ret)
			return ret;
	}

	vfn->user.ctor = nvkm_uvfn_new;
	vfn->user.base = func->user.base;
	return 0;
}
