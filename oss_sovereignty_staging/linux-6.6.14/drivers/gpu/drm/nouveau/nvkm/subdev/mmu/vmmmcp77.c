 
#include "vmm.h"

static const struct nvkm_vmm_func
mcp77_vmm = {
	.join = nv50_vmm_join,
	.part = nv50_vmm_part,
	.valid = nv50_vmm_valid,
	.flush = nv50_vmm_flush,
	.page_block = 1 << 29,
	.page = {
		{ 16, &nv50_vmm_desc_16[0], NVKM_VMM_PAGE_xVxx },
		{ 12, &nv50_vmm_desc_12[0], NVKM_VMM_PAGE_xVHx },
		{}
	}
};

int
mcp77_vmm_new(struct nvkm_mmu *mmu, bool managed, u64 addr, u64 size,
	      void *argv, u32 argc, struct lock_class_key *key,
	      const char *name, struct nvkm_vmm **pvmm)
{
	return nv04_vmm_new_(&mcp77_vmm, mmu, 0, managed, addr, size,
			     argv, argc, key, name, pvmm);
}
