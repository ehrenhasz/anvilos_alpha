 
#include "priv.h"

static int
gm200_pmu_flcn_bind_stat(struct nvkm_falcon *falcon, bool intr)
{
	nvkm_falcon_wr32(falcon, 0x200, 0x0000030e);
	return (nvkm_falcon_rd32(falcon, 0x20c) & 0x00007000) >> 12;
}

void
gm200_pmu_flcn_bind_inst(struct nvkm_falcon *falcon, int target, u64 addr)
{
	nvkm_falcon_wr32(falcon, 0xe00, 4);  
	nvkm_falcon_wr32(falcon, 0xe04, 0);  
	nvkm_falcon_wr32(falcon, 0xe08, 4);  
	nvkm_falcon_wr32(falcon, 0xe0c, 5);  
	nvkm_falcon_wr32(falcon, 0xe10, 6);  
	nvkm_falcon_mask(falcon, 0x090, 0x00010000, 0x00010000);
	nvkm_falcon_wr32(falcon, 0x480, (1 << 30) | (target << 28) | (addr >> 12));
}

const struct nvkm_falcon_func
gm200_pmu_flcn = {
	.disable = gm200_flcn_disable,
	.enable = gm200_flcn_enable,
	.reset_pmc = true,
	.reset_wait_mem_scrubbing = gm200_flcn_reset_wait_mem_scrubbing,
	.debug = 0xc08,
	.bind_inst = gm200_pmu_flcn_bind_inst,
	.bind_stat = gm200_pmu_flcn_bind_stat,
	.imem_pio = &gm200_flcn_imem_pio,
	.dmem_pio = &gm200_flcn_dmem_pio,
	.start = nvkm_falcon_v1_start,
	.cmdq = { 0x4a0, 0x4b0, 4 },
	.msgq = { 0x4c8, 0x4cc, 0 },
};

static const struct nvkm_pmu_func
gm200_pmu = {
	.flcn = &gm200_pmu_flcn,
	.reset = gf100_pmu_reset,
};

int
gm200_pmu_nofw(struct nvkm_pmu *pmu, int ver, const struct nvkm_pmu_fwif *fwif)
{
	nvkm_warn(&pmu->subdev, "firmware unavailable\n");
	return 0;
}

static const struct nvkm_pmu_fwif
gm200_pmu_fwif[] = {
	{ -1, gm200_pmu_nofw, &gm200_pmu },
	{}
};

int
gm200_pmu_new(struct nvkm_device *device, enum nvkm_subdev_type type, int inst,
	      struct nvkm_pmu **ppmu)
{
	return nvkm_pmu_new_(gm200_pmu_fwif, device, type, inst, ppmu);
}
