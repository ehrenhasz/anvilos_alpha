 
#include "vmm.h"

static const struct nvkm_vmm_func
gp10b_vmm = {
	.join = gp100_vmm_join,
	.part = gf100_vmm_part,
	.aper = gk20a_vmm_aper,
	.valid = gp100_vmm_valid,
	.flush = gp100_vmm_flush,
	.mthd = gp100_vmm_mthd,
	.invalidate_pdb = gp100_vmm_invalidate_pdb,
	.page = {
		{ 47, &gp100_vmm_desc_16[4], NVKM_VMM_PAGE_Sxxx },
		{ 38, &gp100_vmm_desc_16[3], NVKM_VMM_PAGE_Sxxx },
		{ 29, &gp100_vmm_desc_16[2], NVKM_VMM_PAGE_Sxxx },
		{ 21, &gp100_vmm_desc_16[1], NVKM_VMM_PAGE_SxHC },
		{ 16, &gp100_vmm_desc_16[0], NVKM_VMM_PAGE_SxHC },
		{ 12, &gp100_vmm_desc_12[0], NVKM_VMM_PAGE_SxHx },
		{}
	}
};

int
gp10b_vmm_new(struct nvkm_mmu *mmu, bool managed, u64 addr, u64 size,
	      void *argv, u32 argc, struct lock_class_key *key,
	      const char *name, struct nvkm_vmm **pvmm)
{
	return gp100_vmm_new_(&gp10b_vmm, mmu, managed, addr, size,
			      argv, argc, key, name, pvmm);
}
