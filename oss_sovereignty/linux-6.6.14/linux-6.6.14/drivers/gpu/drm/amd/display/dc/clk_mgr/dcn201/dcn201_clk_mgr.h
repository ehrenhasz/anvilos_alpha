#ifndef __DCN201_CLK_MGR_H__
#define __DCN201_CLK_MGR_H__
void dcn201_clk_mgr_construct(struct dc_context *ctx,
		struct clk_mgr_internal *clk_mgr,
		struct pp_smu_funcs *pp_smu,
		struct dccg *dccg);
#endif  
