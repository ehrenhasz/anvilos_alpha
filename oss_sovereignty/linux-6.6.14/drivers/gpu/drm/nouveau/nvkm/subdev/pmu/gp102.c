 
#include "priv.h"

static const struct nvkm_falcon_func
gp102_pmu_flcn = {
	.disable = gm200_flcn_disable,
	.enable = gm200_flcn_enable,
	.reset_eng = gp102_flcn_reset_eng,
	.reset_wait_mem_scrubbing = gm200_flcn_reset_wait_mem_scrubbing,
	.debug = 0xc08,
	.bind_inst = gm200_pmu_flcn_bind_inst,
	.bind_stat = gm200_flcn_bind_stat,
	.imem_pio = &gm200_flcn_imem_pio,
	.dmem_pio = &gm200_flcn_dmem_pio,
	.start = nvkm_falcon_v1_start,
	.cmdq = { 0x4a0, 0x4b0, 4 },
	.msgq = { 0x4c8, 0x4cc, 0 },
};

static const struct nvkm_pmu_func
gp102_pmu = {
	.flcn = &gp102_pmu_flcn,
};

static const struct nvkm_pmu_fwif
gp102_pmu_fwif[] = {
	{ -1, gm200_pmu_nofw, &gp102_pmu },
	{}
};

int
gp102_pmu_new(struct nvkm_device *device, enum nvkm_subdev_type type, int inst,
	      struct nvkm_pmu **ppmu)
{
	return nvkm_pmu_new_(gp102_pmu_fwif, device, type, inst, ppmu);
}
