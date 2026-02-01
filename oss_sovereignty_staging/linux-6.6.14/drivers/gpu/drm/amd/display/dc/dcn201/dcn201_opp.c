 

#include "dm_services.h"
#include "dcn201_opp.h"
#include "reg_helper.h"

#define REG(reg) \
	(oppn201->regs->reg)

#undef FN
#define FN(reg_name, field_name) \
	oppn201->opp_shift->field_name, oppn201->opp_mask->field_name

#define CTX \
	oppn201->base.ctx

 
 
 

static struct opp_funcs dcn201_opp_funcs = {
		.opp_set_dyn_expansion = opp1_set_dyn_expansion,
		.opp_program_fmt = opp1_program_fmt,
		.opp_program_bit_depth_reduction = opp1_program_bit_depth_reduction,
		.opp_program_stereo = opp1_program_stereo,
		.opp_pipe_clock_control = opp1_pipe_clock_control,
		.opp_set_disp_pattern_generator = opp2_set_disp_pattern_generator,
		.opp_program_dpg_dimensions = opp2_program_dpg_dimensions,
		.dpg_is_blanked = opp2_dpg_is_blanked,
		.opp_dpg_set_blank_color = opp2_dpg_set_blank_color,
		.opp_destroy = opp1_destroy,
		.opp_program_left_edge_extra_pixel = opp2_program_left_edge_extra_pixel,
};

void dcn201_opp_construct(struct dcn201_opp *oppn201,
	struct dc_context *ctx,
	uint32_t inst,
	const struct dcn201_opp_registers *regs,
	const struct dcn201_opp_shift *opp_shift,
	const struct dcn201_opp_mask *opp_mask)
{
	oppn201->base.ctx = ctx;
	oppn201->base.inst = inst;
	oppn201->base.funcs = &dcn201_opp_funcs;

	oppn201->regs = regs;
	oppn201->opp_shift = opp_shift;
	oppn201->opp_mask = opp_mask;
}
