 
#include "mem.h"

#include <core/memory.h>
#include <subdev/fb.h>

#include <nvif/if000b.h>
#include <nvif/unpack.h>

int
nv04_mem_map(struct nvkm_mmu *mmu, struct nvkm_memory *memory, void *argv,
	     u32 argc, u64 *paddr, u64 *psize, struct nvkm_vma **pvma)
{
	union {
		struct nv04_mem_map_vn vn;
	} *args = argv;
	struct nvkm_device *device = mmu->subdev.device;
	const u64 addr = nvkm_memory_addr(memory);
	int ret = -ENOSYS;

	if ((ret = nvif_unvers(ret, &argv, &argc, args->vn)))
		return ret;

	*paddr = device->func->resource_addr(device, 1) + addr;
	*psize = nvkm_memory_size(memory);
	*pvma = ERR_PTR(-ENODEV);
	return 0;
}

int
nv04_mem_new(struct nvkm_mmu *mmu, int type, u8 page, u64 size,
	     void *argv, u32 argc, struct nvkm_memory **pmemory)
{
	union {
		struct nv04_mem_vn vn;
	} *args = argv;
	int ret = -ENOSYS;

	if ((ret = nvif_unvers(ret, &argv, &argc, args->vn)))
		return ret;

	if (mmu->type[type].type & NVKM_MEM_MAPPABLE)
		type = NVKM_RAM_MM_NORMAL;
	else
		type = NVKM_RAM_MM_NOMAP;

	return nvkm_ram_get(mmu->subdev.device, type, 0x01, page,
			    size, true, false, pmemory);
}
