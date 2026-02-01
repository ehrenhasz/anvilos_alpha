 

#ifndef DAL_DC_DCE_DCE112_CLK_MGR_H_
#define DAL_DC_DCE_DCE112_CLK_MGR_H_


void dce112_clk_mgr_construct(
		struct dc_context *ctx,
		struct clk_mgr_internal *clk_mgr);

 
int dce112_set_clock(struct clk_mgr *clk_mgr_base, int requested_clk_khz);
int dce112_set_dispclk(struct clk_mgr_internal *clk_mgr, int requested_clk_khz);
int dce112_set_dprefclk(struct clk_mgr_internal *clk_mgr);

#endif  
