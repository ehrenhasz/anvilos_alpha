 
#include "mem.h"
#include "vmm.h"

#include <core/option.h>

#include <nvif/class.h>

static const struct nvkm_mmu_func
gp100_mmu = {
	.dma_bits = 47,
	.mmu = {{ -1, -1, NVIF_CLASS_MMU_GF100}},
	.mem = {{ -1,  0, NVIF_CLASS_MEM_GF100}, gf100_mem_new, gf100_mem_map },
	.vmm = {{ -1,  0, NVIF_CLASS_VMM_GP100}, gp100_vmm_new },
	.kind = gm200_mmu_kind,
	.kind_sys = true,
};

int
gp100_mmu_new(struct nvkm_device *device, enum nvkm_subdev_type type, int inst,
	      struct nvkm_mmu **pmmu)
{
	if (!nvkm_boolopt(device->cfgopt, "GP100MmuLayout", true))
		return gm200_mmu_new(device, type, inst, pmmu);
	return nvkm_mmu_new_(&gp100_mmu, device, type, inst, pmmu);
}
