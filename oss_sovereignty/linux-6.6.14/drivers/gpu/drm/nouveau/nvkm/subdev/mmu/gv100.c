 
#include "mem.h"
#include "vmm.h"

#include <core/option.h>

#include <nvif/class.h>

static const struct nvkm_mmu_func
gv100_mmu = {
	.dma_bits = 47,
	.mmu = {{ -1, -1, NVIF_CLASS_MMU_GF100}},
	.mem = {{ -1,  0, NVIF_CLASS_MEM_GF100}, gf100_mem_new, gf100_mem_map },
	.vmm = {{ -1,  0, NVIF_CLASS_VMM_GP100}, gv100_vmm_new },
	.kind = gm200_mmu_kind,
	.kind_sys = true,
};

int
gv100_mmu_new(struct nvkm_device *device, enum nvkm_subdev_type type, int inst,
	      struct nvkm_mmu **pmmu)
{
	return nvkm_mmu_new_(&gv100_mmu, device, type, inst, pmmu);
}
