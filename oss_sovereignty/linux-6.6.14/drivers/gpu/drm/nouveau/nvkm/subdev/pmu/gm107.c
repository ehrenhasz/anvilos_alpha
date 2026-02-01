 
#include "priv.h"
#define gk208_pmu_code gm107_pmu_code
#define gk208_pmu_data gm107_pmu_data
#include "fuc/gk208.fuc5.h"

static const struct nvkm_pmu_func
gm107_pmu = {
	.flcn = &gt215_pmu_flcn,
	.code.data = gm107_pmu_code,
	.code.size = sizeof(gm107_pmu_code),
	.data.data = gm107_pmu_data,
	.data.size = sizeof(gm107_pmu_data),
	.enabled = gf100_pmu_enabled,
	.reset = gf100_pmu_reset,
	.init = gt215_pmu_init,
	.fini = gt215_pmu_fini,
	.intr = gt215_pmu_intr,
	.send = gt215_pmu_send,
	.recv = gt215_pmu_recv,
};

static const struct nvkm_pmu_fwif
gm107_pmu_fwif[] = {
	{ -1, gf100_pmu_nofw, &gm107_pmu },
	{}
};

int
gm107_pmu_new(struct nvkm_device *device, enum nvkm_subdev_type type, int inst,
	      struct nvkm_pmu **ppmu)
{
	return nvkm_pmu_new_(gm107_pmu_fwif, device, type, inst, ppmu);
}
