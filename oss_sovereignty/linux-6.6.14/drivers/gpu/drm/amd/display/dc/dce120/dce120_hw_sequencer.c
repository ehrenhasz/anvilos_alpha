 

#include "dm_services.h"
#include "dc.h"
#include "core_types.h"
#include "dce120_hw_sequencer.h"
#include "dce/dce_hwseq.h"

#include "dce110/dce110_hw_sequencer.h"

#include "dce/dce_12_0_offset.h"
#include "dce/dce_12_0_sh_mask.h"
#include "soc15_hw_ip.h"
#include "vega10_ip_offset.h"
#include "reg_helper.h"

#define CTX \
	hws->ctx
#define REG(reg)\
	hws->regs->reg

#undef FN
#define FN(reg_name, field_name) \
	hws->shifts->field_name, hws->masks->field_name

struct dce120_hw_seq_reg_offsets {
	uint32_t crtc;
};

#if 0
static const struct dce120_hw_seq_reg_offsets reg_offsets[] = {
{
	.crtc = (mmCRTC0_CRTC_GSL_CONTROL - mmCRTC0_CRTC_GSL_CONTROL),
},
{
	.crtc = (mmCRTC1_CRTC_GSL_CONTROL - mmCRTC0_CRTC_GSL_CONTROL),
},
{
	.crtc = (mmCRTC2_CRTC_GSL_CONTROL - mmCRTC0_CRTC_GSL_CONTROL),
},
{
	.crtc = (mmCRTC3_CRTC_GSL_CONTROL - mmCRTC0_CRTC_GSL_CONTROL),
},
{
	.crtc = (mmCRTC4_CRTC_GSL_CONTROL - mmCRTC0_CRTC_GSL_CONTROL),
},
{
	.crtc = (mmCRTC5_CRTC_GSL_CONTROL - mmCRTC0_CRTC_GSL_CONTROL),
}
};

#define HW_REG_CRTC(reg, id)\
	(reg + reg_offsets[id].crtc)

#define CNTL_ID(controller_id)\
	controller_id
 
static void dce120_init_pte(struct dc_context *ctx, uint8_t controller_id)
{
	uint32_t addr;
	uint32_t value = 0;
	uint32_t chunk_int = 0;
	uint32_t chunk_mul = 0;
 

	addr = mmDVMM_PTE_REQ;
	value = dm_read_reg(ctx, addr);

	chunk_int = get_reg_field_value(
		value,
		DVMM_PTE_REQ,
		HFLIP_PTEREQ_PER_CHUNK_INT);

	chunk_mul = get_reg_field_value(
		value,
		DVMM_PTE_REQ,
		HFLIP_PTEREQ_PER_CHUNK_MULTIPLIER);

	if (chunk_int != 0x4 || chunk_mul != 0x4) {

		set_reg_field_value(
			value,
			255,
			DVMM_PTE_REQ,
			MAX_PTEREQ_TO_ISSUE);

		set_reg_field_value(
			value,
			4,
			DVMM_PTE_REQ,
			HFLIP_PTEREQ_PER_CHUNK_INT);

		set_reg_field_value(
			value,
			4,
			DVMM_PTE_REQ,
			HFLIP_PTEREQ_PER_CHUNK_MULTIPLIER);

		dm_write_reg(ctx, addr, value);
	}
}
#endif

static bool dce120_enable_display_power_gating(
	struct dc *dc,
	uint8_t controller_id,
	struct dc_bios *dcb,
	enum pipe_gating_control power_gating)
{
	 
#if 0
	enum bp_result bp_result = BP_RESULT_OK;
	enum bp_pipe_control_action cntl;
	struct dc_context *ctx = dc->ctx;

	if (power_gating == PIPE_GATING_CONTROL_INIT)
		cntl = ASIC_PIPE_INIT;
	else if (power_gating == PIPE_GATING_CONTROL_ENABLE)
		cntl = ASIC_PIPE_ENABLE;
	else
		cntl = ASIC_PIPE_DISABLE;

	if (power_gating != PIPE_GATING_CONTROL_INIT || controller_id == 0) {

		bp_result = dcb->funcs->enable_disp_power_gating(
						dcb, controller_id + 1, cntl);

		 
		dm_write_reg(ctx,
			HW_REG_CRTC(mmCRTC0_CRTC_MASTER_UPDATE_MODE, controller_id),
			0);
	}

	if (power_gating != PIPE_GATING_CONTROL_ENABLE)
		dce120_init_pte(ctx, controller_id);

	if (bp_result == BP_RESULT_OK)
		return true;
	else
		return false;
#endif
	return false;
}

static void dce120_update_dchub(
	struct dce_hwseq *hws,
	struct dchub_init_data *dh_data)
{
	 
	switch (dh_data->fb_mode) {
	case FRAME_BUFFER_MODE_ZFB_ONLY:
		 
		REG_UPDATE_2(DCHUB_FB_LOCATION,
				FB_TOP, 0,
				FB_BASE, 0x0FFFF);

		REG_UPDATE(DCHUB_AGP_BASE,
				AGP_BASE, dh_data->zfb_phys_addr_base >> 22);

		REG_UPDATE(DCHUB_AGP_BOT,
				AGP_BOT, dh_data->zfb_mc_base_addr >> 22);

		REG_UPDATE(DCHUB_AGP_TOP,
				AGP_TOP, (dh_data->zfb_mc_base_addr + dh_data->zfb_size_in_byte - 1) >> 22);
		break;
	case FRAME_BUFFER_MODE_MIXED_ZFB_AND_LOCAL:
		 
		REG_UPDATE(DCHUB_AGP_BASE,
				AGP_BASE, dh_data->zfb_phys_addr_base >> 22);

		REG_UPDATE(DCHUB_AGP_BOT,
				AGP_BOT, dh_data->zfb_mc_base_addr >> 22);

		REG_UPDATE(DCHUB_AGP_TOP,
				AGP_TOP, (dh_data->zfb_mc_base_addr + dh_data->zfb_size_in_byte - 1) >> 22);
		break;
	case FRAME_BUFFER_MODE_LOCAL_ONLY:
		 
		REG_UPDATE(DCHUB_AGP_BASE,
				AGP_BASE, 0);

		REG_UPDATE(DCHUB_AGP_BOT,
				AGP_BOT, 0x03FFFF);

		REG_UPDATE(DCHUB_AGP_TOP,
				AGP_TOP, 0);
		break;
	default:
		break;
	}

	dh_data->dchub_initialzied = true;
	dh_data->dchub_info_valid = false;
}

 
bool dce121_xgmi_enabled(struct dce_hwseq *hws)
{
	uint32_t pf_max_region;

	REG_GET(MC_VM_XGMI_LFB_CNTL, PF_MAX_REGION, &pf_max_region);
	 
	return !!pf_max_region;
}

void dce120_hw_sequencer_construct(struct dc *dc)
{
	 
	dce110_hw_sequencer_construct(dc);
	dc->hwseq->funcs.enable_display_power_gating = dce120_enable_display_power_gating;
	dc->hwss.update_dchub = dce120_update_dchub;
}

