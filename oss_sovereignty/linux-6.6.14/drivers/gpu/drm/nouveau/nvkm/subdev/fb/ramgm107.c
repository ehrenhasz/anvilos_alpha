 
#include "ram.h"

u32
gm107_ram_probe_fbp(const struct nvkm_ram_func *func,
		    struct nvkm_device *device, int fbp, int *pltcs)
{
	u32 fbpao = nvkm_rd32(device, 0x021c14);
	return func->probe_fbp_amount(func, fbpao, device, fbp, pltcs);
}

static const struct nvkm_ram_func
gm107_ram = {
	.upper = 0x1000000000ULL,
	.probe_fbp = gm107_ram_probe_fbp,
	.probe_fbp_amount = gf108_ram_probe_fbp_amount,
	.probe_fbpa_amount = gf100_ram_probe_fbpa_amount,
	.dtor = gk104_ram_dtor,
	.init = gk104_ram_init,
	.calc = gk104_ram_calc,
	.prog = gk104_ram_prog,
	.tidy = gk104_ram_tidy,
};

int
gm107_ram_new(struct nvkm_fb *fb, struct nvkm_ram **pram)
{
	return gk104_ram_new_(&gm107_ram, fb, pram);
}
