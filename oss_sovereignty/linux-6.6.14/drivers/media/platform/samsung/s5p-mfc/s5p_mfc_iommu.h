 
 

#ifndef S5P_MFC_IOMMU_H_
#define S5P_MFC_IOMMU_H_

#if defined(CONFIG_EXYNOS_IOMMU)

#include <linux/iommu.h>

static inline bool exynos_is_iommu_available(struct device *dev)
{
	return dev_iommu_priv_get(dev) != NULL;
}

#else

static inline bool exynos_is_iommu_available(struct device *dev)
{
	return false;
}

#endif

#endif  
