 

#include <linux/dma-fence-array.h>
#include <linux/interval_tree_generic.h>
#include <linux/idr.h>
#include <linux/dma-buf.h>

#include <drm/amdgpu_drm.h>
#include <drm/drm_drv.h>
#include <drm/ttm/ttm_tt.h>
#include <drm/drm_exec.h>
#include "amdgpu.h"
#include "amdgpu_trace.h"
#include "amdgpu_amdkfd.h"
#include "amdgpu_gmc.h"
#include "amdgpu_xgmi.h"
#include "amdgpu_dma_buf.h"
#include "amdgpu_res_cursor.h"
#include "kfd_svm.h"

 

#define START(node) ((node)->start)
#define LAST(node) ((node)->last)

INTERVAL_TREE_DEFINE(struct amdgpu_bo_va_mapping, rb, uint64_t, __subtree_last,
		     START, LAST, static, amdgpu_vm_it)

#undef START
#undef LAST

 
struct amdgpu_prt_cb {

	 
	struct amdgpu_device *adev;

	 
	struct dma_fence_cb cb;
};

 
struct amdgpu_vm_tlb_seq_struct {
	 
	struct amdgpu_vm *vm;

	 
	struct dma_fence_cb cb;
};

 
int amdgpu_vm_set_pasid(struct amdgpu_device *adev, struct amdgpu_vm *vm,
			u32 pasid)
{
	int r;

	if (vm->pasid == pasid)
		return 0;

	if (vm->pasid) {
		r = xa_err(xa_erase_irq(&adev->vm_manager.pasids, vm->pasid));
		if (r < 0)
			return r;

		vm->pasid = 0;
	}

	if (pasid) {
		r = xa_err(xa_store_irq(&adev->vm_manager.pasids, pasid, vm,
					GFP_KERNEL));
		if (r < 0)
			return r;

		vm->pasid = pasid;
	}


	return 0;
}

 
static void amdgpu_vm_bo_evicted(struct amdgpu_vm_bo_base *vm_bo)
{
	struct amdgpu_vm *vm = vm_bo->vm;
	struct amdgpu_bo *bo = vm_bo->bo;

	vm_bo->moved = true;
	spin_lock(&vm_bo->vm->status_lock);
	if (bo->tbo.type == ttm_bo_type_kernel)
		list_move(&vm_bo->vm_status, &vm->evicted);
	else
		list_move_tail(&vm_bo->vm_status, &vm->evicted);
	spin_unlock(&vm_bo->vm->status_lock);
}
 
static void amdgpu_vm_bo_moved(struct amdgpu_vm_bo_base *vm_bo)
{
	spin_lock(&vm_bo->vm->status_lock);
	list_move(&vm_bo->vm_status, &vm_bo->vm->moved);
	spin_unlock(&vm_bo->vm->status_lock);
}

 
static void amdgpu_vm_bo_idle(struct amdgpu_vm_bo_base *vm_bo)
{
	spin_lock(&vm_bo->vm->status_lock);
	list_move(&vm_bo->vm_status, &vm_bo->vm->idle);
	spin_unlock(&vm_bo->vm->status_lock);
	vm_bo->moved = false;
}

 
static void amdgpu_vm_bo_invalidated(struct amdgpu_vm_bo_base *vm_bo)
{
	spin_lock(&vm_bo->vm->status_lock);
	list_move(&vm_bo->vm_status, &vm_bo->vm->invalidated);
	spin_unlock(&vm_bo->vm->status_lock);
}

 
static void amdgpu_vm_bo_relocated(struct amdgpu_vm_bo_base *vm_bo)
{
	if (vm_bo->bo->parent) {
		spin_lock(&vm_bo->vm->status_lock);
		list_move(&vm_bo->vm_status, &vm_bo->vm->relocated);
		spin_unlock(&vm_bo->vm->status_lock);
	} else {
		amdgpu_vm_bo_idle(vm_bo);
	}
}

 
static void amdgpu_vm_bo_done(struct amdgpu_vm_bo_base *vm_bo)
{
	spin_lock(&vm_bo->vm->status_lock);
	list_move(&vm_bo->vm_status, &vm_bo->vm->done);
	spin_unlock(&vm_bo->vm->status_lock);
}

 
static void amdgpu_vm_bo_reset_state_machine(struct amdgpu_vm *vm)
{
	struct amdgpu_vm_bo_base *vm_bo, *tmp;

	spin_lock(&vm->status_lock);
	list_splice_init(&vm->done, &vm->invalidated);
	list_for_each_entry(vm_bo, &vm->invalidated, vm_status)
		vm_bo->moved = true;
	list_for_each_entry_safe(vm_bo, tmp, &vm->idle, vm_status) {
		struct amdgpu_bo *bo = vm_bo->bo;

		vm_bo->moved = true;
		if (!bo || bo->tbo.type != ttm_bo_type_kernel)
			list_move(&vm_bo->vm_status, &vm_bo->vm->moved);
		else if (bo->parent)
			list_move(&vm_bo->vm_status, &vm_bo->vm->relocated);
	}
	spin_unlock(&vm->status_lock);
}

 
void amdgpu_vm_bo_base_init(struct amdgpu_vm_bo_base *base,
			    struct amdgpu_vm *vm, struct amdgpu_bo *bo)
{
	base->vm = vm;
	base->bo = bo;
	base->next = NULL;
	INIT_LIST_HEAD(&base->vm_status);

	if (!bo)
		return;
	base->next = bo->vm_bo;
	bo->vm_bo = base;

	if (bo->tbo.base.resv != vm->root.bo->tbo.base.resv)
		return;

	dma_resv_assert_held(vm->root.bo->tbo.base.resv);

	ttm_bo_set_bulk_move(&bo->tbo, &vm->lru_bulk_move);
	if (bo->tbo.type == ttm_bo_type_kernel && bo->parent)
		amdgpu_vm_bo_relocated(base);
	else
		amdgpu_vm_bo_idle(base);

	if (bo->preferred_domains &
	    amdgpu_mem_type_to_domain(bo->tbo.resource->mem_type))
		return;

	 
	amdgpu_vm_bo_evicted(base);
}

 
int amdgpu_vm_lock_pd(struct amdgpu_vm *vm, struct drm_exec *exec,
		      unsigned int num_fences)
{
	 
	return drm_exec_prepare_obj(exec, &vm->root.bo->tbo.base,
				    2 + num_fences);
}

 
void amdgpu_vm_move_to_lru_tail(struct amdgpu_device *adev,
				struct amdgpu_vm *vm)
{
	spin_lock(&adev->mman.bdev.lru_lock);
	ttm_lru_bulk_move_tail(&vm->lru_bulk_move);
	spin_unlock(&adev->mman.bdev.lru_lock);
}

 
static int amdgpu_vm_init_entities(struct amdgpu_device *adev,
				   struct amdgpu_vm *vm)
{
	int r;

	r = drm_sched_entity_init(&vm->immediate, DRM_SCHED_PRIORITY_NORMAL,
				  adev->vm_manager.vm_pte_scheds,
				  adev->vm_manager.vm_pte_num_scheds, NULL);
	if (r)
		goto error;

	return drm_sched_entity_init(&vm->delayed, DRM_SCHED_PRIORITY_NORMAL,
				     adev->vm_manager.vm_pte_scheds,
				     adev->vm_manager.vm_pte_num_scheds, NULL);

error:
	drm_sched_entity_destroy(&vm->immediate);
	return r;
}

 
static void amdgpu_vm_fini_entities(struct amdgpu_vm *vm)
{
	drm_sched_entity_destroy(&vm->immediate);
	drm_sched_entity_destroy(&vm->delayed);
}

 
uint64_t amdgpu_vm_generation(struct amdgpu_device *adev, struct amdgpu_vm *vm)
{
	uint64_t result = (u64)atomic_read(&adev->vram_lost_counter) << 32;

	if (!vm)
		return result;

	result += vm->generation;
	 
	if (drm_sched_entity_error(&vm->delayed))
		++result;

	return result;
}

 
int amdgpu_vm_validate_pt_bos(struct amdgpu_device *adev, struct amdgpu_vm *vm,
			      int (*validate)(void *p, struct amdgpu_bo *bo),
			      void *param)
{
	struct amdgpu_vm_bo_base *bo_base;
	struct amdgpu_bo *shadow;
	struct amdgpu_bo *bo;
	int r;

	if (drm_sched_entity_error(&vm->delayed)) {
		++vm->generation;
		amdgpu_vm_bo_reset_state_machine(vm);
		amdgpu_vm_fini_entities(vm);
		r = amdgpu_vm_init_entities(adev, vm);
		if (r)
			return r;
	}

	spin_lock(&vm->status_lock);
	while (!list_empty(&vm->evicted)) {
		bo_base = list_first_entry(&vm->evicted,
					   struct amdgpu_vm_bo_base,
					   vm_status);
		spin_unlock(&vm->status_lock);

		bo = bo_base->bo;
		shadow = amdgpu_bo_shadowed(bo);

		r = validate(param, bo);
		if (r)
			return r;
		if (shadow) {
			r = validate(param, shadow);
			if (r)
				return r;
		}

		if (bo->tbo.type != ttm_bo_type_kernel) {
			amdgpu_vm_bo_moved(bo_base);
		} else {
			vm->update_funcs->map_table(to_amdgpu_bo_vm(bo));
			amdgpu_vm_bo_relocated(bo_base);
		}
		spin_lock(&vm->status_lock);
	}
	spin_unlock(&vm->status_lock);

	amdgpu_vm_eviction_lock(vm);
	vm->evicting = false;
	amdgpu_vm_eviction_unlock(vm);

	return 0;
}

 
bool amdgpu_vm_ready(struct amdgpu_vm *vm)
{
	bool empty;
	bool ret;

	amdgpu_vm_eviction_lock(vm);
	ret = !vm->evicting;
	amdgpu_vm_eviction_unlock(vm);

	spin_lock(&vm->status_lock);
	empty = list_empty(&vm->evicted);
	spin_unlock(&vm->status_lock);

	return ret && empty;
}

 
void amdgpu_vm_check_compute_bug(struct amdgpu_device *adev)
{
	const struct amdgpu_ip_block *ip_block;
	bool has_compute_vm_bug;
	struct amdgpu_ring *ring;
	int i;

	has_compute_vm_bug = false;

	ip_block = amdgpu_device_ip_get_ip_block(adev, AMD_IP_BLOCK_TYPE_GFX);
	if (ip_block) {
		 
		if (ip_block->version->major <= 7)
			has_compute_vm_bug = true;
		else if (ip_block->version->major == 8)
			if (adev->gfx.mec_fw_version < 673)
				has_compute_vm_bug = true;
	}

	for (i = 0; i < adev->num_rings; i++) {
		ring = adev->rings[i];
		if (ring->funcs->type == AMDGPU_RING_TYPE_COMPUTE)
			 
			ring->has_compute_vm_bug = has_compute_vm_bug;
		else
			ring->has_compute_vm_bug = false;
	}
}

 
bool amdgpu_vm_need_pipeline_sync(struct amdgpu_ring *ring,
				  struct amdgpu_job *job)
{
	struct amdgpu_device *adev = ring->adev;
	unsigned vmhub = ring->vm_hub;
	struct amdgpu_vmid_mgr *id_mgr = &adev->vm_manager.id_mgr[vmhub];

	if (job->vmid == 0)
		return false;

	if (job->vm_needs_flush || ring->has_compute_vm_bug)
		return true;

	if (ring->funcs->emit_gds_switch && job->gds_switch_needed)
		return true;

	if (amdgpu_vmid_had_gpu_reset(adev, &id_mgr->ids[job->vmid]))
		return true;

	return false;
}

 
int amdgpu_vm_flush(struct amdgpu_ring *ring, struct amdgpu_job *job,
		    bool need_pipe_sync)
{
	struct amdgpu_device *adev = ring->adev;
	unsigned vmhub = ring->vm_hub;
	struct amdgpu_vmid_mgr *id_mgr = &adev->vm_manager.id_mgr[vmhub];
	struct amdgpu_vmid *id = &id_mgr->ids[job->vmid];
	bool spm_update_needed = job->spm_update_needed;
	bool gds_switch_needed = ring->funcs->emit_gds_switch &&
		job->gds_switch_needed;
	bool vm_flush_needed = job->vm_needs_flush;
	struct dma_fence *fence = NULL;
	bool pasid_mapping_needed = false;
	unsigned patch_offset = 0;
	int r;

	if (amdgpu_vmid_had_gpu_reset(adev, id)) {
		gds_switch_needed = true;
		vm_flush_needed = true;
		pasid_mapping_needed = true;
		spm_update_needed = true;
	}

	mutex_lock(&id_mgr->lock);
	if (id->pasid != job->pasid || !id->pasid_mapping ||
	    !dma_fence_is_signaled(id->pasid_mapping))
		pasid_mapping_needed = true;
	mutex_unlock(&id_mgr->lock);

	gds_switch_needed &= !!ring->funcs->emit_gds_switch;
	vm_flush_needed &= !!ring->funcs->emit_vm_flush  &&
			job->vm_pd_addr != AMDGPU_BO_INVALID_OFFSET;
	pasid_mapping_needed &= adev->gmc.gmc_funcs->emit_pasid_mapping &&
		ring->funcs->emit_wreg;

	if (!vm_flush_needed && !gds_switch_needed && !need_pipe_sync)
		return 0;

	amdgpu_ring_ib_begin(ring);
	if (ring->funcs->init_cond_exec)
		patch_offset = amdgpu_ring_init_cond_exec(ring);

	if (need_pipe_sync)
		amdgpu_ring_emit_pipeline_sync(ring);

	if (vm_flush_needed) {
		trace_amdgpu_vm_flush(ring, job->vmid, job->vm_pd_addr);
		amdgpu_ring_emit_vm_flush(ring, job->vmid, job->vm_pd_addr);
	}

	if (pasid_mapping_needed)
		amdgpu_gmc_emit_pasid_mapping(ring, job->vmid, job->pasid);

	if (spm_update_needed && adev->gfx.rlc.funcs->update_spm_vmid)
		adev->gfx.rlc.funcs->update_spm_vmid(adev, job->vmid);

	if (!ring->is_mes_queue && ring->funcs->emit_gds_switch &&
	    gds_switch_needed) {
		amdgpu_ring_emit_gds_switch(ring, job->vmid, job->gds_base,
					    job->gds_size, job->gws_base,
					    job->gws_size, job->oa_base,
					    job->oa_size);
	}

	if (vm_flush_needed || pasid_mapping_needed) {
		r = amdgpu_fence_emit(ring, &fence, NULL, 0);
		if (r)
			return r;
	}

	if (vm_flush_needed) {
		mutex_lock(&id_mgr->lock);
		dma_fence_put(id->last_flush);
		id->last_flush = dma_fence_get(fence);
		id->current_gpu_reset_count =
			atomic_read(&adev->gpu_reset_counter);
		mutex_unlock(&id_mgr->lock);
	}

	if (pasid_mapping_needed) {
		mutex_lock(&id_mgr->lock);
		id->pasid = job->pasid;
		dma_fence_put(id->pasid_mapping);
		id->pasid_mapping = dma_fence_get(fence);
		mutex_unlock(&id_mgr->lock);
	}
	dma_fence_put(fence);

	if (ring->funcs->patch_cond_exec)
		amdgpu_ring_patch_cond_exec(ring, patch_offset);

	 
	if (ring->funcs->emit_switch_buffer) {
		amdgpu_ring_emit_switch_buffer(ring);
		amdgpu_ring_emit_switch_buffer(ring);
	}
	amdgpu_ring_ib_end(ring);
	return 0;
}

 
struct amdgpu_bo_va *amdgpu_vm_bo_find(struct amdgpu_vm *vm,
				       struct amdgpu_bo *bo)
{
	struct amdgpu_vm_bo_base *base;

	for (base = bo->vm_bo; base; base = base->next) {
		if (base->vm != vm)
			continue;

		return container_of(base, struct amdgpu_bo_va, base);
	}
	return NULL;
}

 
uint64_t amdgpu_vm_map_gart(const dma_addr_t *pages_addr, uint64_t addr)
{
	uint64_t result;

	 
	result = pages_addr[addr >> PAGE_SHIFT];

	 
	result |= addr & (~PAGE_MASK);

	result &= 0xFFFFFFFFFFFFF000ULL;

	return result;
}

 
int amdgpu_vm_update_pdes(struct amdgpu_device *adev,
			  struct amdgpu_vm *vm, bool immediate)
{
	struct amdgpu_vm_update_params params;
	struct amdgpu_vm_bo_base *entry;
	bool flush_tlb_needed = false;
	LIST_HEAD(relocated);
	int r, idx;

	spin_lock(&vm->status_lock);
	list_splice_init(&vm->relocated, &relocated);
	spin_unlock(&vm->status_lock);

	if (list_empty(&relocated))
		return 0;

	if (!drm_dev_enter(adev_to_drm(adev), &idx))
		return -ENODEV;

	memset(&params, 0, sizeof(params));
	params.adev = adev;
	params.vm = vm;
	params.immediate = immediate;

	r = vm->update_funcs->prepare(&params, NULL, AMDGPU_SYNC_EXPLICIT);
	if (r)
		goto error;

	list_for_each_entry(entry, &relocated, vm_status) {
		 
		flush_tlb_needed |= entry->moved;

		r = amdgpu_vm_pde_update(&params, entry);
		if (r)
			goto error;
	}

	r = vm->update_funcs->commit(&params, &vm->last_update);
	if (r)
		goto error;

	if (flush_tlb_needed)
		atomic64_inc(&vm->tlb_seq);

	while (!list_empty(&relocated)) {
		entry = list_first_entry(&relocated, struct amdgpu_vm_bo_base,
					 vm_status);
		amdgpu_vm_bo_idle(entry);
	}

error:
	drm_dev_exit(idx);
	return r;
}

 
static void amdgpu_vm_tlb_seq_cb(struct dma_fence *fence,
				 struct dma_fence_cb *cb)
{
	struct amdgpu_vm_tlb_seq_struct *tlb_cb;

	tlb_cb = container_of(cb, typeof(*tlb_cb), cb);
	atomic64_inc(&tlb_cb->vm->tlb_seq);
	kfree(tlb_cb);
}

 
int amdgpu_vm_update_range(struct amdgpu_device *adev, struct amdgpu_vm *vm,
			   bool immediate, bool unlocked, bool flush_tlb,
			   struct dma_resv *resv, uint64_t start, uint64_t last,
			   uint64_t flags, uint64_t offset, uint64_t vram_base,
			   struct ttm_resource *res, dma_addr_t *pages_addr,
			   struct dma_fence **fence)
{
	struct amdgpu_vm_update_params params;
	struct amdgpu_vm_tlb_seq_struct *tlb_cb;
	struct amdgpu_res_cursor cursor;
	enum amdgpu_sync_mode sync_mode;
	int r, idx;

	if (!drm_dev_enter(adev_to_drm(adev), &idx))
		return -ENODEV;

	tlb_cb = kmalloc(sizeof(*tlb_cb), GFP_KERNEL);
	if (!tlb_cb) {
		r = -ENOMEM;
		goto error_unlock;
	}

	 
	flush_tlb |= adev->gmc.xgmi.num_physical_nodes &&
		     adev->ip_versions[GC_HWIP][0] == IP_VERSION(9, 4, 0);

	 
	flush_tlb |= adev->ip_versions[GC_HWIP][0] < IP_VERSION(9, 0, 0);

	memset(&params, 0, sizeof(params));
	params.adev = adev;
	params.vm = vm;
	params.immediate = immediate;
	params.pages_addr = pages_addr;
	params.unlocked = unlocked;

	 
	if (!(flags & AMDGPU_PTE_VALID))
		sync_mode = AMDGPU_SYNC_EQ_OWNER;
	else
		sync_mode = AMDGPU_SYNC_EXPLICIT;

	amdgpu_vm_eviction_lock(vm);
	if (vm->evicting) {
		r = -EBUSY;
		goto error_free;
	}

	if (!unlocked && !dma_fence_is_signaled(vm->last_unlocked)) {
		struct dma_fence *tmp = dma_fence_get_stub();

		amdgpu_bo_fence(vm->root.bo, vm->last_unlocked, true);
		swap(vm->last_unlocked, tmp);
		dma_fence_put(tmp);
	}

	r = vm->update_funcs->prepare(&params, resv, sync_mode);
	if (r)
		goto error_free;

	amdgpu_res_first(pages_addr ? NULL : res, offset,
			 (last - start + 1) * AMDGPU_GPU_PAGE_SIZE, &cursor);
	while (cursor.remaining) {
		uint64_t tmp, num_entries, addr;

		num_entries = cursor.size >> AMDGPU_GPU_PAGE_SHIFT;
		if (pages_addr) {
			bool contiguous = true;

			if (num_entries > AMDGPU_GPU_PAGES_IN_CPU_PAGE) {
				uint64_t pfn = cursor.start >> PAGE_SHIFT;
				uint64_t count;

				contiguous = pages_addr[pfn + 1] ==
					pages_addr[pfn] + PAGE_SIZE;

				tmp = num_entries /
					AMDGPU_GPU_PAGES_IN_CPU_PAGE;
				for (count = 2; count < tmp; ++count) {
					uint64_t idx = pfn + count;

					if (contiguous != (pages_addr[idx] ==
					    pages_addr[idx - 1] + PAGE_SIZE))
						break;
				}
				if (!contiguous)
					count--;
				num_entries = count *
					AMDGPU_GPU_PAGES_IN_CPU_PAGE;
			}

			if (!contiguous) {
				addr = cursor.start;
				params.pages_addr = pages_addr;
			} else {
				addr = pages_addr[cursor.start >> PAGE_SHIFT];
				params.pages_addr = NULL;
			}

		} else if (flags & (AMDGPU_PTE_VALID | AMDGPU_PTE_PRT)) {
			addr = vram_base + cursor.start;
		} else {
			addr = 0;
		}

		tmp = start + num_entries;
		r = amdgpu_vm_ptes_update(&params, start, tmp, addr, flags);
		if (r)
			goto error_free;

		amdgpu_res_next(&cursor, num_entries * AMDGPU_GPU_PAGE_SIZE);
		start = tmp;
	}

	r = vm->update_funcs->commit(&params, fence);

	if (flush_tlb || params.table_freed) {
		tlb_cb->vm = vm;
		if (fence && *fence &&
		    !dma_fence_add_callback(*fence, &tlb_cb->cb,
					   amdgpu_vm_tlb_seq_cb)) {
			dma_fence_put(vm->last_tlb_flush);
			vm->last_tlb_flush = dma_fence_get(*fence);
		} else {
			amdgpu_vm_tlb_seq_cb(NULL, &tlb_cb->cb);
		}
		tlb_cb = NULL;
	}

error_free:
	kfree(tlb_cb);

error_unlock:
	amdgpu_vm_eviction_unlock(vm);
	drm_dev_exit(idx);
	return r;
}

static void amdgpu_vm_bo_get_memory(struct amdgpu_bo_va *bo_va,
				    struct amdgpu_mem_stats *stats)
{
	struct amdgpu_vm *vm = bo_va->base.vm;
	struct amdgpu_bo *bo = bo_va->base.bo;

	if (!bo)
		return;

	 
	if (bo->tbo.base.resv != vm->root.bo->tbo.base.resv &&
	    !dma_resv_trylock(bo->tbo.base.resv))
		return;

	amdgpu_bo_get_memory(bo, stats);
	if (bo->tbo.base.resv != vm->root.bo->tbo.base.resv)
	    dma_resv_unlock(bo->tbo.base.resv);
}

void amdgpu_vm_get_memory(struct amdgpu_vm *vm,
			  struct amdgpu_mem_stats *stats)
{
	struct amdgpu_bo_va *bo_va, *tmp;

	spin_lock(&vm->status_lock);
	list_for_each_entry_safe(bo_va, tmp, &vm->idle, base.vm_status)
		amdgpu_vm_bo_get_memory(bo_va, stats);

	list_for_each_entry_safe(bo_va, tmp, &vm->evicted, base.vm_status)
		amdgpu_vm_bo_get_memory(bo_va, stats);

	list_for_each_entry_safe(bo_va, tmp, &vm->relocated, base.vm_status)
		amdgpu_vm_bo_get_memory(bo_va, stats);

	list_for_each_entry_safe(bo_va, tmp, &vm->moved, base.vm_status)
		amdgpu_vm_bo_get_memory(bo_va, stats);

	list_for_each_entry_safe(bo_va, tmp, &vm->invalidated, base.vm_status)
		amdgpu_vm_bo_get_memory(bo_va, stats);

	list_for_each_entry_safe(bo_va, tmp, &vm->done, base.vm_status)
		amdgpu_vm_bo_get_memory(bo_va, stats);
	spin_unlock(&vm->status_lock);
}

 
int amdgpu_vm_bo_update(struct amdgpu_device *adev, struct amdgpu_bo_va *bo_va,
			bool clear)
{
	struct amdgpu_bo *bo = bo_va->base.bo;
	struct amdgpu_vm *vm = bo_va->base.vm;
	struct amdgpu_bo_va_mapping *mapping;
	dma_addr_t *pages_addr = NULL;
	struct ttm_resource *mem;
	struct dma_fence **last_update;
	bool flush_tlb = clear;
	struct dma_resv *resv;
	uint64_t vram_base;
	uint64_t flags;
	int r;

	if (clear || !bo) {
		mem = NULL;
		resv = vm->root.bo->tbo.base.resv;
	} else {
		struct drm_gem_object *obj = &bo->tbo.base;

		resv = bo->tbo.base.resv;
		if (obj->import_attach && bo_va->is_xgmi) {
			struct dma_buf *dma_buf = obj->import_attach->dmabuf;
			struct drm_gem_object *gobj = dma_buf->priv;
			struct amdgpu_bo *abo = gem_to_amdgpu_bo(gobj);

			if (abo->tbo.resource &&
			    abo->tbo.resource->mem_type == TTM_PL_VRAM)
				bo = gem_to_amdgpu_bo(gobj);
		}
		mem = bo->tbo.resource;
		if (mem && (mem->mem_type == TTM_PL_TT ||
			    mem->mem_type == AMDGPU_PL_PREEMPT))
			pages_addr = bo->tbo.ttm->dma_address;
	}

	if (bo) {
		struct amdgpu_device *bo_adev;

		flags = amdgpu_ttm_tt_pte_flags(adev, bo->tbo.ttm, mem);

		if (amdgpu_bo_encrypted(bo))
			flags |= AMDGPU_PTE_TMZ;

		bo_adev = amdgpu_ttm_adev(bo->tbo.bdev);
		vram_base = bo_adev->vm_manager.vram_base_offset;
	} else {
		flags = 0x0;
		vram_base = 0;
	}

	if (clear || (bo && bo->tbo.base.resv ==
		      vm->root.bo->tbo.base.resv))
		last_update = &vm->last_update;
	else
		last_update = &bo_va->last_pt_update;

	if (!clear && bo_va->base.moved) {
		flush_tlb = true;
		list_splice_init(&bo_va->valids, &bo_va->invalids);

	} else if (bo_va->cleared != clear) {
		list_splice_init(&bo_va->valids, &bo_va->invalids);
	}

	list_for_each_entry(mapping, &bo_va->invalids, list) {
		uint64_t update_flags = flags;

		 
		if (!(mapping->flags & AMDGPU_PTE_READABLE))
			update_flags &= ~AMDGPU_PTE_READABLE;
		if (!(mapping->flags & AMDGPU_PTE_WRITEABLE))
			update_flags &= ~AMDGPU_PTE_WRITEABLE;

		 
		amdgpu_gmc_get_vm_pte(adev, mapping, &update_flags);

		trace_amdgpu_vm_bo_update(mapping);

		r = amdgpu_vm_update_range(adev, vm, false, false, flush_tlb,
					   resv, mapping->start, mapping->last,
					   update_flags, mapping->offset,
					   vram_base, mem, pages_addr,
					   last_update);
		if (r)
			return r;
	}

	 
	if (bo && bo->tbo.base.resv == vm->root.bo->tbo.base.resv) {
		uint32_t mem_type = bo->tbo.resource->mem_type;

		if (!(bo->preferred_domains &
		      amdgpu_mem_type_to_domain(mem_type)))
			amdgpu_vm_bo_evicted(&bo_va->base);
		else
			amdgpu_vm_bo_idle(&bo_va->base);
	} else {
		amdgpu_vm_bo_done(&bo_va->base);
	}

	list_splice_init(&bo_va->invalids, &bo_va->valids);
	bo_va->cleared = clear;
	bo_va->base.moved = false;

	if (trace_amdgpu_vm_bo_mapping_enabled()) {
		list_for_each_entry(mapping, &bo_va->valids, list)
			trace_amdgpu_vm_bo_mapping(mapping);
	}

	return 0;
}

 
static void amdgpu_vm_update_prt_state(struct amdgpu_device *adev)
{
	unsigned long flags;
	bool enable;

	spin_lock_irqsave(&adev->vm_manager.prt_lock, flags);
	enable = !!atomic_read(&adev->vm_manager.num_prt_users);
	adev->gmc.gmc_funcs->set_prt(adev, enable);
	spin_unlock_irqrestore(&adev->vm_manager.prt_lock, flags);
}

 
static void amdgpu_vm_prt_get(struct amdgpu_device *adev)
{
	if (!adev->gmc.gmc_funcs->set_prt)
		return;

	if (atomic_inc_return(&adev->vm_manager.num_prt_users) == 1)
		amdgpu_vm_update_prt_state(adev);
}

 
static void amdgpu_vm_prt_put(struct amdgpu_device *adev)
{
	if (atomic_dec_return(&adev->vm_manager.num_prt_users) == 0)
		amdgpu_vm_update_prt_state(adev);
}

 
static void amdgpu_vm_prt_cb(struct dma_fence *fence, struct dma_fence_cb *_cb)
{
	struct amdgpu_prt_cb *cb = container_of(_cb, struct amdgpu_prt_cb, cb);

	amdgpu_vm_prt_put(cb->adev);
	kfree(cb);
}

 
static void amdgpu_vm_add_prt_cb(struct amdgpu_device *adev,
				 struct dma_fence *fence)
{
	struct amdgpu_prt_cb *cb;

	if (!adev->gmc.gmc_funcs->set_prt)
		return;

	cb = kmalloc(sizeof(struct amdgpu_prt_cb), GFP_KERNEL);
	if (!cb) {
		 
		if (fence)
			dma_fence_wait(fence, false);

		amdgpu_vm_prt_put(adev);
	} else {
		cb->adev = adev;
		if (!fence || dma_fence_add_callback(fence, &cb->cb,
						     amdgpu_vm_prt_cb))
			amdgpu_vm_prt_cb(fence, &cb->cb);
	}
}

 
static void amdgpu_vm_free_mapping(struct amdgpu_device *adev,
				   struct amdgpu_vm *vm,
				   struct amdgpu_bo_va_mapping *mapping,
				   struct dma_fence *fence)
{
	if (mapping->flags & AMDGPU_PTE_PRT)
		amdgpu_vm_add_prt_cb(adev, fence);
	kfree(mapping);
}

 
static void amdgpu_vm_prt_fini(struct amdgpu_device *adev, struct amdgpu_vm *vm)
{
	struct dma_resv *resv = vm->root.bo->tbo.base.resv;
	struct dma_resv_iter cursor;
	struct dma_fence *fence;

	dma_resv_for_each_fence(&cursor, resv, DMA_RESV_USAGE_BOOKKEEP, fence) {
		 
		amdgpu_vm_prt_get(adev);
		amdgpu_vm_add_prt_cb(adev, fence);
	}
}

 
int amdgpu_vm_clear_freed(struct amdgpu_device *adev,
			  struct amdgpu_vm *vm,
			  struct dma_fence **fence)
{
	struct dma_resv *resv = vm->root.bo->tbo.base.resv;
	struct amdgpu_bo_va_mapping *mapping;
	uint64_t init_pte_value = 0;
	struct dma_fence *f = NULL;
	int r;

	while (!list_empty(&vm->freed)) {
		mapping = list_first_entry(&vm->freed,
			struct amdgpu_bo_va_mapping, list);
		list_del(&mapping->list);

		if (vm->pte_support_ats &&
		    mapping->start < AMDGPU_GMC_HOLE_START)
			init_pte_value = AMDGPU_PTE_DEFAULT_ATC;

		r = amdgpu_vm_update_range(adev, vm, false, false, true, resv,
					   mapping->start, mapping->last,
					   init_pte_value, 0, 0, NULL, NULL,
					   &f);
		amdgpu_vm_free_mapping(adev, vm, mapping, f);
		if (r) {
			dma_fence_put(f);
			return r;
		}
	}

	if (fence && f) {
		dma_fence_put(*fence);
		*fence = f;
	} else {
		dma_fence_put(f);
	}

	return 0;

}

 
int amdgpu_vm_handle_moved(struct amdgpu_device *adev,
			   struct amdgpu_vm *vm)
{
	struct amdgpu_bo_va *bo_va;
	struct dma_resv *resv;
	bool clear;
	int r;

	spin_lock(&vm->status_lock);
	while (!list_empty(&vm->moved)) {
		bo_va = list_first_entry(&vm->moved, struct amdgpu_bo_va,
					 base.vm_status);
		spin_unlock(&vm->status_lock);

		 
		r = amdgpu_vm_bo_update(adev, bo_va, false);
		if (r)
			return r;
		spin_lock(&vm->status_lock);
	}

	while (!list_empty(&vm->invalidated)) {
		bo_va = list_first_entry(&vm->invalidated, struct amdgpu_bo_va,
					 base.vm_status);
		resv = bo_va->base.bo->tbo.base.resv;
		spin_unlock(&vm->status_lock);

		 
		if (!amdgpu_vm_debug && dma_resv_trylock(resv))
			clear = false;
		 
		else
			clear = true;

		r = amdgpu_vm_bo_update(adev, bo_va, clear);
		if (r)
			return r;

		if (!clear)
			dma_resv_unlock(resv);
		spin_lock(&vm->status_lock);
	}
	spin_unlock(&vm->status_lock);

	return 0;
}

 
struct amdgpu_bo_va *amdgpu_vm_bo_add(struct amdgpu_device *adev,
				      struct amdgpu_vm *vm,
				      struct amdgpu_bo *bo)
{
	struct amdgpu_bo_va *bo_va;

	bo_va = kzalloc(sizeof(struct amdgpu_bo_va), GFP_KERNEL);
	if (bo_va == NULL) {
		return NULL;
	}
	amdgpu_vm_bo_base_init(&bo_va->base, vm, bo);

	bo_va->ref_count = 1;
	bo_va->last_pt_update = dma_fence_get_stub();
	INIT_LIST_HEAD(&bo_va->valids);
	INIT_LIST_HEAD(&bo_va->invalids);

	if (!bo)
		return bo_va;

	dma_resv_assert_held(bo->tbo.base.resv);
	if (amdgpu_dmabuf_is_xgmi_accessible(adev, bo)) {
		bo_va->is_xgmi = true;
		 
		amdgpu_xgmi_set_pstate(adev, AMDGPU_XGMI_PSTATE_MAX_VEGA20);
	}

	return bo_va;
}


 
static void amdgpu_vm_bo_insert_map(struct amdgpu_device *adev,
				    struct amdgpu_bo_va *bo_va,
				    struct amdgpu_bo_va_mapping *mapping)
{
	struct amdgpu_vm *vm = bo_va->base.vm;
	struct amdgpu_bo *bo = bo_va->base.bo;

	mapping->bo_va = bo_va;
	list_add(&mapping->list, &bo_va->invalids);
	amdgpu_vm_it_insert(mapping, &vm->va);

	if (mapping->flags & AMDGPU_PTE_PRT)
		amdgpu_vm_prt_get(adev);

	if (bo && bo->tbo.base.resv == vm->root.bo->tbo.base.resv &&
	    !bo_va->base.moved) {
		amdgpu_vm_bo_moved(&bo_va->base);
	}
	trace_amdgpu_vm_bo_map(bo_va, mapping);
}

 
int amdgpu_vm_bo_map(struct amdgpu_device *adev,
		     struct amdgpu_bo_va *bo_va,
		     uint64_t saddr, uint64_t offset,
		     uint64_t size, uint64_t flags)
{
	struct amdgpu_bo_va_mapping *mapping, *tmp;
	struct amdgpu_bo *bo = bo_va->base.bo;
	struct amdgpu_vm *vm = bo_va->base.vm;
	uint64_t eaddr;

	 
	if (saddr & ~PAGE_MASK || offset & ~PAGE_MASK || size & ~PAGE_MASK)
		return -EINVAL;
	if (saddr + size <= saddr || offset + size <= offset)
		return -EINVAL;

	 
	eaddr = saddr + size - 1;
	if ((bo && offset + size > amdgpu_bo_size(bo)) ||
	    (eaddr >= adev->vm_manager.max_pfn << AMDGPU_GPU_PAGE_SHIFT))
		return -EINVAL;

	saddr /= AMDGPU_GPU_PAGE_SIZE;
	eaddr /= AMDGPU_GPU_PAGE_SIZE;

	tmp = amdgpu_vm_it_iter_first(&vm->va, saddr, eaddr);
	if (tmp) {
		 
		dev_err(adev->dev, "bo %p va 0x%010Lx-0x%010Lx conflict with "
			"0x%010Lx-0x%010Lx\n", bo, saddr, eaddr,
			tmp->start, tmp->last + 1);
		return -EINVAL;
	}

	mapping = kmalloc(sizeof(*mapping), GFP_KERNEL);
	if (!mapping)
		return -ENOMEM;

	mapping->start = saddr;
	mapping->last = eaddr;
	mapping->offset = offset;
	mapping->flags = flags;

	amdgpu_vm_bo_insert_map(adev, bo_va, mapping);

	return 0;
}

 
int amdgpu_vm_bo_replace_map(struct amdgpu_device *adev,
			     struct amdgpu_bo_va *bo_va,
			     uint64_t saddr, uint64_t offset,
			     uint64_t size, uint64_t flags)
{
	struct amdgpu_bo_va_mapping *mapping;
	struct amdgpu_bo *bo = bo_va->base.bo;
	uint64_t eaddr;
	int r;

	 
	if (saddr & ~PAGE_MASK || offset & ~PAGE_MASK || size & ~PAGE_MASK)
		return -EINVAL;
	if (saddr + size <= saddr || offset + size <= offset)
		return -EINVAL;

	 
	eaddr = saddr + size - 1;
	if ((bo && offset + size > amdgpu_bo_size(bo)) ||
	    (eaddr >= adev->vm_manager.max_pfn << AMDGPU_GPU_PAGE_SHIFT))
		return -EINVAL;

	 
	mapping = kmalloc(sizeof(*mapping), GFP_KERNEL);
	if (!mapping)
		return -ENOMEM;

	r = amdgpu_vm_bo_clear_mappings(adev, bo_va->base.vm, saddr, size);
	if (r) {
		kfree(mapping);
		return r;
	}

	saddr /= AMDGPU_GPU_PAGE_SIZE;
	eaddr /= AMDGPU_GPU_PAGE_SIZE;

	mapping->start = saddr;
	mapping->last = eaddr;
	mapping->offset = offset;
	mapping->flags = flags;

	amdgpu_vm_bo_insert_map(adev, bo_va, mapping);

	return 0;
}

 
int amdgpu_vm_bo_unmap(struct amdgpu_device *adev,
		       struct amdgpu_bo_va *bo_va,
		       uint64_t saddr)
{
	struct amdgpu_bo_va_mapping *mapping;
	struct amdgpu_vm *vm = bo_va->base.vm;
	bool valid = true;

	saddr /= AMDGPU_GPU_PAGE_SIZE;

	list_for_each_entry(mapping, &bo_va->valids, list) {
		if (mapping->start == saddr)
			break;
	}

	if (&mapping->list == &bo_va->valids) {
		valid = false;

		list_for_each_entry(mapping, &bo_va->invalids, list) {
			if (mapping->start == saddr)
				break;
		}

		if (&mapping->list == &bo_va->invalids)
			return -ENOENT;
	}

	list_del(&mapping->list);
	amdgpu_vm_it_remove(mapping, &vm->va);
	mapping->bo_va = NULL;
	trace_amdgpu_vm_bo_unmap(bo_va, mapping);

	if (valid)
		list_add(&mapping->list, &vm->freed);
	else
		amdgpu_vm_free_mapping(adev, vm, mapping,
				       bo_va->last_pt_update);

	return 0;
}

 
int amdgpu_vm_bo_clear_mappings(struct amdgpu_device *adev,
				struct amdgpu_vm *vm,
				uint64_t saddr, uint64_t size)
{
	struct amdgpu_bo_va_mapping *before, *after, *tmp, *next;
	LIST_HEAD(removed);
	uint64_t eaddr;

	eaddr = saddr + size - 1;
	saddr /= AMDGPU_GPU_PAGE_SIZE;
	eaddr /= AMDGPU_GPU_PAGE_SIZE;

	 
	before = kzalloc(sizeof(*before), GFP_KERNEL);
	if (!before)
		return -ENOMEM;
	INIT_LIST_HEAD(&before->list);

	after = kzalloc(sizeof(*after), GFP_KERNEL);
	if (!after) {
		kfree(before);
		return -ENOMEM;
	}
	INIT_LIST_HEAD(&after->list);

	 
	tmp = amdgpu_vm_it_iter_first(&vm->va, saddr, eaddr);
	while (tmp) {
		 
		if (tmp->start < saddr) {
			before->start = tmp->start;
			before->last = saddr - 1;
			before->offset = tmp->offset;
			before->flags = tmp->flags;
			before->bo_va = tmp->bo_va;
			list_add(&before->list, &tmp->bo_va->invalids);
		}

		 
		if (tmp->last > eaddr) {
			after->start = eaddr + 1;
			after->last = tmp->last;
			after->offset = tmp->offset;
			after->offset += (after->start - tmp->start) << PAGE_SHIFT;
			after->flags = tmp->flags;
			after->bo_va = tmp->bo_va;
			list_add(&after->list, &tmp->bo_va->invalids);
		}

		list_del(&tmp->list);
		list_add(&tmp->list, &removed);

		tmp = amdgpu_vm_it_iter_next(tmp, saddr, eaddr);
	}

	 
	list_for_each_entry_safe(tmp, next, &removed, list) {
		amdgpu_vm_it_remove(tmp, &vm->va);
		list_del(&tmp->list);

		if (tmp->start < saddr)
		    tmp->start = saddr;
		if (tmp->last > eaddr)
		    tmp->last = eaddr;

		tmp->bo_va = NULL;
		list_add(&tmp->list, &vm->freed);
		trace_amdgpu_vm_bo_unmap(NULL, tmp);
	}

	 
	if (!list_empty(&before->list)) {
		struct amdgpu_bo *bo = before->bo_va->base.bo;

		amdgpu_vm_it_insert(before, &vm->va);
		if (before->flags & AMDGPU_PTE_PRT)
			amdgpu_vm_prt_get(adev);

		if (bo && bo->tbo.base.resv == vm->root.bo->tbo.base.resv &&
		    !before->bo_va->base.moved)
			amdgpu_vm_bo_moved(&before->bo_va->base);
	} else {
		kfree(before);
	}

	 
	if (!list_empty(&after->list)) {
		struct amdgpu_bo *bo = after->bo_va->base.bo;

		amdgpu_vm_it_insert(after, &vm->va);
		if (after->flags & AMDGPU_PTE_PRT)
			amdgpu_vm_prt_get(adev);

		if (bo && bo->tbo.base.resv == vm->root.bo->tbo.base.resv &&
		    !after->bo_va->base.moved)
			amdgpu_vm_bo_moved(&after->bo_va->base);
	} else {
		kfree(after);
	}

	return 0;
}

 
struct amdgpu_bo_va_mapping *amdgpu_vm_bo_lookup_mapping(struct amdgpu_vm *vm,
							 uint64_t addr)
{
	return amdgpu_vm_it_iter_first(&vm->va, addr, addr);
}

 
void amdgpu_vm_bo_trace_cs(struct amdgpu_vm *vm, struct ww_acquire_ctx *ticket)
{
	struct amdgpu_bo_va_mapping *mapping;

	if (!trace_amdgpu_vm_bo_cs_enabled())
		return;

	for (mapping = amdgpu_vm_it_iter_first(&vm->va, 0, U64_MAX); mapping;
	     mapping = amdgpu_vm_it_iter_next(mapping, 0, U64_MAX)) {
		if (mapping->bo_va && mapping->bo_va->base.bo) {
			struct amdgpu_bo *bo;

			bo = mapping->bo_va->base.bo;
			if (dma_resv_locking_ctx(bo->tbo.base.resv) !=
			    ticket)
				continue;
		}

		trace_amdgpu_vm_bo_cs(mapping);
	}
}

 
void amdgpu_vm_bo_del(struct amdgpu_device *adev,
		      struct amdgpu_bo_va *bo_va)
{
	struct amdgpu_bo_va_mapping *mapping, *next;
	struct amdgpu_bo *bo = bo_va->base.bo;
	struct amdgpu_vm *vm = bo_va->base.vm;
	struct amdgpu_vm_bo_base **base;

	dma_resv_assert_held(vm->root.bo->tbo.base.resv);

	if (bo) {
		dma_resv_assert_held(bo->tbo.base.resv);
		if (bo->tbo.base.resv == vm->root.bo->tbo.base.resv)
			ttm_bo_set_bulk_move(&bo->tbo, NULL);

		for (base = &bo_va->base.bo->vm_bo; *base;
		     base = &(*base)->next) {
			if (*base != &bo_va->base)
				continue;

			*base = bo_va->base.next;
			break;
		}
	}

	spin_lock(&vm->status_lock);
	list_del(&bo_va->base.vm_status);
	spin_unlock(&vm->status_lock);

	list_for_each_entry_safe(mapping, next, &bo_va->valids, list) {
		list_del(&mapping->list);
		amdgpu_vm_it_remove(mapping, &vm->va);
		mapping->bo_va = NULL;
		trace_amdgpu_vm_bo_unmap(bo_va, mapping);
		list_add(&mapping->list, &vm->freed);
	}
	list_for_each_entry_safe(mapping, next, &bo_va->invalids, list) {
		list_del(&mapping->list);
		amdgpu_vm_it_remove(mapping, &vm->va);
		amdgpu_vm_free_mapping(adev, vm, mapping,
				       bo_va->last_pt_update);
	}

	dma_fence_put(bo_va->last_pt_update);

	if (bo && bo_va->is_xgmi)
		amdgpu_xgmi_set_pstate(adev, AMDGPU_XGMI_PSTATE_MIN);

	kfree(bo_va);
}

 
bool amdgpu_vm_evictable(struct amdgpu_bo *bo)
{
	struct amdgpu_vm_bo_base *bo_base = bo->vm_bo;

	 
	if (!bo_base || !bo_base->vm)
		return true;

	 
	if (!dma_resv_test_signaled(bo->tbo.base.resv, DMA_RESV_USAGE_BOOKKEEP))
		return false;

	 
	if (!amdgpu_vm_eviction_trylock(bo_base->vm))
		return false;

	 
	if (!dma_fence_is_signaled(bo_base->vm->last_unlocked)) {
		amdgpu_vm_eviction_unlock(bo_base->vm);
		return false;
	}

	bo_base->vm->evicting = true;
	amdgpu_vm_eviction_unlock(bo_base->vm);
	return true;
}

 
void amdgpu_vm_bo_invalidate(struct amdgpu_device *adev,
			     struct amdgpu_bo *bo, bool evicted)
{
	struct amdgpu_vm_bo_base *bo_base;

	 
	if (bo->parent && (amdgpu_bo_shadowed(bo->parent) == bo))
		bo = bo->parent;

	for (bo_base = bo->vm_bo; bo_base; bo_base = bo_base->next) {
		struct amdgpu_vm *vm = bo_base->vm;

		if (evicted && bo->tbo.base.resv == vm->root.bo->tbo.base.resv) {
			amdgpu_vm_bo_evicted(bo_base);
			continue;
		}

		if (bo_base->moved)
			continue;
		bo_base->moved = true;

		if (bo->tbo.type == ttm_bo_type_kernel)
			amdgpu_vm_bo_relocated(bo_base);
		else if (bo->tbo.base.resv == vm->root.bo->tbo.base.resv)
			amdgpu_vm_bo_moved(bo_base);
		else
			amdgpu_vm_bo_invalidated(bo_base);
	}
}

 
static uint32_t amdgpu_vm_get_block_size(uint64_t vm_size)
{
	 
	unsigned bits = ilog2(vm_size) + 18;

	 
	if (vm_size <= 8)
		return (bits - 9);
	else
		return ((bits + 3) / 2);
}

 
void amdgpu_vm_adjust_size(struct amdgpu_device *adev, uint32_t min_vm_size,
			   uint32_t fragment_size_default, unsigned max_level,
			   unsigned max_bits)
{
	unsigned int max_size = 1 << (max_bits - 30);
	unsigned int vm_size;
	uint64_t tmp;

	 
	if (amdgpu_vm_size != -1) {
		vm_size = amdgpu_vm_size;
		if (vm_size > max_size) {
			dev_warn(adev->dev, "VM size (%d) too large, max is %u GB\n",
				 amdgpu_vm_size, max_size);
			vm_size = max_size;
		}
	} else {
		struct sysinfo si;
		unsigned int phys_ram_gb;

		 
		si_meminfo(&si);
		phys_ram_gb = ((uint64_t)si.totalram * si.mem_unit +
			       (1 << 30) - 1) >> 30;
		vm_size = roundup_pow_of_two(
			min(max(phys_ram_gb * 3, min_vm_size), max_size));
	}

	adev->vm_manager.max_pfn = (uint64_t)vm_size << 18;

	tmp = roundup_pow_of_two(adev->vm_manager.max_pfn);
	if (amdgpu_vm_block_size != -1)
		tmp >>= amdgpu_vm_block_size - 9;
	tmp = DIV_ROUND_UP(fls64(tmp) - 1, 9) - 1;
	adev->vm_manager.num_level = min(max_level, (unsigned)tmp);
	switch (adev->vm_manager.num_level) {
	case 3:
		adev->vm_manager.root_level = AMDGPU_VM_PDB2;
		break;
	case 2:
		adev->vm_manager.root_level = AMDGPU_VM_PDB1;
		break;
	case 1:
		adev->vm_manager.root_level = AMDGPU_VM_PDB0;
		break;
	default:
		dev_err(adev->dev, "VMPT only supports 2~4+1 levels\n");
	}
	 
	if (amdgpu_vm_block_size != -1)
		adev->vm_manager.block_size =
			min((unsigned)amdgpu_vm_block_size, max_bits
			    - AMDGPU_GPU_PAGE_SHIFT
			    - 9 * adev->vm_manager.num_level);
	else if (adev->vm_manager.num_level > 1)
		adev->vm_manager.block_size = 9;
	else
		adev->vm_manager.block_size = amdgpu_vm_get_block_size(tmp);

	if (amdgpu_vm_fragment_size == -1)
		adev->vm_manager.fragment_size = fragment_size_default;
	else
		adev->vm_manager.fragment_size = amdgpu_vm_fragment_size;

	DRM_INFO("vm size is %u GB, %u levels, block size is %u-bit, fragment size is %u-bit\n",
		 vm_size, adev->vm_manager.num_level + 1,
		 adev->vm_manager.block_size,
		 adev->vm_manager.fragment_size);
}

 
long amdgpu_vm_wait_idle(struct amdgpu_vm *vm, long timeout)
{
	timeout = dma_resv_wait_timeout(vm->root.bo->tbo.base.resv,
					DMA_RESV_USAGE_BOOKKEEP,
					true, timeout);
	if (timeout <= 0)
		return timeout;

	return dma_fence_wait_timeout(vm->last_unlocked, true, timeout);
}

 
int amdgpu_vm_init(struct amdgpu_device *adev, struct amdgpu_vm *vm,
		   int32_t xcp_id)
{
	struct amdgpu_bo *root_bo;
	struct amdgpu_bo_vm *root;
	int r, i;

	vm->va = RB_ROOT_CACHED;
	for (i = 0; i < AMDGPU_MAX_VMHUBS; i++)
		vm->reserved_vmid[i] = NULL;
	INIT_LIST_HEAD(&vm->evicted);
	INIT_LIST_HEAD(&vm->relocated);
	INIT_LIST_HEAD(&vm->moved);
	INIT_LIST_HEAD(&vm->idle);
	INIT_LIST_HEAD(&vm->invalidated);
	spin_lock_init(&vm->status_lock);
	INIT_LIST_HEAD(&vm->freed);
	INIT_LIST_HEAD(&vm->done);
	INIT_LIST_HEAD(&vm->pt_freed);
	INIT_WORK(&vm->pt_free_work, amdgpu_vm_pt_free_work);
	INIT_KFIFO(vm->faults);

	r = amdgpu_vm_init_entities(adev, vm);
	if (r)
		return r;

	vm->pte_support_ats = false;
	vm->is_compute_context = false;

	vm->use_cpu_for_update = !!(adev->vm_manager.vm_update_mode &
				    AMDGPU_VM_USE_CPU_FOR_GFX);

	DRM_DEBUG_DRIVER("VM update mode is %s\n",
			 vm->use_cpu_for_update ? "CPU" : "SDMA");
	WARN_ONCE((vm->use_cpu_for_update &&
		   !amdgpu_gmc_vram_full_visible(&adev->gmc)),
		  "CPU update of VM recommended only for large BAR system\n");

	if (vm->use_cpu_for_update)
		vm->update_funcs = &amdgpu_vm_cpu_funcs;
	else
		vm->update_funcs = &amdgpu_vm_sdma_funcs;

	vm->last_update = dma_fence_get_stub();
	vm->last_unlocked = dma_fence_get_stub();
	vm->last_tlb_flush = dma_fence_get_stub();
	vm->generation = 0;

	mutex_init(&vm->eviction_lock);
	vm->evicting = false;

	r = amdgpu_vm_pt_create(adev, vm, adev->vm_manager.root_level,
				false, &root, xcp_id);
	if (r)
		goto error_free_delayed;

	root_bo = amdgpu_bo_ref(&root->bo);
	r = amdgpu_bo_reserve(root_bo, true);
	if (r) {
		amdgpu_bo_unref(&root->shadow);
		amdgpu_bo_unref(&root_bo);
		goto error_free_delayed;
	}

	amdgpu_vm_bo_base_init(&vm->root, vm, root_bo);
	r = dma_resv_reserve_fences(root_bo->tbo.base.resv, 1);
	if (r)
		goto error_free_root;

	r = amdgpu_vm_pt_clear(adev, vm, root, false);
	if (r)
		goto error_free_root;

	amdgpu_bo_unreserve(vm->root.bo);
	amdgpu_bo_unref(&root_bo);

	return 0;

error_free_root:
	amdgpu_vm_pt_free_root(adev, vm);
	amdgpu_bo_unreserve(vm->root.bo);
	amdgpu_bo_unref(&root_bo);

error_free_delayed:
	dma_fence_put(vm->last_tlb_flush);
	dma_fence_put(vm->last_unlocked);
	amdgpu_vm_fini_entities(vm);

	return r;
}

 
int amdgpu_vm_make_compute(struct amdgpu_device *adev, struct amdgpu_vm *vm)
{
	bool pte_support_ats = (adev->asic_type == CHIP_RAVEN);
	int r;

	r = amdgpu_bo_reserve(vm->root.bo, true);
	if (r)
		return r;

	 
	if (pte_support_ats != vm->pte_support_ats) {
		 
		if (!amdgpu_vm_pt_is_root_clean(adev, vm)) {
			r = -EINVAL;
			goto unreserve_bo;
		}

		vm->pte_support_ats = pte_support_ats;
		r = amdgpu_vm_pt_clear(adev, vm, to_amdgpu_bo_vm(vm->root.bo),
				       false);
		if (r)
			goto unreserve_bo;
	}

	 
	vm->use_cpu_for_update = !!(adev->vm_manager.vm_update_mode &
				    AMDGPU_VM_USE_CPU_FOR_COMPUTE);
	DRM_DEBUG_DRIVER("VM update mode is %s\n",
			 vm->use_cpu_for_update ? "CPU" : "SDMA");
	WARN_ONCE((vm->use_cpu_for_update &&
		   !amdgpu_gmc_vram_full_visible(&adev->gmc)),
		  "CPU update of VM recommended only for large BAR system\n");

	if (vm->use_cpu_for_update) {
		 
		r = amdgpu_bo_sync_wait(vm->root.bo,
					AMDGPU_FENCE_OWNER_UNDEFINED, true);
		if (r)
			goto unreserve_bo;

		vm->update_funcs = &amdgpu_vm_cpu_funcs;
		r = amdgpu_vm_pt_map_tables(adev, vm);
		if (r)
			goto unreserve_bo;

	} else {
		vm->update_funcs = &amdgpu_vm_sdma_funcs;
	}

	dma_fence_put(vm->last_update);
	vm->last_update = dma_fence_get_stub();
	vm->is_compute_context = true;

	 
	amdgpu_bo_unref(&to_amdgpu_bo_vm(vm->root.bo)->shadow);

	goto unreserve_bo;

unreserve_bo:
	amdgpu_bo_unreserve(vm->root.bo);
	return r;
}

 
void amdgpu_vm_release_compute(struct amdgpu_device *adev, struct amdgpu_vm *vm)
{
	amdgpu_vm_set_pasid(adev, vm, 0);
	vm->is_compute_context = false;
}

 
void amdgpu_vm_fini(struct amdgpu_device *adev, struct amdgpu_vm *vm)
{
	struct amdgpu_bo_va_mapping *mapping, *tmp;
	bool prt_fini_needed = !!adev->gmc.gmc_funcs->set_prt;
	struct amdgpu_bo *root;
	unsigned long flags;
	int i;

	amdgpu_amdkfd_gpuvm_destroy_cb(adev, vm);

	flush_work(&vm->pt_free_work);

	root = amdgpu_bo_ref(vm->root.bo);
	amdgpu_bo_reserve(root, true);
	amdgpu_vm_set_pasid(adev, vm, 0);
	dma_fence_wait(vm->last_unlocked, false);
	dma_fence_put(vm->last_unlocked);
	dma_fence_wait(vm->last_tlb_flush, false);
	 
	spin_lock_irqsave(vm->last_tlb_flush->lock, flags);
	spin_unlock_irqrestore(vm->last_tlb_flush->lock, flags);
	dma_fence_put(vm->last_tlb_flush);

	list_for_each_entry_safe(mapping, tmp, &vm->freed, list) {
		if (mapping->flags & AMDGPU_PTE_PRT && prt_fini_needed) {
			amdgpu_vm_prt_fini(adev, vm);
			prt_fini_needed = false;
		}

		list_del(&mapping->list);
		amdgpu_vm_free_mapping(adev, vm, mapping, NULL);
	}

	amdgpu_vm_pt_free_root(adev, vm);
	amdgpu_bo_unreserve(root);
	amdgpu_bo_unref(&root);
	WARN_ON(vm->root.bo);

	amdgpu_vm_fini_entities(vm);

	if (!RB_EMPTY_ROOT(&vm->va.rb_root)) {
		dev_err(adev->dev, "still active bo inside vm\n");
	}
	rbtree_postorder_for_each_entry_safe(mapping, tmp,
					     &vm->va.rb_root, rb) {
		 
		list_del(&mapping->list);
		kfree(mapping);
	}

	dma_fence_put(vm->last_update);

	for (i = 0; i < AMDGPU_MAX_VMHUBS; i++) {
		if (vm->reserved_vmid[i]) {
			amdgpu_vmid_free_reserved(adev, i);
			vm->reserved_vmid[i] = false;
		}
	}

}

 
void amdgpu_vm_manager_init(struct amdgpu_device *adev)
{
	unsigned i;

	 
	adev->vm_manager.concurrent_flush = !(adev->asic_type < CHIP_VEGA10 ||
					      adev->asic_type == CHIP_NAVI10 ||
					      adev->asic_type == CHIP_NAVI14);
	amdgpu_vmid_mgr_init(adev);

	adev->vm_manager.fence_context =
		dma_fence_context_alloc(AMDGPU_MAX_RINGS);
	for (i = 0; i < AMDGPU_MAX_RINGS; ++i)
		adev->vm_manager.seqno[i] = 0;

	spin_lock_init(&adev->vm_manager.prt_lock);
	atomic_set(&adev->vm_manager.num_prt_users, 0);

	 
#ifdef CONFIG_X86_64
	if (amdgpu_vm_update_mode == -1) {
		 
		if (amdgpu_gmc_vram_full_visible(&adev->gmc) &&
		    !amdgpu_sriov_vf_mmio_access_protection(adev))
			adev->vm_manager.vm_update_mode =
				AMDGPU_VM_USE_CPU_FOR_COMPUTE;
		else
			adev->vm_manager.vm_update_mode = 0;
	} else
		adev->vm_manager.vm_update_mode = amdgpu_vm_update_mode;
#else
	adev->vm_manager.vm_update_mode = 0;
#endif

	xa_init_flags(&adev->vm_manager.pasids, XA_FLAGS_LOCK_IRQ);
}

 
void amdgpu_vm_manager_fini(struct amdgpu_device *adev)
{
	WARN_ON(!xa_empty(&adev->vm_manager.pasids));
	xa_destroy(&adev->vm_manager.pasids);

	amdgpu_vmid_mgr_fini(adev);
}

 
int amdgpu_vm_ioctl(struct drm_device *dev, void *data, struct drm_file *filp)
{
	union drm_amdgpu_vm *args = data;
	struct amdgpu_device *adev = drm_to_adev(dev);
	struct amdgpu_fpriv *fpriv = filp->driver_priv;

	 
	if (args->in.flags)
		return -EINVAL;

	switch (args->in.op) {
	case AMDGPU_VM_OP_RESERVE_VMID:
		 
		if (!fpriv->vm.reserved_vmid[AMDGPU_GFXHUB(0)]) {
			amdgpu_vmid_alloc_reserved(adev, AMDGPU_GFXHUB(0));
			fpriv->vm.reserved_vmid[AMDGPU_GFXHUB(0)] = true;
		}

		break;
	case AMDGPU_VM_OP_UNRESERVE_VMID:
		if (fpriv->vm.reserved_vmid[AMDGPU_GFXHUB(0)]) {
			amdgpu_vmid_free_reserved(adev, AMDGPU_GFXHUB(0));
			fpriv->vm.reserved_vmid[AMDGPU_GFXHUB(0)] = false;
		}
		break;
	default:
		return -EINVAL;
	}

	return 0;
}

 
void amdgpu_vm_get_task_info(struct amdgpu_device *adev, u32 pasid,
			 struct amdgpu_task_info *task_info)
{
	struct amdgpu_vm *vm;
	unsigned long flags;

	xa_lock_irqsave(&adev->vm_manager.pasids, flags);

	vm = xa_load(&adev->vm_manager.pasids, pasid);
	if (vm)
		*task_info = vm->task_info;

	xa_unlock_irqrestore(&adev->vm_manager.pasids, flags);
}

 
void amdgpu_vm_set_task_info(struct amdgpu_vm *vm)
{
	if (vm->task_info.pid)
		return;

	vm->task_info.pid = current->pid;
	get_task_comm(vm->task_info.task_name, current);

	if (current->group_leader->mm != current->mm)
		return;

	vm->task_info.tgid = current->group_leader->pid;
	get_task_comm(vm->task_info.process_name, current->group_leader);
}

 
bool amdgpu_vm_handle_fault(struct amdgpu_device *adev, u32 pasid,
			    u32 vmid, u32 node_id, uint64_t addr,
			    bool write_fault)
{
	bool is_compute_context = false;
	struct amdgpu_bo *root;
	unsigned long irqflags;
	uint64_t value, flags;
	struct amdgpu_vm *vm;
	int r;

	xa_lock_irqsave(&adev->vm_manager.pasids, irqflags);
	vm = xa_load(&adev->vm_manager.pasids, pasid);
	if (vm) {
		root = amdgpu_bo_ref(vm->root.bo);
		is_compute_context = vm->is_compute_context;
	} else {
		root = NULL;
	}
	xa_unlock_irqrestore(&adev->vm_manager.pasids, irqflags);

	if (!root)
		return false;

	addr /= AMDGPU_GPU_PAGE_SIZE;

	if (is_compute_context && !svm_range_restore_pages(adev, pasid, vmid,
	    node_id, addr, write_fault)) {
		amdgpu_bo_unref(&root);
		return true;
	}

	r = amdgpu_bo_reserve(root, true);
	if (r)
		goto error_unref;

	 
	xa_lock_irqsave(&adev->vm_manager.pasids, irqflags);
	vm = xa_load(&adev->vm_manager.pasids, pasid);
	if (vm && vm->root.bo != root)
		vm = NULL;
	xa_unlock_irqrestore(&adev->vm_manager.pasids, irqflags);
	if (!vm)
		goto error_unlock;

	flags = AMDGPU_PTE_VALID | AMDGPU_PTE_SNOOPED |
		AMDGPU_PTE_SYSTEM;

	if (is_compute_context) {
		 
		flags = AMDGPU_VM_NORETRY_FLAGS;
		value = 0;
	} else if (amdgpu_vm_fault_stop == AMDGPU_VM_FAULT_STOP_NEVER) {
		 
		value = adev->dummy_page_addr;
		flags |= AMDGPU_PTE_EXECUTABLE | AMDGPU_PTE_READABLE |
			AMDGPU_PTE_WRITEABLE;

	} else {
		 
		value = 0;
	}

	r = dma_resv_reserve_fences(root->tbo.base.resv, 1);
	if (r) {
		pr_debug("failed %d to reserve fence slot\n", r);
		goto error_unlock;
	}

	r = amdgpu_vm_update_range(adev, vm, true, false, false, NULL, addr,
				   addr, flags, value, 0, NULL, NULL, NULL);
	if (r)
		goto error_unlock;

	r = amdgpu_vm_update_pdes(adev, vm, true);

error_unlock:
	amdgpu_bo_unreserve(root);
	if (r < 0)
		DRM_ERROR("Can't handle page fault (%d)\n", r);

error_unref:
	amdgpu_bo_unref(&root);

	return false;
}

#if defined(CONFIG_DEBUG_FS)
 
void amdgpu_debugfs_vm_bo_info(struct amdgpu_vm *vm, struct seq_file *m)
{
	struct amdgpu_bo_va *bo_va, *tmp;
	u64 total_idle = 0;
	u64 total_evicted = 0;
	u64 total_relocated = 0;
	u64 total_moved = 0;
	u64 total_invalidated = 0;
	u64 total_done = 0;
	unsigned int total_idle_objs = 0;
	unsigned int total_evicted_objs = 0;
	unsigned int total_relocated_objs = 0;
	unsigned int total_moved_objs = 0;
	unsigned int total_invalidated_objs = 0;
	unsigned int total_done_objs = 0;
	unsigned int id = 0;

	spin_lock(&vm->status_lock);
	seq_puts(m, "\tIdle BOs:\n");
	list_for_each_entry_safe(bo_va, tmp, &vm->idle, base.vm_status) {
		if (!bo_va->base.bo)
			continue;
		total_idle += amdgpu_bo_print_info(id++, bo_va->base.bo, m);
	}
	total_idle_objs = id;
	id = 0;

	seq_puts(m, "\tEvicted BOs:\n");
	list_for_each_entry_safe(bo_va, tmp, &vm->evicted, base.vm_status) {
		if (!bo_va->base.bo)
			continue;
		total_evicted += amdgpu_bo_print_info(id++, bo_va->base.bo, m);
	}
	total_evicted_objs = id;
	id = 0;

	seq_puts(m, "\tRelocated BOs:\n");
	list_for_each_entry_safe(bo_va, tmp, &vm->relocated, base.vm_status) {
		if (!bo_va->base.bo)
			continue;
		total_relocated += amdgpu_bo_print_info(id++, bo_va->base.bo, m);
	}
	total_relocated_objs = id;
	id = 0;

	seq_puts(m, "\tMoved BOs:\n");
	list_for_each_entry_safe(bo_va, tmp, &vm->moved, base.vm_status) {
		if (!bo_va->base.bo)
			continue;
		total_moved += amdgpu_bo_print_info(id++, bo_va->base.bo, m);
	}
	total_moved_objs = id;
	id = 0;

	seq_puts(m, "\tInvalidated BOs:\n");
	list_for_each_entry_safe(bo_va, tmp, &vm->invalidated, base.vm_status) {
		if (!bo_va->base.bo)
			continue;
		total_invalidated += amdgpu_bo_print_info(id++,	bo_va->base.bo, m);
	}
	total_invalidated_objs = id;
	id = 0;

	seq_puts(m, "\tDone BOs:\n");
	list_for_each_entry_safe(bo_va, tmp, &vm->done, base.vm_status) {
		if (!bo_va->base.bo)
			continue;
		total_done += amdgpu_bo_print_info(id++, bo_va->base.bo, m);
	}
	spin_unlock(&vm->status_lock);
	total_done_objs = id;

	seq_printf(m, "\tTotal idle size:        %12lld\tobjs:\t%d\n", total_idle,
		   total_idle_objs);
	seq_printf(m, "\tTotal evicted size:     %12lld\tobjs:\t%d\n", total_evicted,
		   total_evicted_objs);
	seq_printf(m, "\tTotal relocated size:   %12lld\tobjs:\t%d\n", total_relocated,
		   total_relocated_objs);
	seq_printf(m, "\tTotal moved size:       %12lld\tobjs:\t%d\n", total_moved,
		   total_moved_objs);
	seq_printf(m, "\tTotal invalidated size: %12lld\tobjs:\t%d\n", total_invalidated,
		   total_invalidated_objs);
	seq_printf(m, "\tTotal done size:        %12lld\tobjs:\t%d\n", total_done,
		   total_done_objs);
}
#endif
