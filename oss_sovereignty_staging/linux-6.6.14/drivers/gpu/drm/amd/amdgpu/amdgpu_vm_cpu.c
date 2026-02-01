 

#include "amdgpu_vm.h"
#include "amdgpu_object.h"
#include "amdgpu_trace.h"

 
static int amdgpu_vm_cpu_map_table(struct amdgpu_bo_vm *table)
{
	table->bo.flags |= AMDGPU_GEM_CREATE_CPU_ACCESS_REQUIRED;
	return amdgpu_bo_kmap(&table->bo, NULL);
}

 
static int amdgpu_vm_cpu_prepare(struct amdgpu_vm_update_params *p,
				 struct dma_resv *resv,
				 enum amdgpu_sync_mode sync_mode)
{
	if (!resv)
		return 0;

	return amdgpu_bo_sync_wait_resv(p->adev, resv, sync_mode, p->vm, true);
}

 
static int amdgpu_vm_cpu_update(struct amdgpu_vm_update_params *p,
				struct amdgpu_bo_vm *vmbo, uint64_t pe,
				uint64_t addr, unsigned count, uint32_t incr,
				uint64_t flags)
{
	unsigned int i;
	uint64_t value;
	long r;

	r = dma_resv_wait_timeout(vmbo->bo.tbo.base.resv, DMA_RESV_USAGE_KERNEL,
				  true, MAX_SCHEDULE_TIMEOUT);
	if (r < 0)
		return r;

	pe += (unsigned long)amdgpu_bo_kptr(&vmbo->bo);

	trace_amdgpu_vm_set_ptes(pe, addr, count, incr, flags, p->immediate);

	for (i = 0; i < count; i++) {
		value = p->pages_addr ?
			amdgpu_vm_map_gart(p->pages_addr, addr) :
			addr;
		amdgpu_gmc_set_pte_pde(p->adev, (void *)(uintptr_t)pe,
				       i, value, flags);
		addr += incr;
	}
	return 0;
}

 
static int amdgpu_vm_cpu_commit(struct amdgpu_vm_update_params *p,
				struct dma_fence **fence)
{
	 
	mb();
	amdgpu_device_flush_hdp(p->adev, NULL);
	return 0;
}

const struct amdgpu_vm_update_funcs amdgpu_vm_cpu_funcs = {
	.map_table = amdgpu_vm_cpu_map_table,
	.prepare = amdgpu_vm_cpu_prepare,
	.update = amdgpu_vm_cpu_update,
	.commit = amdgpu_vm_cpu_commit
};
