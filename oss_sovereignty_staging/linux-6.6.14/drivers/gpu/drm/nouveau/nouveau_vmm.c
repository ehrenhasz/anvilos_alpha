 
#include "nouveau_vmm.h"
#include "nouveau_drv.h"
#include "nouveau_bo.h"
#include "nouveau_svm.h"
#include "nouveau_mem.h"

void
nouveau_vma_unmap(struct nouveau_vma *vma)
{
	if (vma->mem) {
		nvif_vmm_unmap(&vma->vmm->vmm, vma->addr);
		vma->mem = NULL;
	}
}

int
nouveau_vma_map(struct nouveau_vma *vma, struct nouveau_mem *mem)
{
	struct nvif_vma tmp = { .addr = vma->addr };
	int ret = nouveau_mem_map(mem, &vma->vmm->vmm, &tmp);
	if (ret)
		return ret;
	vma->mem = mem;
	return 0;
}

struct nouveau_vma *
nouveau_vma_find(struct nouveau_bo *nvbo, struct nouveau_vmm *vmm)
{
	struct nouveau_vma *vma;

	list_for_each_entry(vma, &nvbo->vma_list, head) {
		if (vma->vmm == vmm)
			return vma;
	}

	return NULL;
}

void
nouveau_vma_del(struct nouveau_vma **pvma)
{
	struct nouveau_vma *vma = *pvma;
	if (vma && --vma->refs <= 0) {
		if (likely(vma->addr != ~0ULL)) {
			struct nvif_vma tmp = { .addr = vma->addr, .size = 1 };
			nvif_vmm_put(&vma->vmm->vmm, &tmp);
		}
		list_del(&vma->head);
		kfree(*pvma);
	}
	*pvma = NULL;
}

int
nouveau_vma_new(struct nouveau_bo *nvbo, struct nouveau_vmm *vmm,
		struct nouveau_vma **pvma)
{
	struct nouveau_mem *mem = nouveau_mem(nvbo->bo.resource);
	struct nouveau_vma *vma;
	struct nvif_vma tmp;
	int ret;

	if ((vma = *pvma = nouveau_vma_find(nvbo, vmm))) {
		vma->refs++;
		return 0;
	}

	if (!(vma = *pvma = kmalloc(sizeof(*vma), GFP_KERNEL)))
		return -ENOMEM;
	vma->vmm = vmm;
	vma->refs = 1;
	vma->addr = ~0ULL;
	vma->mem = NULL;
	vma->fence = NULL;
	list_add_tail(&vma->head, &nvbo->vma_list);

	if (nvbo->bo.resource->mem_type != TTM_PL_SYSTEM &&
	    mem->mem.page == nvbo->page) {
		ret = nvif_vmm_get(&vmm->vmm, LAZY, false, mem->mem.page, 0,
				   mem->mem.size, &tmp);
		if (ret)
			goto done;

		vma->addr = tmp.addr;
		ret = nouveau_vma_map(vma, mem);
	} else {
		ret = nvif_vmm_get(&vmm->vmm, PTES, false, mem->mem.page, 0,
				   mem->mem.size, &tmp);
		vma->addr = tmp.addr;
	}

done:
	if (ret)
		nouveau_vma_del(pvma);
	return ret;
}

void
nouveau_vmm_fini(struct nouveau_vmm *vmm)
{
	nouveau_svmm_fini(&vmm->svmm);
	nvif_vmm_dtor(&vmm->vmm);
	vmm->cli = NULL;
}

int
nouveau_vmm_init(struct nouveau_cli *cli, s32 oclass, struct nouveau_vmm *vmm)
{
	int ret = nvif_vmm_ctor(&cli->mmu, "drmVmm", oclass, UNMANAGED,
				PAGE_SIZE, 0, NULL, 0, &vmm->vmm);
	if (ret)
		return ret;

	vmm->cli = cli;
	return 0;
}
