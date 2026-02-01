 

#ifndef __DCN316_CLK_MGR_H__
#define __DCN316_CLK_MGR_H__
#include "clk_mgr_internal.h"

struct dcn316_watermarks;

struct dcn316_smu_watermark_set {
	struct dcn316_watermarks *wm_set;
	union large_integer mc_address;
};

struct clk_mgr_dcn316 {
	struct clk_mgr_internal base;
	struct dcn316_smu_watermark_set smu_wm_set;
};

void dcn316_clk_mgr_construct(struct dc_context *ctx,
		struct clk_mgr_dcn316 *clk_mgr,
		struct pp_smu_funcs *pp_smu,
		struct dccg *dccg);

void dcn316_clk_mgr_destroy(struct clk_mgr_internal *clk_mgr_int);

#endif 
