 
#include "nv10.h"

static const struct nvkm_gr_func
nv15_gr = {
	.init = nv10_gr_init,
	.intr = nv10_gr_intr,
	.tile = nv10_gr_tile,
	.chan_new = nv10_gr_chan_new,
	.sclass = {
		{ -1, -1, 0x0012, &nv04_gr_object },  
		{ -1, -1, 0x0019, &nv04_gr_object },  
		{ -1, -1, 0x0030, &nv04_gr_object },  
		{ -1, -1, 0x0039, &nv04_gr_object },  
		{ -1, -1, 0x0043, &nv04_gr_object },  
		{ -1, -1, 0x0044, &nv04_gr_object },  
		{ -1, -1, 0x004a, &nv04_gr_object },  
		{ -1, -1, 0x0052, &nv04_gr_object },  
		{ -1, -1, 0x005f, &nv04_gr_object },  
		{ -1, -1, 0x0062, &nv04_gr_object },  
		{ -1, -1, 0x0072, &nv04_gr_object },  
		{ -1, -1, 0x0089, &nv04_gr_object },  
		{ -1, -1, 0x008a, &nv04_gr_object },  
		{ -1, -1, 0x009f, &nv04_gr_object },  
		{ -1, -1, 0x0093, &nv04_gr_object },  
		{ -1, -1, 0x0094, &nv04_gr_object },  
		{ -1, -1, 0x0095, &nv04_gr_object },  
		{ -1, -1, 0x0096, &nv04_gr_object },  
		{}
	}
};

int
nv15_gr_new(struct nvkm_device *device, enum nvkm_subdev_type type, int inst, struct nvkm_gr **pgr)
{
	return nv10_gr_new_(&nv15_gr, device, type, inst, pgr);
}
