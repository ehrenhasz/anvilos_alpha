 
 

#include <linux/firmware.h>
#include <linux/module.h>
#include <linux/mmu_notifier.h>

#include <drm/drm.h>

#include "radeon.h"

 
static bool radeon_mn_invalidate(struct mmu_interval_notifier *mn,
				 const struct mmu_notifier_range *range,
				 unsigned long cur_seq)
{
	struct radeon_bo *bo = container_of(mn, struct radeon_bo, notifier);
	struct ttm_operation_ctx ctx = { false, false };
	long r;

	if (!bo->tbo.ttm || !radeon_ttm_tt_is_bound(bo->tbo.bdev, bo->tbo.ttm))
		return true;

	if (!mmu_notifier_range_blockable(range))
		return false;

	r = radeon_bo_reserve(bo, true);
	if (r) {
		DRM_ERROR("(%ld) failed to reserve user bo\n", r);
		return true;
	}

	r = dma_resv_wait_timeout(bo->tbo.base.resv, DMA_RESV_USAGE_BOOKKEEP,
				  false, MAX_SCHEDULE_TIMEOUT);
	if (r <= 0)
		DRM_ERROR("(%ld) failed to wait for user bo\n", r);

	radeon_ttm_placement_from_domain(bo, RADEON_GEM_DOMAIN_CPU);
	r = ttm_bo_validate(&bo->tbo, &bo->placement, &ctx);
	if (r)
		DRM_ERROR("(%ld) failed to validate user bo\n", r);

	radeon_bo_unreserve(bo);
	return true;
}

static const struct mmu_interval_notifier_ops radeon_mn_ops = {
	.invalidate = radeon_mn_invalidate,
};

 
int radeon_mn_register(struct radeon_bo *bo, unsigned long addr)
{
	int ret;

	ret = mmu_interval_notifier_insert(&bo->notifier, current->mm, addr,
					   radeon_bo_size(bo), &radeon_mn_ops);
	if (ret)
		return ret;

	 
	mmu_interval_read_begin(&bo->notifier);
	return 0;
}

 
void radeon_mn_unregister(struct radeon_bo *bo)
{
	if (!bo->notifier.mm)
		return;
	mmu_interval_notifier_remove(&bo->notifier);
	bo->notifier.mm = NULL;
}
