 
#include "priv.h"
#include "gf100.h"

 
static const struct nvkm_fb_func
gk20a_fb = {
	.dtor = gf100_fb_dtor,
	.oneinit = gf100_fb_oneinit,
	.init = gf100_fb_init,
	.init_page = gf100_fb_init_page,
	.intr = gf100_fb_intr,
	.default_bigpage = 17,
};

int
gk20a_fb_new(struct nvkm_device *device, enum nvkm_subdev_type type, int inst, struct nvkm_fb **pfb)
{
	return gf100_fb_new_(&gk20a_fb, device, type, inst, pfb);
}
