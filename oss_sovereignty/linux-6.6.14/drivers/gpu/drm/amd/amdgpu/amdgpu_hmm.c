 
 

 

#include <linux/firmware.h>
#include <linux/module.h>
#include <drm/drm.h>

#include "amdgpu.h"
#include "amdgpu_amdkfd.h"
#include "amdgpu_hmm.h"

#define MAX_WALK_BYTE	(2UL << 30)

 
static bool amdgpu_hmm_invalidate_gfx(struct mmu_interval_notifier *mni,
				      const struct mmu_notifier_range *range,
				      unsigned long cur_seq)
{
	struct amdgpu_bo *bo = container_of(mni, struct amdgpu_bo, notifier);
	struct amdgpu_device *adev = amdgpu_ttm_adev(bo->tbo.bdev);
	long r;

	if (!mmu_notifier_range_blockable(range))
		return false;

	mutex_lock(&adev->notifier_lock);

	mmu_interval_set_seq(mni, cur_seq);

	r = dma_resv_wait_timeout(bo->tbo.base.resv, DMA_RESV_USAGE_BOOKKEEP,
				  false, MAX_SCHEDULE_TIMEOUT);
	mutex_unlock(&adev->notifier_lock);
	if (r <= 0)
		DRM_ERROR("(%ld) failed to wait for user bo\n", r);
	return true;
}

static const struct mmu_interval_notifier_ops amdgpu_hmm_gfx_ops = {
	.invalidate = amdgpu_hmm_invalidate_gfx,
};

 
static bool amdgpu_hmm_invalidate_hsa(struct mmu_interval_notifier *mni,
				      const struct mmu_notifier_range *range,
				      unsigned long cur_seq)
{
	struct amdgpu_bo *bo = container_of(mni, struct amdgpu_bo, notifier);

	if (!mmu_notifier_range_blockable(range))
		return false;

	amdgpu_amdkfd_evict_userptr(mni, cur_seq, bo->kfd_bo);

	return true;
}

static const struct mmu_interval_notifier_ops amdgpu_hmm_hsa_ops = {
	.invalidate = amdgpu_hmm_invalidate_hsa,
};

 
int amdgpu_hmm_register(struct amdgpu_bo *bo, unsigned long addr)
{
	if (bo->kfd_bo)
		return mmu_interval_notifier_insert(&bo->notifier, current->mm,
						    addr, amdgpu_bo_size(bo),
						    &amdgpu_hmm_hsa_ops);
	return mmu_interval_notifier_insert(&bo->notifier, current->mm, addr,
					    amdgpu_bo_size(bo),
					    &amdgpu_hmm_gfx_ops);
}

 
void amdgpu_hmm_unregister(struct amdgpu_bo *bo)
{
	if (!bo->notifier.mm)
		return;
	mmu_interval_notifier_remove(&bo->notifier);
	bo->notifier.mm = NULL;
}

int amdgpu_hmm_range_get_pages(struct mmu_interval_notifier *notifier,
			       uint64_t start, uint64_t npages, bool readonly,
			       void *owner, struct page **pages,
			       struct hmm_range **phmm_range)
{
	struct hmm_range *hmm_range;
	unsigned long end;
	unsigned long timeout;
	unsigned long i;
	unsigned long *pfns;
	int r = 0;

	hmm_range = kzalloc(sizeof(*hmm_range), GFP_KERNEL);
	if (unlikely(!hmm_range))
		return -ENOMEM;

	pfns = kvmalloc_array(npages, sizeof(*pfns), GFP_KERNEL);
	if (unlikely(!pfns)) {
		r = -ENOMEM;
		goto out_free_range;
	}

	hmm_range->notifier = notifier;
	hmm_range->default_flags = HMM_PFN_REQ_FAULT;
	if (!readonly)
		hmm_range->default_flags |= HMM_PFN_REQ_WRITE;
	hmm_range->hmm_pfns = pfns;
	hmm_range->start = start;
	end = start + npages * PAGE_SIZE;
	hmm_range->dev_private_owner = owner;

	do {
		hmm_range->end = min(hmm_range->start + MAX_WALK_BYTE, end);

		pr_debug("hmm range: start = 0x%lx, end = 0x%lx",
			hmm_range->start, hmm_range->end);

		 
		timeout = max((hmm_range->end - hmm_range->start) >> 27, 1UL);
		timeout *= HMM_RANGE_DEFAULT_TIMEOUT;
		timeout = jiffies + msecs_to_jiffies(timeout);

retry:
		hmm_range->notifier_seq = mmu_interval_read_begin(notifier);
		r = hmm_range_fault(hmm_range);
		if (unlikely(r)) {
			 
			if (r == -EBUSY && !time_after(jiffies, timeout))
				goto retry;
			goto out_free_pfns;
		}

		if (hmm_range->end == end)
			break;
		hmm_range->hmm_pfns += MAX_WALK_BYTE >> PAGE_SHIFT;
		hmm_range->start = hmm_range->end;
		schedule();
	} while (hmm_range->end < end);

	hmm_range->start = start;
	hmm_range->hmm_pfns = pfns;

	 
	for (i = 0; pages && i < npages; i++)
		pages[i] = hmm_pfn_to_page(pfns[i]);

	*phmm_range = hmm_range;

	return 0;

out_free_pfns:
	kvfree(pfns);
out_free_range:
	kfree(hmm_range);

	return r;
}

bool amdgpu_hmm_range_get_pages_done(struct hmm_range *hmm_range)
{
	bool r;

	r = mmu_interval_read_retry(hmm_range->notifier,
				    hmm_range->notifier_seq);
	kvfree(hmm_range->hmm_pfns);
	kfree(hmm_range);

	return r;
}
