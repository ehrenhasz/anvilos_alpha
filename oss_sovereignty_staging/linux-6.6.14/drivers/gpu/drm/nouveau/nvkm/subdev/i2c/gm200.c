 
#include "priv.h"
#include "pad.h"

static void
gm200_aux_autodpcd(struct nvkm_i2c *i2c, int aux, bool enable)
{
	nvkm_mask(i2c->subdev.device, 0x00d968 + (aux * 0x50), 0x00010000, enable << 16);
}

static const struct nvkm_i2c_func
gm200_i2c = {
	.pad_x_new = gf119_i2c_pad_x_new,
	.pad_s_new = gm200_i2c_pad_s_new,
	.aux = 8,
	.aux_stat = gk104_aux_stat,
	.aux_mask = gk104_aux_mask,
	.aux_autodpcd = gm200_aux_autodpcd,
};

int
gm200_i2c_new(struct nvkm_device *device, enum nvkm_subdev_type type, int inst,
	      struct nvkm_i2c **pi2c)
{
	return nvkm_i2c_new_(&gm200_i2c, device, type, inst, pi2c);
}
