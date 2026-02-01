 
#include "priv.h"
#include "fuc/gk208.fuc5.h"

static const struct nvkm_pmu_func
gk208_pmu = {
	.flcn = &gt215_pmu_flcn,
	.code.data = gk208_pmu_code,
	.code.size = sizeof(gk208_pmu_code),
	.data.data = gk208_pmu_data,
	.data.size = sizeof(gk208_pmu_data),
	.enabled = gf100_pmu_enabled,
	.reset = gf100_pmu_reset,
	.init = gt215_pmu_init,
	.fini = gt215_pmu_fini,
	.intr = gt215_pmu_intr,
	.send = gt215_pmu_send,
	.recv = gt215_pmu_recv,
	.pgob = gk110_pmu_pgob,
};

static const struct nvkm_pmu_fwif
gk208_pmu_fwif[] = {
	{ -1, gf100_pmu_nofw, &gk208_pmu },
	{}
};

int
gk208_pmu_new(struct nvkm_device *device, enum nvkm_subdev_type type, int inst,
	      struct nvkm_pmu **ppmu)
{
	return nvkm_pmu_new_(gk208_pmu_fwif, device, type, inst, ppmu);
}
