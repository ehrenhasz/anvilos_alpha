 
#include "mem.h"
#include "vmm.h"

#include <core/option.h>

#include <nvif/class.h>

static const u8 *
tu102_mmu_kind(struct nvkm_mmu *mmu, int *count, u8 *invalid)
{
	static const u8
	kind[16] = {
		0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,  
		0x06, 0x06, 0x02, 0x01, 0x03, 0x04, 0x05, 0x07,
	};
	*count = ARRAY_SIZE(kind);
	*invalid = 0x07;
	return kind;
}

static const struct nvkm_mmu_func
tu102_mmu = {
	.dma_bits = 47,
	.mmu = {{ -1, -1, NVIF_CLASS_MMU_GF100}},
	.mem = {{ -1,  0, NVIF_CLASS_MEM_GF100}, gf100_mem_new, gf100_mem_map },
	.vmm = {{ -1,  0, NVIF_CLASS_VMM_GP100}, tu102_vmm_new },
	.kind = tu102_mmu_kind,
	.kind_sys = true,
};

int
tu102_mmu_new(struct nvkm_device *device, enum nvkm_subdev_type type, int inst,
	      struct nvkm_mmu **pmmu)
{
	return nvkm_mmu_new_(&tu102_mmu, device, type, inst, pmmu);
}
