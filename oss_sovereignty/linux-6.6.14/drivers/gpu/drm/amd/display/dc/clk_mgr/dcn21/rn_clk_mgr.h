 

#ifndef __RN_CLK_MGR_H__
#define __RN_CLK_MGR_H__

#include "clk_mgr.h"
#include "dm_pp_smu.h"
#include "clk_mgr_internal.h"

extern struct wm_table ddr4_wm_table_gs;
extern struct wm_table lpddr4_wm_table_gs;
extern struct wm_table lpddr4_wm_table_with_disabled_ppt;
extern struct wm_table ddr4_wm_table_rn;
extern struct wm_table ddr4_1R_wm_table_rn;
extern struct wm_table lpddr4_wm_table_rn;

struct rn_clk_registers {
	uint32_t CLK1_CLK0_CURRENT_CNT;  
};

void rn_clk_mgr_construct(struct dc_context *ctx,
		struct clk_mgr_internal *clk_mgr,
		struct pp_smu_funcs *pp_smu,
		struct dccg *dccg);

#endif 
