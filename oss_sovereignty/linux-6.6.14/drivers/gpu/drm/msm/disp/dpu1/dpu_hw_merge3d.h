 
 

#ifndef _DPU_HW_MERGE3D_H
#define _DPU_HW_MERGE3D_H

#include "dpu_hw_catalog.h"
#include "dpu_hw_mdss.h"
#include "dpu_hw_util.h"

struct dpu_hw_merge_3d;

 
struct dpu_hw_merge_3d_ops {
	void (*setup_3d_mode)(struct dpu_hw_merge_3d *merge_3d,
			enum dpu_3d_blend_mode mode_3d);

};

struct dpu_hw_merge_3d {
	struct dpu_hw_blk base;
	struct dpu_hw_blk_reg_map hw;

	 
	enum dpu_merge_3d idx;
	const struct dpu_merge_3d_cfg *caps;

	 
	struct dpu_hw_merge_3d_ops ops;
};

 
static inline struct dpu_hw_merge_3d *to_dpu_hw_merge_3d(struct dpu_hw_blk *hw)
{
	return container_of(hw, struct dpu_hw_merge_3d, base);
}

 
struct dpu_hw_merge_3d *dpu_hw_merge_3d_init(const struct dpu_merge_3d_cfg *cfg,
		void __iomem *addr);

 
void dpu_hw_merge_3d_destroy(struct dpu_hw_merge_3d *pp);

#endif  
