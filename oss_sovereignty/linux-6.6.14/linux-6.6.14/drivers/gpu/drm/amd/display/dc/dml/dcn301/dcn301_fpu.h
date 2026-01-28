#ifndef __DCN301_FPU_H__
#define __DCN301_FPU_H__
void dcn301_update_bw_bounding_box(struct dc *dc, struct clk_bw_params *bw_params);
void dcn301_fpu_set_wm_ranges(int i,
	struct pp_smu_wm_range_sets *ranges,
	struct _vcs_dpi_soc_bounding_box_st *loaded_bb);
void dcn301_fpu_init_soc_bounding_box(struct bp_soc_bb_info bb_info);
void dcn301_calculate_wm_and_dlg_fp(struct dc *dc,
		struct dc_state *context,
		display_e2e_pipe_params_st *pipes,
		int pipe_cnt,
		int vlevel_req);
#endif  
