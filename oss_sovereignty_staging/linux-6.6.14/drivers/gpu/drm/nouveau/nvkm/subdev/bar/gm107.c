 
#include "gf100.h"

#include <subdev/timer.h>

void
gm107_bar_bar1_wait(struct nvkm_bar *bar)
{
	struct nvkm_device *device = bar->subdev.device;
	nvkm_msec(device, 2000,
		if (!(nvkm_rd32(device, 0x001710) & 0x00000003))
			break;
	);
}

static void
gm107_bar_bar2_wait(struct nvkm_bar *bar)
{
	struct nvkm_device *device = bar->subdev.device;
	nvkm_msec(device, 2000,
		if (!(nvkm_rd32(device, 0x001710) & 0x0000000c))
			break;
	);
}

static const struct nvkm_bar_func
gm107_bar_func = {
	.dtor = gf100_bar_dtor,
	.oneinit = gf100_bar_oneinit,
	.bar1.init = gf100_bar_bar1_init,
	.bar1.fini = gf100_bar_bar1_fini,
	.bar1.wait = gm107_bar_bar1_wait,
	.bar1.vmm = gf100_bar_bar1_vmm,
	.bar2.init = gf100_bar_bar2_init,
	.bar2.fini = gf100_bar_bar2_fini,
	.bar2.wait = gm107_bar_bar2_wait,
	.bar2.vmm = gf100_bar_bar2_vmm,
	.flush = g84_bar_flush,
};

int
gm107_bar_new(struct nvkm_device *device, enum nvkm_subdev_type type, int inst,
	      struct nvkm_bar **pbar)
{
	return gf100_bar_new_(&gm107_bar_func, device, type, inst, pbar);
}
