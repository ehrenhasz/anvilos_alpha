 
#include "dm_services.h"
#include "dcn301_hubbub.h"
#include "reg_helper.h"

#define REG(reg)\
	hubbub1->regs->reg
#define DC_LOGGER \
	hubbub1->base.ctx->logger
#define CTX \
	hubbub1->base.ctx

#undef FN
#define FN(reg_name, field_name) \
	hubbub1->shifts->field_name, hubbub1->masks->field_name

#define REG(reg)\
	hubbub1->regs->reg

#define CTX \
	hubbub1->base.ctx

#undef FN
#define FN(reg_name, field_name) \
	hubbub1->shifts->field_name, hubbub1->masks->field_name


static const struct hubbub_funcs hubbub301_funcs = {
	.update_dchub = hubbub2_update_dchub,
	.init_dchub_sys_ctx = hubbub21_init_dchub,
	.init_vm_ctx = hubbub2_init_vm_ctx,
	.dcc_support_swizzle = hubbub3_dcc_support_swizzle,
	.dcc_support_pixel_format = hubbub2_dcc_support_pixel_format,
	.get_dcc_compression_cap = hubbub3_get_dcc_compression_cap,
	.wm_read_state = hubbub21_wm_read_state,
	.get_dchub_ref_freq = hubbub2_get_dchub_ref_freq,
	.program_watermarks = hubbub3_program_watermarks,
	.allow_self_refresh_control = hubbub1_allow_self_refresh_control,
	.is_allow_self_refresh_enabled = hubbub1_is_allow_self_refresh_enabled,
	.verify_allow_pstate_change_high = hubbub1_verify_allow_pstate_change_high,
	.force_wm_propagate_to_pipes = hubbub3_force_wm_propagate_to_pipes,
	.force_pstate_change_control = hubbub3_force_pstate_change_control,
	.hubbub_read_state = hubbub2_read_state,
};

void hubbub301_construct(struct dcn20_hubbub *hubbub3,
	struct dc_context *ctx,
	const struct dcn_hubbub_registers *hubbub_regs,
	const struct dcn_hubbub_shift *hubbub_shift,
	const struct dcn_hubbub_mask *hubbub_mask)
{
	hubbub3->base.ctx = ctx;
	hubbub3->base.funcs = &hubbub301_funcs;
	hubbub3->regs = hubbub_regs;
	hubbub3->shifts = hubbub_shift;
	hubbub3->masks = hubbub_mask;

	hubbub3->debug_test_index_pstate = 0xB;
	hubbub3->detile_buf_size = 184 * 1024;  
}
