 
#include "priv.h"
#include "pad.h"

static void
gk110_aux_autodpcd(struct nvkm_i2c *i2c, int aux, bool enable)
{
	nvkm_mask(i2c->subdev.device, 0x00e4f8 + (aux * 0x50), 0x00010000, enable << 16);
}

static const struct nvkm_i2c_func
gk110_i2c = {
	.pad_x_new = gf119_i2c_pad_x_new,
	.pad_s_new = gf119_i2c_pad_s_new,
	.aux = 4,
	.aux_stat = gk104_aux_stat,
	.aux_mask = gk104_aux_mask,
	.aux_autodpcd = gk110_aux_autodpcd,
};

int
gk110_i2c_new(struct nvkm_device *device, enum nvkm_subdev_type type, int inst,
	      struct nvkm_i2c **pi2c)
{
	return nvkm_i2c_new_(&gk110_i2c, device, type, inst, pi2c);
}
