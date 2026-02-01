 

#ifndef __DCN20_CLK_MGR_H__
#define __DCN20_CLK_MGR_H__

void dcn2_update_clocks(struct clk_mgr *dccg,
			struct dc_state *context,
			bool safe_to_lower);

void dcn2_update_clocks_fpga(struct clk_mgr *clk_mgr,
			struct dc_state *context,
			bool safe_to_lower);
void dcn20_update_clocks_update_dpp_dto(struct clk_mgr_internal *clk_mgr,
		struct dc_state *context, bool safe_to_lower);

void dcn2_init_clocks(struct clk_mgr *clk_mgr);

void dcn20_clk_mgr_construct(struct dc_context *ctx,
		struct clk_mgr_internal *clk_mgr,
		struct pp_smu_funcs *pp_smu,
		struct dccg *dccg);

uint32_t dentist_get_did_from_divider(int divider);

void dcn2_get_clock(struct clk_mgr *clk_mgr,
		struct dc_state *context,
			enum dc_clock_type clock_type,
			struct dc_clock_config *clock_cfg);

void dcn20_update_clocks_update_dentist(struct clk_mgr_internal *clk_mgr,
					struct dc_state *context);

void dcn2_read_clocks_from_hw_dentist(struct clk_mgr *clk_mgr_base);


#endif 
