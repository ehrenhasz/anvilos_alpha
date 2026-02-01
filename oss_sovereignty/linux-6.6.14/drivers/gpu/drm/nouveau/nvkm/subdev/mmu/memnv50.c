 
#include "mem.h"

#include <core/memory.h>
#include <subdev/bar.h>
#include <subdev/fb.h>

#include <nvif/class.h>
#include <nvif/if500b.h>
#include <nvif/if500d.h>
#include <nvif/unpack.h>

int
nv50_mem_map(struct nvkm_mmu *mmu, struct nvkm_memory *memory, void *argv,
	     u32 argc, u64 *paddr, u64 *psize, struct nvkm_vma **pvma)
{
	struct nv50_vmm_map_v0 uvmm = {};
	union {
		struct nv50_mem_map_vn vn;
		struct nv50_mem_map_v0 v0;
	} *args = argv;
	struct nvkm_device *device = mmu->subdev.device;
	struct nvkm_vmm *bar = nvkm_bar_bar1_vmm(device);
	u64 size = nvkm_memory_size(memory);
	int ret = -ENOSYS;

	if (!(ret = nvif_unpack(ret, &argv, &argc, args->v0, 0, 0, false))) {
		uvmm.ro   = args->v0.ro;
		uvmm.kind = args->v0.kind;
		uvmm.comp = args->v0.comp;
	} else
	if (!(ret = nvif_unvers(ret, &argv, &argc, args->vn))) {
	} else
		return ret;

	ret = nvkm_vmm_get(bar, 12, size, pvma);
	if (ret)
		return ret;

	*paddr = device->func->resource_addr(device, 1) + (*pvma)->addr;
	*psize = (*pvma)->size;
	return nvkm_memory_map(memory, 0, bar, *pvma, &uvmm, sizeof(uvmm));
}

int
nv50_mem_new(struct nvkm_mmu *mmu, int type, u8 page, u64 size,
	     void *argv, u32 argc, struct nvkm_memory **pmemory)
{
	union {
		struct nv50_mem_vn vn;
		struct nv50_mem_v0 v0;
	} *args = argv;
	int ret = -ENOSYS;
	bool contig;

	if (!(ret = nvif_unpack(ret, &argv, &argc, args->v0, 0, 0, false))) {
		type   = args->v0.bankswz ? 0x02 : 0x01;
		contig = args->v0.contig;
	} else
	if (!(ret = nvif_unvers(ret, &argv, &argc, args->vn))) {
		type   = 0x01;
		contig = false;
	} else
		return -ENOSYS;

	return nvkm_ram_get(mmu->subdev.device, NVKM_RAM_MM_NORMAL, type,
			    page, size, contig, false, pmemory);
}
