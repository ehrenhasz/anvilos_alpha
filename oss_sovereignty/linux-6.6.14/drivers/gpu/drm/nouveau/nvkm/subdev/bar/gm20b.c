 
#include "gf100.h"

static const struct nvkm_bar_func
gm20b_bar_func = {
	.dtor = gf100_bar_dtor,
	.oneinit = gf100_bar_oneinit,
	.bar1.init = gf100_bar_bar1_init,
	.bar1.wait = gm107_bar_bar1_wait,
	.bar1.vmm = gf100_bar_bar1_vmm,
	.flush = g84_bar_flush,
};

int
gm20b_bar_new(struct nvkm_device *device, enum nvkm_subdev_type type, int inst,
	      struct nvkm_bar **pbar)
{
	int ret = gf100_bar_new_(&gm20b_bar_func, device, type, inst, pbar);
	if (ret == 0)
		(*pbar)->iomap_uncached = true;
	return ret;
}
