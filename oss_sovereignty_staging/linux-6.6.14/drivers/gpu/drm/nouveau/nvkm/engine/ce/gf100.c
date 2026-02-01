 
#include "priv.h"
#include "fuc/gf100.fuc3.h"

#include <nvif/class.h>

static void
gf100_ce_init(struct nvkm_falcon *ce)
{
	nvkm_wr32(ce->engine.subdev.device, ce->addr + 0x084, ce->engine.subdev.inst);
}

static const struct nvkm_falcon_func
gf100_ce0 = {
	.code.data = gf100_ce_code,
	.code.size = sizeof(gf100_ce_code),
	.data.data = gf100_ce_data,
	.data.size = sizeof(gf100_ce_data),
	.init = gf100_ce_init,
	.intr = gt215_ce_intr,
	.sclass = {
		{ -1, -1, FERMI_DMA },
		{}
	}
};

static const struct nvkm_falcon_func
gf100_ce1 = {
	.code.data = gf100_ce_code,
	.code.size = sizeof(gf100_ce_code),
	.data.data = gf100_ce_data,
	.data.size = sizeof(gf100_ce_data),
	.init = gf100_ce_init,
	.intr = gt215_ce_intr,
	.sclass = {
		{ -1, -1, FERMI_DECOMPRESS },
		{}
	}
};

int
gf100_ce_new(struct nvkm_device *device, enum nvkm_subdev_type type, int inst,
	     struct nvkm_engine **pengine)
{
	return nvkm_falcon_new_(inst ? &gf100_ce1 : &gf100_ce0, device, type, inst, true,
				0x104000 + (inst * 0x1000), pengine);
}
