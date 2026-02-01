 
#include "nv50.h"

static const struct nvkm_clk_func
g84_clk = {
	.read = nv50_clk_read,
	.calc = nv50_clk_calc,
	.prog = nv50_clk_prog,
	.tidy = nv50_clk_tidy,
	.domains = {
		{ nv_clk_src_crystal, 0xff },
		{ nv_clk_src_href   , 0xff },
		{ nv_clk_src_core   , 0xff, 0, "core", 1000 },
		{ nv_clk_src_shader , 0xff, 0, "shader", 1000 },
		{ nv_clk_src_mem    , 0xff, 0, "memory", 1000 },
		{ nv_clk_src_vdec   , 0xff },
		{ nv_clk_src_max }
	}
};

int
g84_clk_new(struct nvkm_device *device, enum nvkm_subdev_type type, int inst,
	    struct nvkm_clk **pclk)
{
	return nv50_clk_new_(&g84_clk, device, type, inst, (device->chipset >= 0x94), pclk);
}
