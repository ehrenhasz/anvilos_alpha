 
#include "mem.h"
#include "vmm.h"

#include <core/option.h>

#include <nvif/class.h>

static void
nv44_mmu_init(struct nvkm_mmu *mmu)
{
	struct nvkm_device *device = mmu->subdev.device;
	struct nvkm_memory *pt = mmu->vmm->pd->pt[0]->memory;
	u32 addr;

	 
	addr  = nvkm_rd32(device, 0x10020c);
	addr -= ((nvkm_memory_addr(pt) >> 19) + 1) << 19;

	nvkm_wr32(device, 0x100850, 0x80000000);
	nvkm_wr32(device, 0x100818, mmu->vmm->null);
	nvkm_wr32(device, 0x100804, (nvkm_memory_size(pt) / 4) * 4096);
	nvkm_wr32(device, 0x100850, 0x00008000);
	nvkm_mask(device, 0x10008c, 0x00000200, 0x00000200);
	nvkm_wr32(device, 0x100820, 0x00000000);
	nvkm_wr32(device, 0x10082c, 0x00000001);
	nvkm_wr32(device, 0x100800, addr | 0x00000010);
}

static const struct nvkm_mmu_func
nv44_mmu = {
	.init = nv44_mmu_init,
	.dma_bits = 39,
	.mmu = {{ -1, -1, NVIF_CLASS_MMU_NV04}},
	.mem = {{ -1, -1, NVIF_CLASS_MEM_NV04}, nv04_mem_new, nv04_mem_map },
	.vmm = {{ -1, -1, NVIF_CLASS_VMM_NV04}, nv44_vmm_new, true },
};

int
nv44_mmu_new(struct nvkm_device *device, enum nvkm_subdev_type type, int inst,
	     struct nvkm_mmu **pmmu)
{
	if (device->type == NVKM_DEVICE_AGP ||
	    !nvkm_boolopt(device->cfgopt, "NvPCIE", true))
		return nv04_mmu_new(device, type, inst, pmmu);

	return nvkm_mmu_new_(&nv44_mmu, device, type, inst, pmmu);
}
