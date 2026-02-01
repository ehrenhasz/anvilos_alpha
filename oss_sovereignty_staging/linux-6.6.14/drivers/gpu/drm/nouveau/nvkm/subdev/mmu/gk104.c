 
#include "mem.h"
#include "vmm.h"

#include <nvif/class.h>

static const struct nvkm_mmu_func
gk104_mmu = {
	.dma_bits = 40,
	.mmu = {{ -1, -1, NVIF_CLASS_MMU_GF100}},
	.mem = {{ -1,  0, NVIF_CLASS_MEM_GF100}, gf100_mem_new, gf100_mem_map },
	.vmm = {{ -1, -1, NVIF_CLASS_VMM_GF100}, gk104_vmm_new },
	.kind = gf100_mmu_kind,
	.kind_sys = true,
};

int
gk104_mmu_new(struct nvkm_device *device, enum nvkm_subdev_type type, int inst,
	      struct nvkm_mmu **pmmu)
{
	return nvkm_mmu_new_(&gk104_mmu, device, type, inst, pmmu);
}
