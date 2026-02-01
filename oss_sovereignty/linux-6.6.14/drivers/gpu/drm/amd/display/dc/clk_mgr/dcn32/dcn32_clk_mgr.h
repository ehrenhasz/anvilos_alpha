 
#ifndef __DCN32_CLK_MGR_H_
#define __DCN32_CLK_MGR_H_

void dcn32_init_clocks(struct clk_mgr *clk_mgr_base);

void dcn32_clk_mgr_construct(struct dc_context *ctx,
		struct clk_mgr_internal *clk_mgr,
		struct pp_smu_funcs *pp_smu,
		struct dccg *dccg);

void dcn32_update_clocks_update_dpp_dto(struct clk_mgr_internal *clk_mgr,
		struct dc_state *context, bool safe_to_lower);

void dcn32_clk_mgr_destroy(struct clk_mgr_internal *clk_mgr);



#endif  
