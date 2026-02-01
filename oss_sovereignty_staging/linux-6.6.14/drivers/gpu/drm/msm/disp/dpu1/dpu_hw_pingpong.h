 
 

#ifndef _DPU_HW_PINGPONG_H
#define _DPU_HW_PINGPONG_H

#include "dpu_hw_catalog.h"
#include "dpu_hw_mdss.h"
#include "dpu_hw_util.h"

#define DITHER_MATRIX_SZ 16

struct dpu_hw_pingpong;

 
struct dpu_hw_dither_cfg {
	u64 flags;
	u32 temporal_en;
	u32 c0_bitdepth;
	u32 c1_bitdepth;
	u32 c2_bitdepth;
	u32 c3_bitdepth;
	u32 matrix[DITHER_MATRIX_SZ];
};

 
struct dpu_hw_pingpong_ops {
	 
	int (*enable_tearcheck)(struct dpu_hw_pingpong *pp,
			struct dpu_hw_tear_check *cfg);

	 
	int (*disable_tearcheck)(struct dpu_hw_pingpong *pp);

	 
	int (*connect_external_te)(struct dpu_hw_pingpong *pp,
			bool enable_external_te);

	 
	u32 (*get_line_count)(struct dpu_hw_pingpong *pp);

	 
	void (*disable_autorefresh)(struct dpu_hw_pingpong *pp, uint32_t encoder_id, u16 vdisplay);

	 
	void (*setup_dither)(struct dpu_hw_pingpong *pp,
			struct dpu_hw_dither_cfg *cfg);
	 
	int (*enable_dsc)(struct dpu_hw_pingpong *pp);

	 
	void (*disable_dsc)(struct dpu_hw_pingpong *pp);

	 
	int (*setup_dsc)(struct dpu_hw_pingpong *pp);
};

struct dpu_hw_merge_3d;

struct dpu_hw_pingpong {
	struct dpu_hw_blk base;
	struct dpu_hw_blk_reg_map hw;

	 
	enum dpu_pingpong idx;
	const struct dpu_pingpong_cfg *caps;
	struct dpu_hw_merge_3d *merge_3d;

	 
	struct dpu_hw_pingpong_ops ops;
};

 
static inline struct dpu_hw_pingpong *to_dpu_hw_pingpong(struct dpu_hw_blk *hw)
{
	return container_of(hw, struct dpu_hw_pingpong, base);
}

 
struct dpu_hw_pingpong *dpu_hw_pingpong_init(const struct dpu_pingpong_cfg *cfg,
		void __iomem *addr);

 
void dpu_hw_pingpong_destroy(struct dpu_hw_pingpong *pp);

#endif  
