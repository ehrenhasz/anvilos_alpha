 
#include "gf100.h"
#include "gk104.h"
#include "ram.h"
#include <subdev/therm.h>
#include <subdev/fb.h>

 

static const struct nvkm_therm_clkgate_init
gk110_fb_clkgate_blcg_init_unk_0[] = {
	{ 0x100d10, 1, 0x0000c242 },
	{ 0x100d30, 1, 0x0000c242 },
	{ 0x100d3c, 1, 0x00000242 },
	{ 0x100d48, 1, 0x0000c242 },
	{ 0x100d1c, 1, 0x00000042 },
	{}
};

static const struct nvkm_therm_clkgate_pack
gk110_fb_clkgate_pack[] = {
	{ gk110_fb_clkgate_blcg_init_unk_0 },
	{ gk104_fb_clkgate_blcg_init_vm_0 },
	{ gk104_fb_clkgate_blcg_init_main_0 },
	{ gk104_fb_clkgate_blcg_init_bcast_0 },
	{}
};

static const struct nvkm_fb_func
gk110_fb = {
	.dtor = gf100_fb_dtor,
	.oneinit = gf100_fb_oneinit,
	.init = gf100_fb_init,
	.init_page = gf100_fb_init_page,
	.intr = gf100_fb_intr,
	.sysmem.flush_page_init = gf100_fb_sysmem_flush_page_init,
	.ram_new = gk104_ram_new,
	.default_bigpage = 17,
	.clkgate_pack = gk110_fb_clkgate_pack,
};

int
gk110_fb_new(struct nvkm_device *device, enum nvkm_subdev_type type, int inst, struct nvkm_fb **pfb)
{
	return gf100_fb_new_(&gk110_fb, device, type, inst, pfb);
}
