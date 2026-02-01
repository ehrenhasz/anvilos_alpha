 
#include "mem.h"

#include <core/memory.h>
#include <subdev/bar.h>
#include <subdev/fb.h>

#include <nvif/class.h>
#include <nvif/if900b.h>
#include <nvif/if900d.h>
#include <nvif/unpack.h>

int
gf100_mem_map(struct nvkm_mmu *mmu, struct nvkm_memory *memory, void *argv,
	      u32 argc, u64 *paddr, u64 *psize, struct nvkm_vma **pvma)
{
	struct gf100_vmm_map_v0 uvmm = {};
	union {
		struct gf100_mem_map_vn vn;
		struct gf100_mem_map_v0 v0;
	} *args = argv;
	struct nvkm_device *device = mmu->subdev.device;
	struct nvkm_vmm *bar = nvkm_bar_bar1_vmm(device);
	int ret = -ENOSYS;

	if (!(ret = nvif_unpack(ret, &argv, &argc, args->v0, 0, 0, false))) {
		uvmm.ro   = args->v0.ro;
		uvmm.kind = args->v0.kind;
	} else
	if (!(ret = nvif_unvers(ret, &argv, &argc, args->vn))) {
	} else
		return ret;

	ret = nvkm_vmm_get(bar, nvkm_memory_page(memory),
				nvkm_memory_size(memory), pvma);
	if (ret)
		return ret;

	ret = nvkm_memory_map(memory, 0, bar, *pvma, &uvmm, sizeof(uvmm));
	if (ret)
		return ret;

	*paddr = device->func->resource_addr(device, 1) + (*pvma)->addr;
	*psize = (*pvma)->size;
	return 0;
}

int
gf100_mem_new(struct nvkm_mmu *mmu, int type, u8 page, u64 size,
	      void *argv, u32 argc, struct nvkm_memory **pmemory)
{
	union {
		struct gf100_mem_vn vn;
		struct gf100_mem_v0 v0;
	} *args = argv;
	int ret = -ENOSYS;
	bool contig;

	if (!(ret = nvif_unpack(ret, &argv, &argc, args->v0, 0, 0, false))) {
		contig = args->v0.contig;
	} else
	if (!(ret = nvif_unvers(ret, &argv, &argc, args->vn))) {
		contig = false;
	} else
		return ret;

	if (mmu->type[type].type & (NVKM_MEM_DISP | NVKM_MEM_COMP))
		type = NVKM_RAM_MM_NORMAL;
	else
		type = NVKM_RAM_MM_MIXED;

	return nvkm_ram_get(mmu->subdev.device, type, 0x01, page,
			    size, contig, false, pmemory);
}
