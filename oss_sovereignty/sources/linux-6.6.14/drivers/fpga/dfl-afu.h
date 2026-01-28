


#ifndef __DFL_AFU_H
#define __DFL_AFU_H

#include <linux/mm.h>

#include "dfl.h"


struct dfl_afu_mmio_region {
	u32 index;
	u32 flags;
	u64 size;
	u64 offset;
	u64 phys;
	struct list_head node;
};


struct dfl_afu_dma_region {
	u64 user_addr;
	u64 length;
	u64 iova;
	struct page **pages;
	struct rb_node node;
	bool in_use;
};


struct dfl_afu {
	u64 region_cur_offset;
	int num_regions;
	u8 num_umsgs;
	struct list_head regions;
	struct rb_root dma_regions;

	struct dfl_feature_platform_data *pdata;
};


int __afu_port_enable(struct platform_device *pdev);
int __afu_port_disable(struct platform_device *pdev);

void afu_mmio_region_init(struct dfl_feature_platform_data *pdata);
int afu_mmio_region_add(struct dfl_feature_platform_data *pdata,
			u32 region_index, u64 region_size, u64 phys, u32 flags);
void afu_mmio_region_destroy(struct dfl_feature_platform_data *pdata);
int afu_mmio_region_get_by_index(struct dfl_feature_platform_data *pdata,
				 u32 region_index,
				 struct dfl_afu_mmio_region *pregion);
int afu_mmio_region_get_by_offset(struct dfl_feature_platform_data *pdata,
				  u64 offset, u64 size,
				  struct dfl_afu_mmio_region *pregion);
void afu_dma_region_init(struct dfl_feature_platform_data *pdata);
void afu_dma_region_destroy(struct dfl_feature_platform_data *pdata);
int afu_dma_map_region(struct dfl_feature_platform_data *pdata,
		       u64 user_addr, u64 length, u64 *iova);
int afu_dma_unmap_region(struct dfl_feature_platform_data *pdata, u64 iova);
struct dfl_afu_dma_region *
afu_dma_region_find(struct dfl_feature_platform_data *pdata,
		    u64 iova, u64 size);

extern const struct dfl_feature_ops port_err_ops;
extern const struct dfl_feature_id port_err_id_table[];
extern const struct attribute_group port_err_group;

#endif 
