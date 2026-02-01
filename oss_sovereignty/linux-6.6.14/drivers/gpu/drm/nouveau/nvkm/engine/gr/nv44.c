 
#include "nv40.h"
#include "regs.h"

#include <subdev/fb.h>
#include <engine/fifo.h>

static void
nv44_gr_tile(struct nvkm_gr *base, int i, struct nvkm_fb_tile *tile)
{
	struct nv40_gr *gr = nv40_gr(base);
	struct nvkm_device *device = gr->base.engine.subdev.device;
	struct nvkm_fifo *fifo = device->fifo;
	unsigned long flags;

	nvkm_fifo_pause(fifo, &flags);
	nv04_gr_idle(&gr->base);

	switch (device->chipset) {
	case 0x44:
	case 0x4a:
		nvkm_wr32(device, NV20_PGRAPH_TSIZE(i), tile->pitch);
		nvkm_wr32(device, NV20_PGRAPH_TLIMIT(i), tile->limit);
		nvkm_wr32(device, NV20_PGRAPH_TILE(i), tile->addr);
		break;
	case 0x46:
	case 0x4c:
	case 0x63:
	case 0x67:
	case 0x68:
		nvkm_wr32(device, NV47_PGRAPH_TSIZE(i), tile->pitch);
		nvkm_wr32(device, NV47_PGRAPH_TLIMIT(i), tile->limit);
		nvkm_wr32(device, NV47_PGRAPH_TILE(i), tile->addr);
		nvkm_wr32(device, NV40_PGRAPH_TSIZE1(i), tile->pitch);
		nvkm_wr32(device, NV40_PGRAPH_TLIMIT1(i), tile->limit);
		nvkm_wr32(device, NV40_PGRAPH_TILE1(i), tile->addr);
		break;
	case 0x4e:
		nvkm_wr32(device, NV20_PGRAPH_TSIZE(i), tile->pitch);
		nvkm_wr32(device, NV20_PGRAPH_TLIMIT(i), tile->limit);
		nvkm_wr32(device, NV20_PGRAPH_TILE(i), tile->addr);
		nvkm_wr32(device, NV40_PGRAPH_TSIZE1(i), tile->pitch);
		nvkm_wr32(device, NV40_PGRAPH_TLIMIT1(i), tile->limit);
		nvkm_wr32(device, NV40_PGRAPH_TILE1(i), tile->addr);
		break;
	default:
		WARN_ON(1);
		break;
	}

	nvkm_fifo_start(fifo, &flags);
}

static const struct nvkm_gr_func
nv44_gr = {
	.init = nv40_gr_init,
	.intr = nv40_gr_intr,
	.tile = nv44_gr_tile,
	.units = nv40_gr_units,
	.chan_new = nv40_gr_chan_new,
	.sclass = {
		{ -1, -1, 0x0012, &nv40_gr_object },  
		{ -1, -1, 0x0019, &nv40_gr_object },  
		{ -1, -1, 0x0030, &nv40_gr_object },  
		{ -1, -1, 0x0039, &nv40_gr_object },  
		{ -1, -1, 0x0043, &nv40_gr_object },  
		{ -1, -1, 0x0044, &nv40_gr_object },  
		{ -1, -1, 0x004a, &nv40_gr_object },  
		{ -1, -1, 0x0062, &nv40_gr_object },  
		{ -1, -1, 0x0072, &nv40_gr_object },  
		{ -1, -1, 0x0089, &nv40_gr_object },  
		{ -1, -1, 0x008a, &nv40_gr_object },  
		{ -1, -1, 0x009f, &nv40_gr_object },  
		{ -1, -1, 0x3062, &nv40_gr_object },  
		{ -1, -1, 0x3089, &nv40_gr_object },  
		{ -1, -1, 0x309e, &nv40_gr_object },  
		{ -1, -1, 0x4497, &nv40_gr_object },  
		{}
	}
};

int
nv44_gr_new(struct nvkm_device *device, enum nvkm_subdev_type type, int inst, struct nvkm_gr **pgr)
{
	return nv40_gr_new_(&nv44_gr, device, type, inst, pgr);
}
