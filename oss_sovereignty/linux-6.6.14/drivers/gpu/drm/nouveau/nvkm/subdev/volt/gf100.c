 
#include "priv.h"

#include <subdev/fuse.h>

static int
gf100_volt_speedo_read(struct nvkm_volt *volt)
{
	struct nvkm_device *device = volt->subdev.device;
	struct nvkm_fuse *fuse = device->fuse;

	if (!fuse)
		return -EINVAL;

	return nvkm_fuse_read(fuse, 0x1cc);
}

int
gf100_volt_oneinit(struct nvkm_volt *volt)
{
	struct nvkm_subdev *subdev = &volt->subdev;
	if (volt->speedo <= 0)
		nvkm_error(subdev, "couldn't find speedo value, volting not "
			   "possible\n");
	return 0;
}

static const struct nvkm_volt_func
gf100_volt = {
	.oneinit = gf100_volt_oneinit,
	.vid_get = nvkm_voltgpio_get,
	.vid_set = nvkm_voltgpio_set,
	.speedo_read = gf100_volt_speedo_read,
};

int
gf100_volt_new(struct nvkm_device *device, enum nvkm_subdev_type type, int inst,
	       struct nvkm_volt **pvolt)
{
	struct nvkm_volt *volt;
	int ret;

	ret = nvkm_volt_new_(&gf100_volt, device, type, inst, &volt);
	*pvolt = volt;
	if (ret)
		return ret;

	return nvkm_voltgpio_init(volt);
}
