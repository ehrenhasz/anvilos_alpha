 
#include "mem.h"
#include "vmm.h"

#include <subdev/fb.h>

#include <nvif/class.h>

static const struct nvkm_mmu_func
gm20b_mmu = {
	.dma_bits = 40,
	.mmu = {{ -1, -1, NVIF_CLASS_MMU_GF100}},
	.mem = {{ -1, -1, NVIF_CLASS_MEM_GF100}, .umap = gf100_mem_map },
	.vmm = {{ -1,  0, NVIF_CLASS_VMM_GM200}, gm20b_vmm_new },
	.kind = gm200_mmu_kind,
	.kind_sys = true,
};

static const struct nvkm_mmu_func
gm20b_mmu_fixed = {
	.dma_bits = 40,
	.mmu = {{ -1, -1, NVIF_CLASS_MMU_GF100}},
	.mem = {{ -1, -1, NVIF_CLASS_MEM_GF100}, .umap = gf100_mem_map },
	.vmm = {{ -1, -1, NVIF_CLASS_VMM_GM200}, gm20b_vmm_new_fixed },
	.kind = gm200_mmu_kind,
	.kind_sys = true,
};

int
gm20b_mmu_new(struct nvkm_device *device, enum nvkm_subdev_type type, int inst,
	      struct nvkm_mmu **pmmu)
{
	if (device->fb->page)
		return nvkm_mmu_new_(&gm20b_mmu_fixed, device, type, inst, pmmu);
	return nvkm_mmu_new_(&gm20b_mmu, device, type, inst, pmmu);
}
