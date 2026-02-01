 
#include "priv.h"
#include "fuc/gf119.fuc4.h"

static const struct nvkm_pmu_func
gf119_pmu = {
	.flcn = &gt215_pmu_flcn,
	.code.data = gf119_pmu_code,
	.code.size = sizeof(gf119_pmu_code),
	.data.data = gf119_pmu_data,
	.data.size = sizeof(gf119_pmu_data),
	.enabled = gf100_pmu_enabled,
	.reset = gf100_pmu_reset,
	.init = gt215_pmu_init,
	.fini = gt215_pmu_fini,
	.intr = gt215_pmu_intr,
	.send = gt215_pmu_send,
	.recv = gt215_pmu_recv,
};

static const struct nvkm_pmu_fwif
gf119_pmu_fwif[] = {
	{ -1, gf100_pmu_nofw, &gf119_pmu },
	{}
};

int
gf119_pmu_new(struct nvkm_device *device, enum nvkm_subdev_type type, int inst,
	      struct nvkm_pmu **ppmu)
{
	return nvkm_pmu_new_(gf119_pmu_fwif, device, type, inst, ppmu);
}
