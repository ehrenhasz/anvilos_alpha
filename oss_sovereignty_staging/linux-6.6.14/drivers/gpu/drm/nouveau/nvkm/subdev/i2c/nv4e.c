 
#include "priv.h"
#include "pad.h"

static const struct nvkm_i2c_func
nv4e_i2c = {
	.pad_x_new = nv4e_i2c_pad_new,
};

int
nv4e_i2c_new(struct nvkm_device *device, enum nvkm_subdev_type type, int inst,
	     struct nvkm_i2c **pi2c)
{
	return nvkm_i2c_new_(&nv4e_i2c, device, type, inst, pi2c);
}
