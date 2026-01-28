#ifndef __DCN30_FPU_H__
#define __DCN30_FPU_H__
#include "core_types.h"
#include "dcn20/dcn20_optc.h"
void optc3_fpu_set_vrr_m_const(struct timing_generator *optc,
		double vtotal_avg);
void dcn30_fpu_populate_dml_writeback_from_context(
		struct dc *dc, struct resource_context *res_ctx, display_e2e_pipe_params_st *pipes);
void dcn30_fpu_set_mcif_arb_params(struct mcif_arb_params *wb_arb_params,
	struct display_mode_lib *dml,
	display_e2e_pipe_params_st *pipes,
	int pipe_cnt,
	int cur_pipe);
void dcn30_fpu_update_soc_for_wm_a(struct dc *dc, struct dc_state *context);
void dcn30_fpu_calculate_wm_and_dlg(
		struct dc *dc, struct dc_state *context,
		display_e2e_pipe_params_st *pipes,
		int pipe_cnt,
		int vlevel);
void dcn30_fpu_update_dram_channel_width_bytes(struct dc *dc);
void dcn30_fpu_update_max_clk(struct dc_bounding_box_max_clk *dcn30_bb_max_clk);
void dcn30_fpu_get_optimal_dcfclk_fclk_for_uclk(unsigned int uclk_mts,
		unsigned int *optimal_dcfclk,
		unsigned int *optimal_fclk);
void dcn30_fpu_update_bw_bounding_box(struct dc *dc,
	struct clk_bw_params *bw_params,
	struct dc_bounding_box_max_clk *dcn30_bb_max_clk,
	unsigned int *dcfclk_mhz,
	unsigned int *dram_speed_mts);
int dcn30_find_dummy_latency_index_for_fw_based_mclk_switch(struct dc *dc,
							    struct dc_state *context,
							    display_e2e_pipe_params_st *pipes,
							    int pipe_cnt,
							    int vlevel);
void dcn3_fpu_build_wm_range_table(struct clk_mgr *base);
void patch_dcn30_soc_bounding_box(struct dc *dc, struct _vcs_dpi_soc_bounding_box_st *dcn3_0_ip);
#endif  
