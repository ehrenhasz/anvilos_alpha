#ifndef __FSL_PAMU_STASH_H
#define __FSL_PAMU_STASH_H
struct iommu_domain;
enum pamu_stash_target {
	PAMU_ATTR_CACHE_L1 = 1,
	PAMU_ATTR_CACHE_L2,
	PAMU_ATTR_CACHE_L3,
};
int fsl_pamu_configure_l1_stash(struct iommu_domain *domain, u32 cpu);
#endif   
