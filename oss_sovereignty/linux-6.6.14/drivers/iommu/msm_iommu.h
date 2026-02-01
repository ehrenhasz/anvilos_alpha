 
 

#ifndef MSM_IOMMU_H
#define MSM_IOMMU_H

#include <linux/interrupt.h>
#include <linux/iommu.h>
#include <linux/clk.h>

 
#define MSM_IOMMU_ATTR_NON_SH		0x0
#define MSM_IOMMU_ATTR_SH		0x4

 
#define MSM_IOMMU_ATTR_NONCACHED	0x0
#define MSM_IOMMU_ATTR_CACHED_WB_WA	0x1
#define MSM_IOMMU_ATTR_CACHED_WB_NWA	0x2
#define MSM_IOMMU_ATTR_CACHED_WT	0x3

 
#define MSM_IOMMU_CP_MASK		0x03

 
#define MAX_NUM_MIDS	32

 
#define IOMMU_MAX_CBS	128

 
struct msm_iommu_dev {
	void __iomem *base;
	int ncb;
	struct device *dev;
	int irq;
	struct clk *clk;
	struct clk *pclk;
	struct list_head dev_node;
	struct list_head dom_node;
	struct list_head ctx_list;
	DECLARE_BITMAP(context_map, IOMMU_MAX_CBS);

	struct iommu_device iommu;
};

 
struct msm_iommu_ctx_dev {
	struct device_node *of_node;
	int num;
	int mids[MAX_NUM_MIDS];
	int num_mids;
	struct list_head list;
};

 
irqreturn_t msm_iommu_fault_handler(int irq, void *dev_id);

#endif
