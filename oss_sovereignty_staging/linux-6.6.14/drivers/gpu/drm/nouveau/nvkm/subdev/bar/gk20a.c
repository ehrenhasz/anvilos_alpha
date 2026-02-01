 
#include "gf100.h"

static const struct nvkm_bar_func
gk20a_bar_func = {
	.dtor = gf100_bar_dtor,
	.oneinit = gf100_bar_oneinit,
	.bar1.init = gf100_bar_bar1_init,
	.bar1.wait = gf100_bar_bar1_wait,
	.bar1.vmm = gf100_bar_bar1_vmm,
	.flush = g84_bar_flush,
};

int
gk20a_bar_new(struct nvkm_device *device, enum nvkm_subdev_type type, int inst,
	      struct nvkm_bar **pbar)
{
	int ret = gf100_bar_new_(&gk20a_bar_func, device, type, inst, pbar);
	if (ret == 0)
		(*pbar)->iomap_uncached = true;
	return ret;
}
