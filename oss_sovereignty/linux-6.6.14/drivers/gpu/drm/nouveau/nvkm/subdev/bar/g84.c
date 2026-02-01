 
#include "nv50.h"

#include <subdev/timer.h>

void
g84_bar_flush(struct nvkm_bar *bar)
{
	struct nvkm_device *device = bar->subdev.device;
	unsigned long flags;
	spin_lock_irqsave(&bar->lock, flags);
	nvkm_wr32(device, 0x070000, 0x00000001);
	nvkm_msec(device, 2000,
		if (!(nvkm_rd32(device, 0x070000) & 0x00000002))
			break;
	);
	spin_unlock_irqrestore(&bar->lock, flags);
}

static const struct nvkm_bar_func
g84_bar_func = {
	.dtor = nv50_bar_dtor,
	.oneinit = nv50_bar_oneinit,
	.init = nv50_bar_init,
	.bar1.init = nv50_bar_bar1_init,
	.bar1.fini = nv50_bar_bar1_fini,
	.bar1.wait = nv50_bar_bar1_wait,
	.bar1.vmm = nv50_bar_bar1_vmm,
	.bar2.init = nv50_bar_bar2_init,
	.bar2.fini = nv50_bar_bar2_fini,
	.bar2.wait = nv50_bar_bar1_wait,
	.bar2.vmm = nv50_bar_bar2_vmm,
	.flush = g84_bar_flush,
};

int
g84_bar_new(struct nvkm_device *device, enum nvkm_subdev_type type, int inst,
	    struct nvkm_bar **pbar)
{
	return nv50_bar_new_(&g84_bar_func, device, type, inst, 0x200, pbar);
}
