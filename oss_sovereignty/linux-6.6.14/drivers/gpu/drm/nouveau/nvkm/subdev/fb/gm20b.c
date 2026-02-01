 
#include "priv.h"
#include "gf100.h"

 
static const struct nvkm_fb_func
gm20b_fb = {
	.dtor = gf100_fb_dtor,
	.oneinit = gf100_fb_oneinit,
	.init = gm200_fb_init,
	.init_page = gm200_fb_init_page,
	.intr = gf100_fb_intr,
	.sysmem.flush_page_init = gf100_fb_sysmem_flush_page_init,
	.default_bigpage = 0  ,
};

int
gm20b_fb_new(struct nvkm_device *device, enum nvkm_subdev_type type, int inst, struct nvkm_fb **pfb)
{
	return gf100_fb_new_(&gm20b_fb, device, type, inst, pfb);
}
