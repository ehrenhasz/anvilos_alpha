 


#include "dc_bios_types.h"
#include "hw_shared.h"
#include "dcn30_afmt.h"
#include "reg_helper.h"

#define DC_LOGGER \
		afmt3->base.ctx->logger

#define REG(reg)\
	(afmt3->regs->reg)

#undef FN
#define FN(reg_name, field_name) \
	afmt3->afmt_shift->field_name, afmt3->afmt_mask->field_name


#define CTX \
	afmt3->base.ctx


void afmt3_setup_hdmi_audio(
	struct afmt *afmt)
{
	struct dcn30_afmt *afmt3 = DCN30_AFMT_FROM_AFMT(afmt);

	if (afmt->funcs->afmt_poweron)
		afmt->funcs->afmt_poweron(afmt);

	 
	REG_UPDATE(AFMT_AUDIO_PACKET_CONTROL, AFMT_60958_CS_UPDATE, 1);

	 
	REG_UPDATE_2(AFMT_AUDIO_PACKET_CONTROL2,
			AFMT_AUDIO_LAYOUT_OVRD, 0,
			AFMT_60958_OSF_OVRD, 0);

	 
	REG_UPDATE_2(AFMT_60958_0,
			AFMT_60958_CS_CHANNEL_NUMBER_L, 1,
			AFMT_60958_CS_CLOCK_ACCURACY, 0);

	 
	REG_UPDATE(AFMT_60958_1, AFMT_60958_CS_CHANNEL_NUMBER_R, 2);

	 
	REG_UPDATE_6(AFMT_60958_2,
			AFMT_60958_CS_CHANNEL_NUMBER_2, 3,
			AFMT_60958_CS_CHANNEL_NUMBER_3, 4,
			AFMT_60958_CS_CHANNEL_NUMBER_4, 5,
			AFMT_60958_CS_CHANNEL_NUMBER_5, 6,
			AFMT_60958_CS_CHANNEL_NUMBER_6, 7,
			AFMT_60958_CS_CHANNEL_NUMBER_7, 8);
}

static union audio_cea_channels speakers_to_channels(
	struct audio_speaker_flags speaker_flags)
{
	union audio_cea_channels cea_channels = {0};

	 
	cea_channels.channels.FL = speaker_flags.FL_FR;
	cea_channels.channels.FR = speaker_flags.FL_FR;
	cea_channels.channels.LFE = speaker_flags.LFE;
	cea_channels.channels.FC = speaker_flags.FC;

	 
	if (speaker_flags.RL_RR) {
		cea_channels.channels.RL_RC = speaker_flags.RL_RR;
		cea_channels.channels.RR = speaker_flags.RL_RR;
		cea_channels.channels.RC_RLC_FLC = speaker_flags.RC;
	} else {
		cea_channels.channels.RL_RC = speaker_flags.RC;
	}

	 
	if (speaker_flags.FLC_FRC) {
		cea_channels.channels.RC_RLC_FLC = speaker_flags.FLC_FRC;
		cea_channels.channels.RRC_FRC = speaker_flags.FLC_FRC;
	} else {
		cea_channels.channels.RC_RLC_FLC = speaker_flags.RLC_RRC;
		cea_channels.channels.RRC_FRC = speaker_flags.RLC_RRC;
	}

	return cea_channels;
}

void afmt3_se_audio_setup(
	struct afmt *afmt,
	unsigned int az_inst,
	struct audio_info *audio_info)
{
	struct dcn30_afmt *afmt3 = DCN30_AFMT_FROM_AFMT(afmt);

	uint32_t channels = 0;

	ASSERT(audio_info);
	 
	if (audio_info == NULL)
		return;

	channels = speakers_to_channels(audio_info->flags.speaker_flags).all;

	 
	REG_SET(AFMT_AUDIO_SRC_CONTROL, 0, AFMT_AUDIO_SRC_SELECT, az_inst);

	 
	REG_UPDATE(AFMT_AUDIO_PACKET_CONTROL2, AFMT_AUDIO_CHANNEL_ENABLE, channels);

	 
	if (afmt->funcs->afmt_poweron == NULL)
		REG_UPDATE(AFMT_MEM_PWR, AFMT_MEM_PWR_FORCE, 0);
}

void afmt3_audio_mute_control(
	struct afmt *afmt,
	bool mute)
{
	struct dcn30_afmt *afmt3 = DCN30_AFMT_FROM_AFMT(afmt);
	if (mute && afmt->funcs->afmt_powerdown)
		afmt->funcs->afmt_powerdown(afmt);
	if (!mute && afmt->funcs->afmt_poweron)
		afmt->funcs->afmt_poweron(afmt);
	 
	REG_UPDATE(AFMT_AUDIO_PACKET_CONTROL, AFMT_AUDIO_SAMPLE_SEND, !mute);
}

void afmt3_audio_info_immediate_update(
	struct afmt *afmt)
{
	struct dcn30_afmt *afmt3 = DCN30_AFMT_FROM_AFMT(afmt);

	 
	REG_UPDATE(AFMT_INFOFRAME_CONTROL0, AFMT_AUDIO_INFO_UPDATE, 1);
}

void afmt3_setup_dp_audio(
		struct afmt *afmt)
{
	struct dcn30_afmt *afmt3 = DCN30_AFMT_FROM_AFMT(afmt);

	if (afmt->funcs->afmt_poweron)
		afmt->funcs->afmt_poweron(afmt);

	 
	REG_UPDATE(AFMT_AUDIO_PACKET_CONTROL, AFMT_60958_CS_UPDATE, 1);

	 
	 
	REG_UPDATE_2(AFMT_AUDIO_PACKET_CONTROL2,
			AFMT_AUDIO_LAYOUT_OVRD, 0,
			AFMT_60958_OSF_OVRD, 0);

	 
	REG_UPDATE(AFMT_INFOFRAME_CONTROL0, AFMT_AUDIO_INFO_UPDATE, 1);

	 
	REG_UPDATE(AFMT_60958_0, AFMT_60958_CS_CLOCK_ACCURACY, 0);
}

static struct afmt_funcs dcn30_afmt_funcs = {
	.setup_hdmi_audio		= afmt3_setup_hdmi_audio,
	.se_audio_setup			= afmt3_se_audio_setup,
	.audio_mute_control		= afmt3_audio_mute_control,
	.audio_info_immediate_update	= afmt3_audio_info_immediate_update,
	.setup_dp_audio			= afmt3_setup_dp_audio,
};

void afmt3_construct(struct dcn30_afmt *afmt3,
	struct dc_context *ctx,
	uint32_t inst,
	const struct dcn30_afmt_registers *afmt_regs,
	const struct dcn30_afmt_shift *afmt_shift,
	const struct dcn30_afmt_mask *afmt_mask)
{
	afmt3->base.ctx = ctx;

	afmt3->base.inst = inst;
	afmt3->base.funcs = &dcn30_afmt_funcs;

	afmt3->regs = afmt_regs;
	afmt3->afmt_shift = afmt_shift;
	afmt3->afmt_mask = afmt_mask;
}
