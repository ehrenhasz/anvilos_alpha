 
 
#ifndef __LINUX_IOMMU_PRIV_H
#define __LINUX_IOMMU_PRIV_H

#include <linux/iommu.h>

static inline const struct iommu_ops *dev_iommu_ops(struct device *dev)
{
	 
	return dev->iommu->iommu_dev->ops;
}

int iommu_group_replace_domain(struct iommu_group *group,
			       struct iommu_domain *new_domain);

int iommu_device_register_bus(struct iommu_device *iommu,
			      const struct iommu_ops *ops, struct bus_type *bus,
			      struct notifier_block *nb);
void iommu_device_unregister_bus(struct iommu_device *iommu,
				 struct bus_type *bus,
				 struct notifier_block *nb);

#endif  
