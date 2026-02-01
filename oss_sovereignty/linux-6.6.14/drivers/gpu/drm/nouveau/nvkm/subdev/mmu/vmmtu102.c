 
#include "vmm.h"

#include <subdev/timer.h>

static void
tu102_vmm_flush(struct nvkm_vmm *vmm, int depth)
{
	struct nvkm_device *device = vmm->mmu->subdev.device;
	u32 type = (5   - depth) << 24;

	type |= 0x00000001;  
	if (atomic_read(&vmm->engref[NVKM_SUBDEV_BAR]))
		type |= 0x00000006;  

	mutex_lock(&vmm->mmu->mutex);

	nvkm_wr32(device, 0xb830a0, vmm->pd->pt[0]->addr >> 8);
	nvkm_wr32(device, 0xb830a4, 0x00000000);
	nvkm_wr32(device, 0x100e68, 0x00000000);
	nvkm_wr32(device, 0xb830b0, 0x80000000 | type);

	nvkm_msec(device, 2000,
		if (!(nvkm_rd32(device, 0xb830b0) & 0x80000000))
			break;
	);

	mutex_unlock(&vmm->mmu->mutex);
}

static const struct nvkm_vmm_func
tu102_vmm = {
	.join = gv100_vmm_join,
	.part = gf100_vmm_part,
	.aper = gf100_vmm_aper,
	.valid = gp100_vmm_valid,
	.flush = tu102_vmm_flush,
	.mthd = gp100_vmm_mthd,
	.page = {
		{ 47, &gp100_vmm_desc_16[4], NVKM_VMM_PAGE_Sxxx },
		{ 38, &gp100_vmm_desc_16[3], NVKM_VMM_PAGE_Sxxx },
		{ 29, &gp100_vmm_desc_16[2], NVKM_VMM_PAGE_Sxxx },
		{ 21, &gp100_vmm_desc_16[1], NVKM_VMM_PAGE_SVxC },
		{ 16, &gp100_vmm_desc_16[0], NVKM_VMM_PAGE_SVxC },
		{ 12, &gp100_vmm_desc_12[0], NVKM_VMM_PAGE_SVHx },
		{}
	}
};

int
tu102_vmm_new(struct nvkm_mmu *mmu, bool managed, u64 addr, u64 size,
	      void *argv, u32 argc, struct lock_class_key *key,
	      const char *name, struct nvkm_vmm **pvmm)
{
	return gp100_vmm_new_(&tu102_vmm, mmu, managed, addr, size,
			      argv, argc, key, name, pvmm);
}
