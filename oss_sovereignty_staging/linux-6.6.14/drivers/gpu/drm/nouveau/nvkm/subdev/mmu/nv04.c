 
#include "mem.h"
#include "vmm.h"

#include <nvif/class.h>

const struct nvkm_mmu_func
nv04_mmu = {
	.dma_bits = 32,
	.mmu = {{ -1, -1, NVIF_CLASS_MMU_NV04}},
	.mem = {{ -1, -1, NVIF_CLASS_MEM_NV04}, nv04_mem_new, nv04_mem_map },
	.vmm = {{ -1, -1, NVIF_CLASS_VMM_NV04}, nv04_vmm_new, true },
};

int
nv04_mmu_new(struct nvkm_device *device, enum nvkm_subdev_type type, int inst,
	     struct nvkm_mmu **pmmu)
{
	return nvkm_mmu_new_(&nv04_mmu, device, type, inst, pmmu);
}
