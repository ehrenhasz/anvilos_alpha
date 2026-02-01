 
#include "priv.h"

#include <nvif/class.h>

static const struct nvkm_engine_func
ga102_ce = {
	.oneinit = ga100_ce_oneinit,
	.init = ga100_ce_init,
	.fini = ga100_ce_fini,
	.nonstall = ga100_ce_nonstall,
	.cclass = &gv100_ce_cclass,
	.sclass = {
		{ -1, -1, AMPERE_DMA_COPY_A },
		{ -1, -1, AMPERE_DMA_COPY_B },
		{}
	}
};

int
ga102_ce_new(struct nvkm_device *device, enum nvkm_subdev_type type, int inst,
	     struct nvkm_engine **pengine)
{
	return nvkm_engine_new_(&ga102_ce, device, type, inst, true, pengine);
}
