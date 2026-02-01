 
 

#ifndef _DPU_HW_WB_H
#define _DPU_HW_WB_H

#include "dpu_hw_catalog.h"
#include "dpu_hw_mdss.h"
#include "dpu_hw_top.h"
#include "dpu_hw_util.h"
#include "dpu_hw_pingpong.h"

struct dpu_hw_wb;

struct dpu_hw_wb_cfg {
	struct dpu_hw_fmt_layout dest;
	enum dpu_intf_mode intf_mode;
	struct drm_rect roi;
	struct drm_rect crop;
};

 
struct dpu_hw_wb_ops {
	void (*setup_outaddress)(struct dpu_hw_wb *ctx,
			struct dpu_hw_wb_cfg *wb);

	void (*setup_outformat)(struct dpu_hw_wb *ctx,
			struct dpu_hw_wb_cfg *wb);

	void (*setup_roi)(struct dpu_hw_wb *ctx,
			struct dpu_hw_wb_cfg *wb);

	void (*setup_qos_lut)(struct dpu_hw_wb *ctx,
			struct dpu_hw_qos_cfg *cfg);

	void (*setup_cdp)(struct dpu_hw_wb *ctx,
			  const struct dpu_format *fmt,
			  bool enable);

	void (*bind_pingpong_blk)(struct dpu_hw_wb *ctx,
				  const enum dpu_pingpong pp);
};

 
struct dpu_hw_wb {
	struct dpu_hw_blk_reg_map hw;

	 
	int idx;
	const struct dpu_wb_cfg *caps;

	 
	struct dpu_hw_wb_ops ops;
};

 
struct dpu_hw_wb *dpu_hw_wb_init(const struct dpu_wb_cfg *cfg,
		void __iomem *addr);

 
void dpu_hw_wb_destroy(struct dpu_hw_wb *hw_wb);

#endif  
