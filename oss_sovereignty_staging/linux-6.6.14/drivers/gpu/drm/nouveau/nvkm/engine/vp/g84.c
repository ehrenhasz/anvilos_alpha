 
#include <engine/vp.h>

#include <nvif/class.h>

static const struct nvkm_xtensa_func
g84_vp = {
	.fifo_val = 0x111,
	.unkd28 = 0x9c544,
	.sclass = {
		{ -1, -1, NV74_VP2 },
		{}
	}
};

int
g84_vp_new(struct nvkm_device *device, enum nvkm_subdev_type type, int inst,
	   struct nvkm_engine **pengine)
{
	return nvkm_xtensa_new_(&g84_vp, device, type, inst, true, 0x00f000, pengine);
}
