


#ifndef __FSL_PAMU_DOMAIN_H
#define __FSL_PAMU_DOMAIN_H

#include "fsl_pamu.h"

struct fsl_dma_domain {
	
	struct list_head		devices;
	u32				stash_id;
	struct iommu_domain		iommu_domain;
	spinlock_t			domain_lock;
};


struct device_domain_info {
	struct list_head link;	
	struct device *dev;
	u32 liodn;
	struct fsl_dma_domain *domain; 
};
#endif  
