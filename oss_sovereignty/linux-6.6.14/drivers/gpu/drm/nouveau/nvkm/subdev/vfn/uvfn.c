 
#define nvkm_uvfn(p) container_of((p), struct nvkm_uvfn, object)
#include "priv.h"

#include <core/object.h>

struct nvkm_uvfn {
	struct nvkm_object object;
	struct nvkm_vfn *vfn;
};

static int
nvkm_uvfn_map(struct nvkm_object *object, void *argv, u32 argc,
	      enum nvkm_object_map *type, u64 *addr, u64 *size)
{
	struct nvkm_vfn *vfn = nvkm_uvfn(object)->vfn;
	struct nvkm_device *device = vfn->subdev.device;

	*addr = device->func->resource_addr(device, 0) + vfn->addr.user;
	*size = vfn->func->user.size;
	*type = NVKM_OBJECT_MAP_IO;
	return 0;
}

static const struct nvkm_object_func
nvkm_uvfn = {
	.map = nvkm_uvfn_map,
};

int
nvkm_uvfn_new(struct nvkm_device *device, const struct nvkm_oclass *oclass,
	      void *argv, u32 argc, struct nvkm_object **pobject)
{
	struct nvkm_uvfn *uvfn;

	if (argc != 0)
		return -ENOSYS;

	if (!(uvfn = kzalloc(sizeof(*uvfn), GFP_KERNEL)))
		return -ENOMEM;

	nvkm_object_ctor(&nvkm_uvfn, oclass, &uvfn->object);
	uvfn->vfn = device->vfn;

	*pobject = &uvfn->object;
	return 0;
}
