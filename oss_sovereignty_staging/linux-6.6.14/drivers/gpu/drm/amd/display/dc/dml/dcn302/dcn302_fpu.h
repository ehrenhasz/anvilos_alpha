 

#ifndef __DCN302_FPU_H__
#define __DCN302_FPU_H__

void dcn302_fpu_init_soc_bounding_box(struct bp_soc_bb_info bb_info);
void dcn302_fpu_update_bw_bounding_box(struct dc *dc, struct clk_bw_params *bw_params);

#endif  
