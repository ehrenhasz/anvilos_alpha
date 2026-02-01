 
#include <engine/bsp.h>

#include <nvif/class.h>

static const struct nvkm_xtensa_func
g84_bsp = {
	.fifo_val = 0x1111,
	.unkd28 = 0x90044,
	.sclass = {
		{ -1, -1, NV74_BSP },
		{}
	}
};

int
g84_bsp_new(struct nvkm_device *device, enum nvkm_subdev_type type, int inst,
	    struct nvkm_engine **pengine)
{
	return nvkm_xtensa_new_(&g84_bsp, device, type, inst,
				device->chipset != 0x92, 0x103000, pengine);
}
