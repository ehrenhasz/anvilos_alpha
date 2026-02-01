 

#ifndef __DCN31_FPU_H__
#define __DCN31_FPU_H__

#define DCN3_1_DEFAULT_DET_SIZE 384
#define DCN3_15_DEFAULT_DET_SIZE 192
#define DCN3_15_MIN_COMPBUF_SIZE_KB 128
#define DCN3_16_DEFAULT_DET_SIZE 192

void dcn31_zero_pipe_dcc_fraction(display_e2e_pipe_params_st *pipes,
				  int pipe_cnt);

void dcn31_update_soc_for_wm_a(struct dc *dc, struct dc_state *context);
void dcn315_update_soc_for_wm_a(struct dc *dc, struct dc_state *context);

void dcn31_calculate_wm_and_dlg_fp(
		struct dc *dc, struct dc_state *context,
		display_e2e_pipe_params_st *pipes,
		int pipe_cnt,
		int vlevel);

void dcn31_update_bw_bounding_box(struct dc *dc, struct clk_bw_params *bw_params);
void dcn315_update_bw_bounding_box(struct dc *dc, struct clk_bw_params *bw_params);
void dcn316_update_bw_bounding_box(struct dc *dc, struct clk_bw_params *bw_params);
int dcn_get_max_non_odm_pix_rate_100hz(struct _vcs_dpi_soc_bounding_box_st *soc);
int dcn_get_approx_det_segs_required_for_pstate(
		struct _vcs_dpi_soc_bounding_box_st *soc,
		int pix_clk_100hz, int bpp, int seg_size_kb);

int dcn31x_populate_dml_pipes_from_context(struct dc *dc,
					  struct dc_state *context,
					  display_e2e_pipe_params_st *pipes,
					  bool fast_validate);
#endif  
