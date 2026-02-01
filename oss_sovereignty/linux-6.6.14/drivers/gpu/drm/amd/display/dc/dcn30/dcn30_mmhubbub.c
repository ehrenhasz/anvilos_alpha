 


#include "reg_helper.h"
#include "resource.h"
#include "mcif_wb.h"
#include "dcn30_mmhubbub.h"


#define REG(reg)\
	mcif_wb30->mcif_wb_regs->reg

#define CTX \
	mcif_wb30->base.ctx

#undef FN
#define FN(reg_name, field_name) \
	mcif_wb30->mcif_wb_shift->field_name, mcif_wb30->mcif_wb_mask->field_name

#define MCIF_ADDR(addr) (((unsigned long long)addr & 0xffffffffff) + 0xFE) >> 8
#define MCIF_ADDR_HIGH(addr) (unsigned long long)addr >> 40

 

static void mmhubbub3_warmup_mcif(struct mcif_wb *mcif_wb,
		struct mcif_warmup_params *params)
{
	struct dcn30_mmhubbub *mcif_wb30 = TO_DCN30_MMHUBBUB(mcif_wb);
	union large_integer start_address_shift = {.quad_part = params->start_address.quad_part >> 5};

	 
	REG_SET(MMHUBBUB_WARMUP_BASE_ADDR_HIGH, 0, MMHUBBUB_WARMUP_BASE_ADDR_HIGH, start_address_shift.high_part);
	REG_SET(MMHUBBUB_WARMUP_BASE_ADDR_LOW, 0, MMHUBBUB_WARMUP_BASE_ADDR_LOW, start_address_shift.low_part);
	REG_SET(MMHUBBUB_WARMUP_ADDR_REGION, 0, MMHUBBUB_WARMUP_ADDR_REGION, params->region_size >> 5);


	 
	REG_SET_3(MMHUBBUB_WARMUP_CONTROL_STATUS, 0, MMHUBBUB_WARMUP_EN, true,
			MMHUBBUB_WARMUP_SW_INT_EN, true,
			MMHUBBUB_WARMUP_INC_ADDR, params->address_increment >> 5);

	 
	REG_WAIT(MMHUBBUB_WARMUP_CONTROL_STATUS, MMHUBBUB_WARMUP_SW_INT_STATUS, 1, 20, 100);

	 
	REG_UPDATE(MMHUBBUB_WARMUP_CONTROL_STATUS, MMHUBBUB_WARMUP_SW_INT_ACK, 1);

	 
	REG_UPDATE(MMHUBBUB_WARMUP_CONTROL_STATUS, MMHUBBUB_WARMUP_EN, false);
}

static void mmhubbub3_config_mcif_buf(struct mcif_wb *mcif_wb,
		struct mcif_buf_params *params,
		unsigned int dest_height)
{
	struct dcn30_mmhubbub *mcif_wb30 = TO_DCN30_MMHUBBUB(mcif_wb);

	 
	REG_UPDATE(MCIF_WB_BUF_1_ADDR_Y, MCIF_WB_BUF_1_ADDR_Y, MCIF_ADDR(params->luma_address[0]));
	REG_UPDATE(MCIF_WB_BUF_1_ADDR_Y_HIGH, MCIF_WB_BUF_1_ADDR_Y_HIGH, MCIF_ADDR_HIGH(params->luma_address[0]));

	 
	REG_UPDATE(MCIF_WB_BUF_1_ADDR_C, MCIF_WB_BUF_1_ADDR_C, MCIF_ADDR(params->chroma_address[0]));
	REG_UPDATE(MCIF_WB_BUF_1_ADDR_C_HIGH, MCIF_WB_BUF_1_ADDR_C_HIGH, MCIF_ADDR_HIGH(params->chroma_address[0]));

	 
	REG_UPDATE(MCIF_WB_BUF_2_ADDR_Y, MCIF_WB_BUF_2_ADDR_Y, MCIF_ADDR(params->luma_address[1]));
	REG_UPDATE(MCIF_WB_BUF_2_ADDR_Y_HIGH, MCIF_WB_BUF_2_ADDR_Y_HIGH, MCIF_ADDR_HIGH(params->luma_address[1]));

	 
	REG_UPDATE(MCIF_WB_BUF_2_ADDR_C, MCIF_WB_BUF_2_ADDR_C, MCIF_ADDR(params->chroma_address[1]));
	REG_UPDATE(MCIF_WB_BUF_2_ADDR_C_HIGH, MCIF_WB_BUF_2_ADDR_C_HIGH, MCIF_ADDR_HIGH(params->chroma_address[1]));

	 
	REG_UPDATE(MCIF_WB_BUF_3_ADDR_Y, MCIF_WB_BUF_3_ADDR_Y, MCIF_ADDR(params->luma_address[2]));
	REG_UPDATE(MCIF_WB_BUF_3_ADDR_Y_HIGH, MCIF_WB_BUF_3_ADDR_Y_HIGH, MCIF_ADDR_HIGH(params->luma_address[2]));

	 
	REG_UPDATE(MCIF_WB_BUF_3_ADDR_C, MCIF_WB_BUF_3_ADDR_C, MCIF_ADDR(params->chroma_address[2]));
	REG_UPDATE(MCIF_WB_BUF_3_ADDR_C_HIGH, MCIF_WB_BUF_3_ADDR_C_HIGH, MCIF_ADDR_HIGH(params->chroma_address[2]));

	 
	REG_UPDATE(MCIF_WB_BUF_4_ADDR_Y, MCIF_WB_BUF_4_ADDR_Y, MCIF_ADDR(params->luma_address[3]));
	REG_UPDATE(MCIF_WB_BUF_4_ADDR_Y_HIGH, MCIF_WB_BUF_4_ADDR_Y_HIGH, MCIF_ADDR_HIGH(params->luma_address[3]));

	 
	REG_UPDATE(MCIF_WB_BUF_4_ADDR_C, MCIF_WB_BUF_4_ADDR_C, MCIF_ADDR(params->chroma_address[3]));
	REG_UPDATE(MCIF_WB_BUF_4_ADDR_C_HIGH, MCIF_WB_BUF_4_ADDR_C_HIGH, MCIF_ADDR_HIGH(params->chroma_address[3]));

	 
	REG_UPDATE(MCIF_WB_BUF_LUMA_SIZE, MCIF_WB_BUF_LUMA_SIZE, (params->luma_pitch>>8) * dest_height);
	REG_UPDATE(MCIF_WB_BUF_CHROMA_SIZE, MCIF_WB_BUF_CHROMA_SIZE, (params->chroma_pitch>>8) * dest_height);

	 
	REG_UPDATE(MCIF_WB_BUFMGR_SW_CONTROL, MCIF_WB_BUF_ADDR_FENCE_EN, 1);

	 
	REG_UPDATE_2(MCIF_WB_BUF_PITCH, MCIF_WB_BUF_LUMA_PITCH, params->luma_pitch >> 8,
			MCIF_WB_BUF_CHROMA_PITCH, params->chroma_pitch >> 8);
}

static void mmhubbub3_config_mcif_arb(struct mcif_wb *mcif_wb,
		struct mcif_arb_params *params)
{
	struct dcn30_mmhubbub *mcif_wb30 = TO_DCN30_MMHUBBUB(mcif_wb);

	 
	REG_UPDATE(MCIF_WB_ARBITRATION_CONTROL, MCIF_WB_TIME_PER_PIXEL, params->time_per_pixel);

	 
	 
	 
	REG_UPDATE(MCIF_WB_WATERMARK, MCIF_WB_CLI_WATERMARK_MASK, 0x0);
	 
	REG_UPDATE(MCIF_WB_WATERMARK, MCIF_WB_CLI_WATERMARK,  params->cli_watermark[0]);
	REG_UPDATE(MCIF_WB_WATERMARK, MCIF_WB_CLI_WATERMARK_MASK, 0x1);
	 
	REG_UPDATE(MCIF_WB_WATERMARK, MCIF_WB_CLI_WATERMARK,  params->cli_watermark[1]);
	REG_UPDATE(MCIF_WB_WATERMARK, MCIF_WB_CLI_WATERMARK_MASK, 0x2);
	 
	REG_UPDATE(MCIF_WB_WATERMARK, MCIF_WB_CLI_WATERMARK,  params->cli_watermark[2]);
	REG_UPDATE(MCIF_WB_WATERMARK, MCIF_WB_CLI_WATERMARK_MASK, 0x3);
	 
	REG_UPDATE(MCIF_WB_WATERMARK, MCIF_WB_CLI_WATERMARK,  params->cli_watermark[3]);

	 
	 
	REG_UPDATE(MCIF_WB_NB_PSTATE_LATENCY_WATERMARK, NB_PSTATE_CHANGE_WATERMARK_MASK, 0x0);
	REG_UPDATE(MCIF_WB_NB_PSTATE_LATENCY_WATERMARK,
			NB_PSTATE_CHANGE_REFRESH_WATERMARK, params->pstate_watermark[0]);
	 
	REG_UPDATE(MCIF_WB_NB_PSTATE_LATENCY_WATERMARK, NB_PSTATE_CHANGE_WATERMARK_MASK, 0x1);
	REG_UPDATE(MCIF_WB_NB_PSTATE_LATENCY_WATERMARK,
			NB_PSTATE_CHANGE_REFRESH_WATERMARK, params->pstate_watermark[1]);
	 
	REG_UPDATE(MCIF_WB_NB_PSTATE_LATENCY_WATERMARK, NB_PSTATE_CHANGE_WATERMARK_MASK, 0x2);
	REG_UPDATE(MCIF_WB_NB_PSTATE_LATENCY_WATERMARK,
			NB_PSTATE_CHANGE_REFRESH_WATERMARK, params->pstate_watermark[2]);
	 
	REG_UPDATE(MCIF_WB_NB_PSTATE_LATENCY_WATERMARK, NB_PSTATE_CHANGE_WATERMARK_MASK, 0x3);
	REG_UPDATE(MCIF_WB_NB_PSTATE_LATENCY_WATERMARK,
			NB_PSTATE_CHANGE_REFRESH_WATERMARK, params->pstate_watermark[3]);

	 
	REG_UPDATE(MCIF_WB_DRAM_SPEED_CHANGE_DURATION_VBI,
			MCIF_WB_DRAM_SPEED_CHANGE_DURATION_VBI, params->dram_speed_change_duration);

	 
	REG_UPDATE(MULTI_LEVEL_QOS_CTRL, MAX_SCALED_TIME_TO_URGENT, params->max_scaled_time);

	 
	REG_UPDATE(MCIF_WB_BUFMGR_VCE_CONTROL, MCIF_WB_BUFMGR_SLICE_SIZE, params->slice_lines-1);

	 
	 
	 
	REG_UPDATE(MCIF_WB_ARBITRATION_CONTROL, MCIF_WB_CLIENT_ARBITRATION_SLICE,  params->arbitration_slice);
}

static const struct mcif_wb_funcs dcn30_mmhubbub_funcs = {
	.warmup_mcif		= mmhubbub3_warmup_mcif,
	.enable_mcif		= mmhubbub2_enable_mcif,
	.disable_mcif		= mmhubbub2_disable_mcif,
	.config_mcif_buf	= mmhubbub3_config_mcif_buf,
	.config_mcif_arb	= mmhubbub3_config_mcif_arb,
	.config_mcif_irq	= mmhubbub2_config_mcif_irq,
	.dump_frame		= mcifwb2_dump_frame,
};

void dcn30_mmhubbub_construct(struct dcn30_mmhubbub *mcif_wb30,
		struct dc_context *ctx,
		const struct dcn30_mmhubbub_registers *mcif_wb_regs,
		const struct dcn30_mmhubbub_shift *mcif_wb_shift,
		const struct dcn30_mmhubbub_mask *mcif_wb_mask,
		int inst)
{
	mcif_wb30->base.ctx = ctx;

	mcif_wb30->base.inst = inst;
	mcif_wb30->base.funcs = &dcn30_mmhubbub_funcs;

	mcif_wb30->mcif_wb_regs = mcif_wb_regs;
	mcif_wb30->mcif_wb_shift = mcif_wb_shift;
	mcif_wb30->mcif_wb_mask = mcif_wb_mask;
}
