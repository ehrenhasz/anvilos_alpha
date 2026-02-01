 
#include "priv.h"
#include "pad.h"

static const struct nvkm_i2c_func
gf119_i2c = {
	.pad_x_new = gf119_i2c_pad_x_new,
	.pad_s_new = gf119_i2c_pad_s_new,
	.aux = 4,
	.aux_stat = g94_aux_stat,
	.aux_mask = g94_aux_mask,
};

int
gf119_i2c_new(struct nvkm_device *device, enum nvkm_subdev_type type, int inst,
	      struct nvkm_i2c **pi2c)
{
	return nvkm_i2c_new_(&gf119_i2c, device, type, inst, pi2c);
}
