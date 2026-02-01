 
#ifndef __AMDGPU_MN_H__
#define __AMDGPU_MN_H__

#include <linux/types.h>
#include <linux/hmm.h>
#include <linux/rwsem.h>
#include <linux/workqueue.h>
#include <linux/interval_tree.h>
#include <linux/mmu_notifier.h>

int amdgpu_hmm_range_get_pages(struct mmu_interval_notifier *notifier,
			       uint64_t start, uint64_t npages, bool readonly,
			       void *owner, struct page **pages,
			       struct hmm_range **phmm_range);
bool amdgpu_hmm_range_get_pages_done(struct hmm_range *hmm_range);

#if defined(CONFIG_HMM_MIRROR)
int amdgpu_hmm_register(struct amdgpu_bo *bo, unsigned long addr);
void amdgpu_hmm_unregister(struct amdgpu_bo *bo);
#else
static inline int amdgpu_hmm_register(struct amdgpu_bo *bo, unsigned long addr)
{
	DRM_WARN_ONCE("HMM_MIRROR kernel config option is not enabled, "
		      "add CONFIG_ZONE_DEVICE=y in config file to fix this\n");
	return -ENODEV;
}
static inline void amdgpu_hmm_unregister(struct amdgpu_bo *bo) {}
#endif

#endif
