 
#include "priv.h"
#include "fuc/gf100.fuc3.h"

#include <subdev/mc.h>

void
gf100_pmu_reset(struct nvkm_pmu *pmu)
{
	struct nvkm_device *device = pmu->subdev.device;
	nvkm_mc_disable(device, NVKM_SUBDEV_PMU, 0);
	nvkm_mc_enable(device, NVKM_SUBDEV_PMU, 0);
}

bool
gf100_pmu_enabled(struct nvkm_pmu *pmu)
{
	return nvkm_mc_enabled(pmu->subdev.device, NVKM_SUBDEV_PMU, 0);
}

static const struct nvkm_pmu_func
gf100_pmu = {
	.flcn = &gt215_pmu_flcn,
	.code.data = gf100_pmu_code,
	.code.size = sizeof(gf100_pmu_code),
	.data.data = gf100_pmu_data,
	.data.size = sizeof(gf100_pmu_data),
	.enabled = gf100_pmu_enabled,
	.reset = gf100_pmu_reset,
	.init = gt215_pmu_init,
	.fini = gt215_pmu_fini,
	.intr = gt215_pmu_intr,
	.send = gt215_pmu_send,
	.recv = gt215_pmu_recv,
};

int
gf100_pmu_nofw(struct nvkm_pmu *pmu, int ver, const struct nvkm_pmu_fwif *fwif)
{
	return 0;
}

static const struct nvkm_pmu_fwif
gf100_pmu_fwif[] = {
	{ -1, gf100_pmu_nofw, &gf100_pmu },
	{}
};

int
gf100_pmu_new(struct nvkm_device *device, enum nvkm_subdev_type type, int inst,
	      struct nvkm_pmu **ppmu)
{
	return nvkm_pmu_new_(gf100_pmu_fwif, device, type, inst, ppmu);
}
