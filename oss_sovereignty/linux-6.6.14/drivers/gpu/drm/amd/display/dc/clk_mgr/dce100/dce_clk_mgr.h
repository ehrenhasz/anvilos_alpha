 


#ifndef _DCE_CLK_MGR_H_
#define _DCE_CLK_MGR_H_

#include "dc.h"

 
int dce_adjust_dp_ref_freq_for_ss(struct clk_mgr_internal *clk_mgr_dce, int dp_ref_clk_khz);
int dce_get_dp_ref_freq_khz(struct clk_mgr *clk_mgr_base);
enum dm_pp_clocks_state dce_get_required_clocks_state(
	struct clk_mgr *clk_mgr_base,
	struct dc_state *context);

uint32_t dce_get_max_pixel_clock_for_all_paths(struct dc_state *context);


void dce_clk_mgr_construct(
		struct dc_context *ctx,
		struct clk_mgr_internal *clk_mgr_dce);

void dce_clock_read_ss_info(struct clk_mgr_internal *dccg_dce);

int dce12_get_dp_ref_freq_khz(struct clk_mgr *dccg);

int dce_set_clock(
	struct clk_mgr *clk_mgr_base,
	int requested_clk_khz);


void dce_clk_mgr_destroy(struct clk_mgr **clk_mgr);

int dentist_get_divider_from_did(int did);

#endif  
