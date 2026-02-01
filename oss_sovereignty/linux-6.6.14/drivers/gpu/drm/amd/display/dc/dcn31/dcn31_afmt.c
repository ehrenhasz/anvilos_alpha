 

#include "dc_bios_types.h"
#include "hw_shared.h"
#include "dcn30/dcn30_afmt.h"
#include "dcn31_afmt.h"
#include "reg_helper.h"
#include "dc/dc.h"

#define DC_LOGGER \
		afmt31->base.ctx->logger

#define REG(reg)\
	(afmt31->regs->reg)

#undef FN
#define FN(reg_name, field_name) \
	afmt31->afmt_shift->field_name, afmt31->afmt_mask->field_name


#define CTX \
	afmt31->base.ctx

static struct afmt_funcs dcn31_afmt_funcs = {
	.setup_hdmi_audio		= afmt3_setup_hdmi_audio,
	.se_audio_setup			= afmt3_se_audio_setup,
	.audio_mute_control		= afmt3_audio_mute_control,
	.audio_info_immediate_update	= afmt3_audio_info_immediate_update,
	.setup_dp_audio			= afmt3_setup_dp_audio,
	.afmt_powerdown			= afmt31_powerdown,
	.afmt_poweron			= afmt31_poweron
};

void afmt31_powerdown(struct afmt *afmt)
{
	struct dcn31_afmt *afmt31 = DCN31_AFMT_FROM_AFMT(afmt);

	if (afmt->ctx->dc->debug.enable_mem_low_power.bits.afmt == false)
		return;

	REG_UPDATE_2(AFMT_MEM_PWR, AFMT_MEM_PWR_DIS, 0, AFMT_MEM_PWR_FORCE, 1);
}

void afmt31_poweron(struct afmt *afmt)
{
	struct dcn31_afmt *afmt31 = DCN31_AFMT_FROM_AFMT(afmt);

	if (afmt->ctx->dc->debug.enable_mem_low_power.bits.afmt == false)
		return;

	REG_UPDATE_2(AFMT_MEM_PWR, AFMT_MEM_PWR_DIS, 1, AFMT_MEM_PWR_FORCE, 0);
}

void afmt31_construct(struct dcn31_afmt *afmt31,
	struct dc_context *ctx,
	uint32_t inst,
	const struct dcn31_afmt_registers *afmt_regs,
	const struct dcn31_afmt_shift *afmt_shift,
	const struct dcn31_afmt_mask *afmt_mask)
{
	afmt31->base.ctx = ctx;

	afmt31->base.inst = inst;
	afmt31->base.funcs = &dcn31_afmt_funcs;

	afmt31->regs = afmt_regs;
	afmt31->afmt_shift = afmt_shift;
	afmt31->afmt_mask = afmt_mask;
}
