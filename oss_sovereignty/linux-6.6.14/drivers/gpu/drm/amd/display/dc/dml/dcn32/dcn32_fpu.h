 
 

#ifndef __DCN32_FPU_H__
#define __DCN32_FPU_H__

#include "clk_mgr_internal.h"

void dcn32_build_wm_range_table_fpu(struct clk_mgr_internal *clk_mgr);

void dcn32_helper_populate_phantom_dlg_params(struct dc *dc,
					      struct dc_state *context,
					      display_e2e_pipe_params_st *pipes,
					      int pipe_cnt);

uint8_t dcn32_predict_pipe_split(struct dc_state *context,
				  display_e2e_pipe_params_st *pipe_e2e);

void dcn32_set_phantom_stream_timing(struct dc *dc,
				     struct dc_state *context,
				     struct pipe_ctx *ref_pipe,
				     struct dc_stream_state *phantom_stream,
				     display_e2e_pipe_params_st *pipes,
				     unsigned int pipe_cnt,
				     unsigned int dc_pipe_idx);

bool dcn32_internal_validate_bw(struct dc *dc,
				struct dc_state *context,
				display_e2e_pipe_params_st *pipes,
				int *pipe_cnt_out,
				int *vlevel_out,
				bool fast_validate);

void dcn32_calculate_wm_and_dlg_fpu(struct dc *dc, struct dc_state *context,
				display_e2e_pipe_params_st *pipes,
				int pipe_cnt,
				int vlevel);

void dcn32_update_bw_bounding_box_fpu(struct dc *dc, struct clk_bw_params *bw_params);

int dcn32_find_dummy_latency_index_for_fw_based_mclk_switch(struct dc *dc,
							    struct dc_state *context,
							    display_e2e_pipe_params_st *pipes,
							    int pipe_cnt,
							    int vlevel);

void dcn32_patch_dpm_table(struct clk_bw_params *bw_params);

void dcn32_zero_pipe_dcc_fraction(display_e2e_pipe_params_st *pipes,
				  int pipe_cnt);

void dcn32_assign_fpo_vactive_candidate(struct dc *dc, const struct dc_state *context, struct dc_stream_state **fpo_candidate_stream);

bool dcn32_find_vactive_pipe(struct dc *dc, const struct dc_state *context, uint32_t vactive_margin_req);

void dcn32_override_min_req_memclk(struct dc *dc, struct dc_state *context);

void dcn32_set_clock_limits(const struct _vcs_dpi_soc_bounding_box_st *soc_bb);

#endif
