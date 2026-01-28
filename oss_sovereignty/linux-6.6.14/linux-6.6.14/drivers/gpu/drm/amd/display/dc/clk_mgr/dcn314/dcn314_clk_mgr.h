#ifndef __DCN314_CLK_MGR_H__
#define __DCN314_CLK_MGR_H__
#include "clk_mgr_internal.h"
struct dcn314_watermarks;
struct dcn314_smu_watermark_set {
	struct dcn314_watermarks *wm_set;
	union large_integer mc_address;
};
struct clk_mgr_dcn314 {
	struct clk_mgr_internal base;
	struct dcn314_smu_watermark_set smu_wm_set;
};
bool dcn314_are_clock_states_equal(struct dc_clocks *a,
		struct dc_clocks *b);
void dcn314_update_clocks(struct clk_mgr *clk_mgr_base,
			struct dc_state *context,
			bool safe_to_lower);
void dcn314_clk_mgr_construct(struct dc_context *ctx,
		struct clk_mgr_dcn314 *clk_mgr,
		struct pp_smu_funcs *pp_smu,
		struct dccg *dccg);
void dcn314_clk_mgr_destroy(struct clk_mgr_internal *clk_mgr_int);
#endif  
