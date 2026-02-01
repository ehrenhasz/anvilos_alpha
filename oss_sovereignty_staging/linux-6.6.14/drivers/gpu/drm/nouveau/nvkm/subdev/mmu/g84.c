 
#include "mem.h"
#include "vmm.h"

#include <nvif/class.h>

static const struct nvkm_mmu_func
g84_mmu = {
	.dma_bits = 40,
	.mmu = {{ -1, -1, NVIF_CLASS_MMU_NV50}},
	.mem = {{ -1,  0, NVIF_CLASS_MEM_NV50}, nv50_mem_new, nv50_mem_map },
	.vmm = {{ -1, -1, NVIF_CLASS_VMM_NV50}, nv50_vmm_new, false, 0x0200 },
	.kind = nv50_mmu_kind,
	.kind_sys = true,
};

int
g84_mmu_new(struct nvkm_device *device, enum nvkm_subdev_type type, int inst,
	    struct nvkm_mmu **pmmu)
{
	return nvkm_mmu_new_(&g84_mmu, device, type, inst, pmmu);
}
