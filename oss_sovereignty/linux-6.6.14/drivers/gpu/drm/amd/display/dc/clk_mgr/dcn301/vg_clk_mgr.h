 

#ifndef __VG_CLK_MGR_H__
#define __VG_CLK_MGR_H__
#include "clk_mgr_internal.h"

struct watermarks;

extern struct wm_table ddr4_wm_table;
extern struct wm_table lpddr5_wm_table;

struct smu_watermark_set {
	struct watermarks *wm_set;
	union large_integer mc_address;
};

struct clk_mgr_vgh {
	struct clk_mgr_internal base;
	struct smu_watermark_set smu_wm_set;
};

void vg_clk_mgr_construct(struct dc_context *ctx,
		struct clk_mgr_vgh *clk_mgr,
		struct pp_smu_funcs *pp_smu,
		struct dccg *dccg);

void vg_clk_mgr_destroy(struct clk_mgr_internal *clk_mgr);

#endif 
