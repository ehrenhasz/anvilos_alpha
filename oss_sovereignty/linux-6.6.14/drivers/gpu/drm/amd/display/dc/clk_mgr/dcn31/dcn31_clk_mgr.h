 

#ifndef __DCN31_CLK_MGR_H__
#define __DCN31_CLK_MGR_H__
#include "clk_mgr_internal.h"

struct dcn31_watermarks;

struct dcn31_smu_watermark_set {
	struct dcn31_watermarks *wm_set;
	union large_integer mc_address;
};

struct clk_mgr_dcn31 {
	struct clk_mgr_internal base;
	struct dcn31_smu_watermark_set smu_wm_set;
};

bool dcn31_are_clock_states_equal(struct dc_clocks *a,
		struct dc_clocks *b);
void dcn31_init_clocks(struct clk_mgr *clk_mgr);
void dcn31_update_clocks(struct clk_mgr *clk_mgr_base,
			struct dc_state *context,
			bool safe_to_lower);

void dcn31_clk_mgr_construct(struct dc_context *ctx,
		struct clk_mgr_dcn31 *clk_mgr,
		struct pp_smu_funcs *pp_smu,
		struct dccg *dccg);

int dcn31_get_dtb_ref_freq_khz(struct clk_mgr *clk_mgr_base);

void dcn31_clk_mgr_destroy(struct clk_mgr_internal *clk_mgr_int);

#endif 
