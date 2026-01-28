#ifndef DAL_DC_DCE_DCE110_CLK_MGR_H_
#define DAL_DC_DCE_DCE110_CLK_MGR_H_
void dce110_clk_mgr_construct(
		struct dc_context *ctx,
		struct clk_mgr_internal *clk_mgr);
void dce110_fill_display_configs(
	const struct dc_state *context,
	struct dm_pp_display_configuration *pp_display_cfg);
void dce11_pplib_apply_display_requirements(
	struct dc *dc,
	struct dc_state *context);
uint32_t dce110_get_min_vblank_time_us(const struct dc_state *context);
#endif  
