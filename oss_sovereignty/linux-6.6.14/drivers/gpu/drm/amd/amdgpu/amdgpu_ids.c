 
#include "amdgpu_ids.h"

#include <linux/idr.h>
#include <linux/dma-fence-array.h>


#include "amdgpu.h"
#include "amdgpu_trace.h"

 
static DEFINE_IDA(amdgpu_pasid_ida);

 
struct amdgpu_pasid_cb {
	struct dma_fence_cb cb;
	u32 pasid;
};

 
int amdgpu_pasid_alloc(unsigned int bits)
{
	int pasid = -EINVAL;

	for (bits = min(bits, 31U); bits > 0; bits--) {
		pasid = ida_simple_get(&amdgpu_pasid_ida,
				       1U << (bits - 1), 1U << bits,
				       GFP_KERNEL);
		if (pasid != -ENOSPC)
			break;
	}

	if (pasid >= 0)
		trace_amdgpu_pasid_allocated(pasid);

	return pasid;
}

 
void amdgpu_pasid_free(u32 pasid)
{
	trace_amdgpu_pasid_freed(pasid);
	ida_simple_remove(&amdgpu_pasid_ida, pasid);
}

static void amdgpu_pasid_free_cb(struct dma_fence *fence,
				 struct dma_fence_cb *_cb)
{
	struct amdgpu_pasid_cb *cb =
		container_of(_cb, struct amdgpu_pasid_cb, cb);

	amdgpu_pasid_free(cb->pasid);
	dma_fence_put(fence);
	kfree(cb);
}

 
void amdgpu_pasid_free_delayed(struct dma_resv *resv,
			       u32 pasid)
{
	struct amdgpu_pasid_cb *cb;
	struct dma_fence *fence;
	int r;

	r = dma_resv_get_singleton(resv, DMA_RESV_USAGE_BOOKKEEP, &fence);
	if (r)
		goto fallback;

	if (!fence) {
		amdgpu_pasid_free(pasid);
		return;
	}

	cb = kmalloc(sizeof(*cb), GFP_KERNEL);
	if (!cb) {
		 
		dma_fence_wait(fence, false);
		dma_fence_put(fence);
		amdgpu_pasid_free(pasid);
	} else {
		cb->pasid = pasid;
		if (dma_fence_add_callback(fence, &cb->cb,
					   amdgpu_pasid_free_cb))
			amdgpu_pasid_free_cb(fence, &cb->cb);
	}

	return;

fallback:
	 
	dma_resv_wait_timeout(resv, DMA_RESV_USAGE_BOOKKEEP,
			      false, MAX_SCHEDULE_TIMEOUT);
	amdgpu_pasid_free(pasid);
}

 

 
bool amdgpu_vmid_had_gpu_reset(struct amdgpu_device *adev,
			       struct amdgpu_vmid *id)
{
	return id->current_gpu_reset_count !=
		atomic_read(&adev->gpu_reset_counter);
}

 
static bool amdgpu_vmid_gds_switch_needed(struct amdgpu_vmid *id,
					  struct amdgpu_job *job)
{
	return id->gds_base != job->gds_base ||
		id->gds_size != job->gds_size ||
		id->gws_base != job->gws_base ||
		id->gws_size != job->gws_size ||
		id->oa_base != job->oa_base ||
		id->oa_size != job->oa_size;
}

 
static bool amdgpu_vmid_compatible(struct amdgpu_vmid *id,
				   struct amdgpu_job *job)
{
	return  id->pd_gpu_addr == job->vm_pd_addr &&
		!amdgpu_vmid_gds_switch_needed(id, job);
}

 
static int amdgpu_vmid_grab_idle(struct amdgpu_vm *vm,
				 struct amdgpu_ring *ring,
				 struct amdgpu_vmid **idle,
				 struct dma_fence **fence)
{
	struct amdgpu_device *adev = ring->adev;
	unsigned vmhub = ring->vm_hub;
	struct amdgpu_vmid_mgr *id_mgr = &adev->vm_manager.id_mgr[vmhub];
	struct dma_fence **fences;
	unsigned i;

	if (!dma_fence_is_signaled(ring->vmid_wait)) {
		*fence = dma_fence_get(ring->vmid_wait);
		return 0;
	}

	fences = kmalloc_array(id_mgr->num_ids, sizeof(void *), GFP_KERNEL);
	if (!fences)
		return -ENOMEM;

	 
	i = 0;
	list_for_each_entry((*idle), &id_mgr->ids_lru, list) {
		 
		struct amdgpu_ring *r = adev->vm_manager.concurrent_flush ?
			NULL : ring;

		fences[i] = amdgpu_sync_peek_fence(&(*idle)->active, r);
		if (!fences[i])
			break;
		++i;
	}

	 
	if (&(*idle)->list == &id_mgr->ids_lru) {
		u64 fence_context = adev->vm_manager.fence_context + ring->idx;
		unsigned seqno = ++adev->vm_manager.seqno[ring->idx];
		struct dma_fence_array *array;
		unsigned j;

		*idle = NULL;
		for (j = 0; j < i; ++j)
			dma_fence_get(fences[j]);

		array = dma_fence_array_create(i, fences, fence_context,
					       seqno, true);
		if (!array) {
			for (j = 0; j < i; ++j)
				dma_fence_put(fences[j]);
			kfree(fences);
			return -ENOMEM;
		}

		*fence = dma_fence_get(&array->base);
		dma_fence_put(ring->vmid_wait);
		ring->vmid_wait = &array->base;
		return 0;
	}
	kfree(fences);

	return 0;
}

 
static int amdgpu_vmid_grab_reserved(struct amdgpu_vm *vm,
				     struct amdgpu_ring *ring,
				     struct amdgpu_job *job,
				     struct amdgpu_vmid **id,
				     struct dma_fence **fence)
{
	struct amdgpu_device *adev = ring->adev;
	unsigned vmhub = ring->vm_hub;
	struct amdgpu_vmid_mgr *id_mgr = &adev->vm_manager.id_mgr[vmhub];
	uint64_t fence_context = adev->fence_context + ring->idx;
	bool needs_flush = vm->use_cpu_for_update;
	uint64_t updates = amdgpu_vm_tlb_seq(vm);
	int r;

	*id = id_mgr->reserved;
	if ((*id)->owner != vm->immediate.fence_context ||
	    !amdgpu_vmid_compatible(*id, job) ||
	    (*id)->flushed_updates < updates ||
	    !(*id)->last_flush ||
	    ((*id)->last_flush->context != fence_context &&
	     !dma_fence_is_signaled((*id)->last_flush))) {
		struct dma_fence *tmp;

		 
		if (adev->vm_manager.concurrent_flush)
			ring = NULL;

		 
		(*id)->pd_gpu_addr = 0;
		tmp = amdgpu_sync_peek_fence(&(*id)->active, ring);
		if (tmp) {
			*id = NULL;
			*fence = dma_fence_get(tmp);
			return 0;
		}
		needs_flush = true;
	}

	 
	r = amdgpu_sync_fence(&(*id)->active, &job->base.s_fence->finished);
	if (r)
		return r;

	job->vm_needs_flush = needs_flush;
	job->spm_update_needed = true;
	return 0;
}

 
static int amdgpu_vmid_grab_used(struct amdgpu_vm *vm,
				 struct amdgpu_ring *ring,
				 struct amdgpu_job *job,
				 struct amdgpu_vmid **id,
				 struct dma_fence **fence)
{
	struct amdgpu_device *adev = ring->adev;
	unsigned vmhub = ring->vm_hub;
	struct amdgpu_vmid_mgr *id_mgr = &adev->vm_manager.id_mgr[vmhub];
	uint64_t fence_context = adev->fence_context + ring->idx;
	uint64_t updates = amdgpu_vm_tlb_seq(vm);
	int r;

	job->vm_needs_flush = vm->use_cpu_for_update;

	 
	list_for_each_entry_reverse((*id), &id_mgr->ids_lru, list) {
		bool needs_flush = vm->use_cpu_for_update;

		 
		if ((*id)->owner != vm->immediate.fence_context)
			continue;

		if (!amdgpu_vmid_compatible(*id, job))
			continue;

		if (!(*id)->last_flush ||
		    ((*id)->last_flush->context != fence_context &&
		     !dma_fence_is_signaled((*id)->last_flush)))
			needs_flush = true;

		if ((*id)->flushed_updates < updates)
			needs_flush = true;

		if (needs_flush && !adev->vm_manager.concurrent_flush)
			continue;

		 
		r = amdgpu_sync_fence(&(*id)->active,
				      &job->base.s_fence->finished);
		if (r)
			return r;

		job->vm_needs_flush |= needs_flush;
		return 0;
	}

	*id = NULL;
	return 0;
}

 
int amdgpu_vmid_grab(struct amdgpu_vm *vm, struct amdgpu_ring *ring,
		     struct amdgpu_job *job, struct dma_fence **fence)
{
	struct amdgpu_device *adev = ring->adev;
	unsigned vmhub = ring->vm_hub;
	struct amdgpu_vmid_mgr *id_mgr = &adev->vm_manager.id_mgr[vmhub];
	struct amdgpu_vmid *idle = NULL;
	struct amdgpu_vmid *id = NULL;
	int r = 0;

	mutex_lock(&id_mgr->lock);
	r = amdgpu_vmid_grab_idle(vm, ring, &idle, fence);
	if (r || !idle)
		goto error;

	if (vm->reserved_vmid[vmhub] || (enforce_isolation && (vmhub == AMDGPU_GFXHUB(0)))) {
		r = amdgpu_vmid_grab_reserved(vm, ring, job, &id, fence);
		if (r || !id)
			goto error;
	} else {
		r = amdgpu_vmid_grab_used(vm, ring, job, &id, fence);
		if (r)
			goto error;

		if (!id) {
			 
			id = idle;

			 
			r = amdgpu_sync_fence(&id->active,
					      &job->base.s_fence->finished);
			if (r)
				goto error;

			job->vm_needs_flush = true;
		}

		list_move_tail(&id->list, &id_mgr->ids_lru);
	}

	job->gds_switch_needed = amdgpu_vmid_gds_switch_needed(id, job);
	if (job->vm_needs_flush) {
		id->flushed_updates = amdgpu_vm_tlb_seq(vm);
		dma_fence_put(id->last_flush);
		id->last_flush = NULL;
	}
	job->vmid = id - id_mgr->ids;
	job->pasid = vm->pasid;

	id->gds_base = job->gds_base;
	id->gds_size = job->gds_size;
	id->gws_base = job->gws_base;
	id->gws_size = job->gws_size;
	id->oa_base = job->oa_base;
	id->oa_size = job->oa_size;
	id->pd_gpu_addr = job->vm_pd_addr;
	id->owner = vm->immediate.fence_context;

	trace_amdgpu_vm_grab_id(vm, ring, job);

error:
	mutex_unlock(&id_mgr->lock);
	return r;
}

int amdgpu_vmid_alloc_reserved(struct amdgpu_device *adev,
			       unsigned vmhub)
{
	struct amdgpu_vmid_mgr *id_mgr = &adev->vm_manager.id_mgr[vmhub];

	mutex_lock(&id_mgr->lock);

	++id_mgr->reserved_use_count;
	if (!id_mgr->reserved) {
		struct amdgpu_vmid *id;

		id = list_first_entry(&id_mgr->ids_lru, struct amdgpu_vmid,
				      list);
		 
		list_del_init(&id->list);
		id_mgr->reserved = id;
	}

	mutex_unlock(&id_mgr->lock);
	return 0;
}

void amdgpu_vmid_free_reserved(struct amdgpu_device *adev,
			       unsigned vmhub)
{
	struct amdgpu_vmid_mgr *id_mgr = &adev->vm_manager.id_mgr[vmhub];

	mutex_lock(&id_mgr->lock);
	if (!--id_mgr->reserved_use_count) {
		 
		list_add(&id_mgr->reserved->list, &id_mgr->ids_lru);
		id_mgr->reserved = NULL;
	}

	mutex_unlock(&id_mgr->lock);
}

 
void amdgpu_vmid_reset(struct amdgpu_device *adev, unsigned vmhub,
		       unsigned vmid)
{
	struct amdgpu_vmid_mgr *id_mgr = &adev->vm_manager.id_mgr[vmhub];
	struct amdgpu_vmid *id = &id_mgr->ids[vmid];

	mutex_lock(&id_mgr->lock);
	id->owner = 0;
	id->gds_base = 0;
	id->gds_size = 0;
	id->gws_base = 0;
	id->gws_size = 0;
	id->oa_base = 0;
	id->oa_size = 0;
	mutex_unlock(&id_mgr->lock);
}

 
void amdgpu_vmid_reset_all(struct amdgpu_device *adev)
{
	unsigned i, j;

	for (i = 0; i < AMDGPU_MAX_VMHUBS; ++i) {
		struct amdgpu_vmid_mgr *id_mgr =
			&adev->vm_manager.id_mgr[i];

		for (j = 1; j < id_mgr->num_ids; ++j)
			amdgpu_vmid_reset(adev, i, j);
	}
}

 
void amdgpu_vmid_mgr_init(struct amdgpu_device *adev)
{
	unsigned i, j;

	for (i = 0; i < AMDGPU_MAX_VMHUBS; ++i) {
		struct amdgpu_vmid_mgr *id_mgr =
			&adev->vm_manager.id_mgr[i];

		mutex_init(&id_mgr->lock);
		INIT_LIST_HEAD(&id_mgr->ids_lru);
		id_mgr->reserved_use_count = 0;

		 
		id_mgr->num_ids = adev->vm_manager.first_kfd_vmid;

		 
		for (j = 1; j < id_mgr->num_ids; ++j) {
			amdgpu_vmid_reset(adev, i, j);
			amdgpu_sync_create(&id_mgr->ids[j].active);
			list_add_tail(&id_mgr->ids[j].list, &id_mgr->ids_lru);
		}
	}
	 
	if (enforce_isolation)
		amdgpu_vmid_alloc_reserved(adev, AMDGPU_GFXHUB(0));

}

 
void amdgpu_vmid_mgr_fini(struct amdgpu_device *adev)
{
	unsigned i, j;

	for (i = 0; i < AMDGPU_MAX_VMHUBS; ++i) {
		struct amdgpu_vmid_mgr *id_mgr =
			&adev->vm_manager.id_mgr[i];

		mutex_destroy(&id_mgr->lock);
		for (j = 0; j < AMDGPU_NUM_VMID; ++j) {
			struct amdgpu_vmid *id = &id_mgr->ids[j];

			amdgpu_sync_free(&id->active);
			dma_fence_put(id->last_flush);
			dma_fence_put(id->pasid_mapping);
		}
	}
}
