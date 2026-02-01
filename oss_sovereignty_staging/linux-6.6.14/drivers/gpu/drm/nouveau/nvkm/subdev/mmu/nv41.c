 
#include "mem.h"
#include "vmm.h"

#include <core/option.h>

#include <nvif/class.h>

static void
nv41_mmu_init(struct nvkm_mmu *mmu)
{
	struct nvkm_device *device = mmu->subdev.device;
	nvkm_wr32(device, 0x100800, 0x00000002 | mmu->vmm->pd->pt[0]->addr);
	nvkm_mask(device, 0x10008c, 0x00000100, 0x00000100);
	nvkm_wr32(device, 0x100820, 0x00000000);
}

static const struct nvkm_mmu_func
nv41_mmu = {
	.init = nv41_mmu_init,
	.dma_bits = 39,
	.mmu = {{ -1, -1, NVIF_CLASS_MMU_NV04}},
	.mem = {{ -1, -1, NVIF_CLASS_MEM_NV04}, nv04_mem_new, nv04_mem_map },
	.vmm = {{ -1, -1, NVIF_CLASS_VMM_NV04}, nv41_vmm_new, true },
};

int
nv41_mmu_new(struct nvkm_device *device, enum nvkm_subdev_type type, int inst,
	     struct nvkm_mmu **pmmu)
{
	if (device->type == NVKM_DEVICE_AGP ||
	    !nvkm_boolopt(device->cfgopt, "NvPCIE", true))
		return nv04_mmu_new(device, type, inst, pmmu);

	return nvkm_mmu_new_(&nv41_mmu, device, type, inst, pmmu);
}
