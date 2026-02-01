 
#include "nv50.h"

#include <nvif/class.h>

static const struct nvkm_gr_func
mcp89_gr = {
	.init = nv50_gr_init,
	.intr = nv50_gr_intr,
	.chan_new = nv50_gr_chan_new,
	.tlb_flush = g84_gr_tlb_flush,
	.units = nv50_gr_units,
	.sclass = {
		{ -1, -1, NV_NULL_CLASS, &nv50_gr_object },
		{ -1, -1, NV50_TWOD, &nv50_gr_object },
		{ -1, -1, NV50_MEMORY_TO_MEMORY_FORMAT, &nv50_gr_object },
		{ -1, -1, NV50_COMPUTE, &nv50_gr_object },
		{ -1, -1, GT214_COMPUTE, &nv50_gr_object },
		{ -1, -1, GT21A_TESLA, &nv50_gr_object },
		{}
	}
};

int
mcp89_gr_new(struct nvkm_device *device, enum nvkm_subdev_type type, int inst, struct nvkm_gr **pgr)
{
	return nv50_gr_new_(&mcp89_gr, device, type, inst, pgr);
}
