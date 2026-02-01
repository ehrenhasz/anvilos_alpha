 
 

#ifndef __DCN314_FPU_H__
#define __DCN314_FPU_H__

#define DCN3_14_DEFAULT_DET_SIZE 384
#define DCN3_14_MAX_DET_SIZE 384
#define DCN3_14_MIN_COMPBUF_SIZE_KB 128
#define DCN3_14_CRB_SEGMENT_SIZE_KB 64

void dcn314_update_bw_bounding_box_fpu(struct dc *dc, struct clk_bw_params *bw_params);
int dcn314_populate_dml_pipes_from_context_fpu(struct dc *dc, struct dc_state *context,
					       display_e2e_pipe_params_st *pipes,
					       bool fast_validate);

#endif
