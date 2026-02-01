 
#include "pad.h"
#include "aux.h"
#include "bus.h"

static const struct nvkm_i2c_pad_func
gf119_i2c_pad_s_func = {
	.bus_new_4 = gf119_i2c_bus_new,
	.aux_new_6 = gf119_i2c_aux_new,
	.mode = g94_i2c_pad_mode,
};

int
gf119_i2c_pad_s_new(struct nvkm_i2c *i2c, int id, struct nvkm_i2c_pad **ppad)
{
	return nvkm_i2c_pad_new_(&gf119_i2c_pad_s_func, i2c, id, ppad);
}

static const struct nvkm_i2c_pad_func
gf119_i2c_pad_x_func = {
	.bus_new_4 = gf119_i2c_bus_new,
	.aux_new_6 = gf119_i2c_aux_new,
};

int
gf119_i2c_pad_x_new(struct nvkm_i2c *i2c, int id, struct nvkm_i2c_pad **ppad)
{
	return nvkm_i2c_pad_new_(&gf119_i2c_pad_x_func, i2c, id, ppad);
}
