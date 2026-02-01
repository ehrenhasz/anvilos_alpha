 
#include "gk104.h"
#include "gf100.h"
#include "ram.h"

 
const struct nvkm_therm_clkgate_init
gk104_fb_clkgate_blcg_init_unk_0[] = {
	{ 0x100d10, 1, 0x0000c244 },
	{ 0x100d30, 1, 0x0000c242 },
	{ 0x100d3c, 1, 0x00000242 },
	{ 0x100d48, 1, 0x00000242 },
	{ 0x100d1c, 1, 0x00000042 },
	{}
};

const struct nvkm_therm_clkgate_init
gk104_fb_clkgate_blcg_init_vm_0[] = {
	{ 0x100c98, 1, 0x00000242 },
	{}
};

const struct nvkm_therm_clkgate_init
gk104_fb_clkgate_blcg_init_main_0[] = {
	{ 0x10f000, 1, 0x00000042 },
	{ 0x17e030, 1, 0x00000044 },
	{ 0x17e040, 1, 0x00000044 },
	{}
};

const struct nvkm_therm_clkgate_init
gk104_fb_clkgate_blcg_init_bcast_0[] = {
	{ 0x17ea60, 4, 0x00000044 },
	{}
};

static const struct nvkm_therm_clkgate_pack
gk104_fb_clkgate_pack[] = {
	{ gk104_fb_clkgate_blcg_init_unk_0 },
	{ gk104_fb_clkgate_blcg_init_vm_0 },
	{ gk104_fb_clkgate_blcg_init_main_0 },
	{ gk104_fb_clkgate_blcg_init_bcast_0 },
	{}
};

static const struct nvkm_fb_func
gk104_fb = {
	.dtor = gf100_fb_dtor,
	.oneinit = gf100_fb_oneinit,
	.init = gf100_fb_init,
	.init_page = gf100_fb_init_page,
	.intr = gf100_fb_intr,
	.sysmem.flush_page_init = gf100_fb_sysmem_flush_page_init,
	.ram_new = gk104_ram_new,
	.default_bigpage = 17,
	.clkgate_pack = gk104_fb_clkgate_pack,
};

int
gk104_fb_new(struct nvkm_device *device, enum nvkm_subdev_type type, int inst, struct nvkm_fb **pfb)
{
	return gf100_fb_new_(&gk104_fb, device, type, inst, pfb);
}
