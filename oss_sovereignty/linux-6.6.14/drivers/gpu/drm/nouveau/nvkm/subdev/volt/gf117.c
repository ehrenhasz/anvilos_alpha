 
#include "priv.h"

#include <subdev/fuse.h>

static int
gf117_volt_speedo_read(struct nvkm_volt *volt)
{
	struct nvkm_device *device = volt->subdev.device;
	struct nvkm_fuse *fuse = device->fuse;

	if (!fuse)
		return -EINVAL;

	return nvkm_fuse_read(fuse, 0x3a8);
}

static const struct nvkm_volt_func
gf117_volt = {
	.oneinit = gf100_volt_oneinit,
	.vid_get = nvkm_voltgpio_get,
	.vid_set = nvkm_voltgpio_set,
	.speedo_read = gf117_volt_speedo_read,
};

int
gf117_volt_new(struct nvkm_device *device, enum nvkm_subdev_type type, int inst,
	       struct nvkm_volt **pvolt)
{
	struct nvkm_volt *volt;
	int ret;

	ret = nvkm_volt_new_(&gf117_volt, device, type, inst, &volt);
	*pvolt = volt;
	if (ret)
		return ret;

	return nvkm_voltgpio_init(volt);
}
