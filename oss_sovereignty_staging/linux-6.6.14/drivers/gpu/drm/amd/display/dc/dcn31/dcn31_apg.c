 


#include "dc_bios_types.h"
#include "hw_shared.h"
#include "dcn31_apg.h"
#include "reg_helper.h"

#define DC_LOGGER \
		apg31->base.ctx->logger

#define REG(reg)\
	(apg31->regs->reg)

#undef FN
#define FN(reg_name, field_name) \
	apg31->apg_shift->field_name, apg31->apg_mask->field_name


#define CTX \
	apg31->base.ctx


static void apg31_enable(
	struct apg *apg)
{
	struct dcn31_apg *apg31 = DCN31_APG_FROM_APG(apg);

	 
	REG_UPDATE(APG_CONTROL, APG_RESET, 1);
	REG_WAIT(APG_CONTROL,
			APG_RESET_DONE, 1,
			1, 10);
	REG_UPDATE(APG_CONTROL, APG_RESET, 0);
	REG_WAIT(APG_CONTROL,
			APG_RESET_DONE, 0,
			1, 10);

	 
	REG_UPDATE(APG_CONTROL2, APG_ENABLE, 1);
}

static void apg31_disable(
	struct apg *apg)
{
	struct dcn31_apg *apg31 = DCN31_APG_FROM_APG(apg);

	 
	REG_UPDATE(APG_CONTROL2, APG_ENABLE, 0);
}

static void apg31_se_audio_setup(
	struct apg *apg,
	unsigned int az_inst,
	struct audio_info *audio_info)
{
	struct dcn31_apg *apg31 = DCN31_APG_FROM_APG(apg);

	ASSERT(audio_info);
	 
	if (audio_info == NULL)
		return;

	 
	REG_UPDATE(APG_CONTROL2, APG_DP_AUDIO_STREAM_ID, 0);

	 
	REG_UPDATE(APG_DBG_GEN_CONTROL, APG_DBG_AUDIO_CHANNEL_ENABLE, 0xFF);

	 
	REG_UPDATE(APG_MEM_PWR, APG_MEM_PWR_FORCE, 0);
}

static struct apg_funcs dcn31_apg_funcs = {
	.se_audio_setup			= apg31_se_audio_setup,
	.enable_apg			= apg31_enable,
	.disable_apg			= apg31_disable,
};

void apg31_construct(struct dcn31_apg *apg31,
	struct dc_context *ctx,
	uint32_t inst,
	const struct dcn31_apg_registers *apg_regs,
	const struct dcn31_apg_shift *apg_shift,
	const struct dcn31_apg_mask *apg_mask)
{
	apg31->base.ctx = ctx;

	apg31->base.inst = inst;
	apg31->base.funcs = &dcn31_apg_funcs;

	apg31->regs = apg_regs;
	apg31->apg_shift = apg_shift;
	apg31->apg_mask = apg_mask;
}
