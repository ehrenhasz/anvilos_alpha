 


#include "reg_helper.h"
#include "resource.h"
#include "mcif_wb.h"
#include "dcn20_mmhubbub.h"


#define REG(reg)\
	mcif_wb20->mcif_wb_regs->reg

#define CTX \
	mcif_wb20->base.ctx

#undef FN
#define FN(reg_name, field_name) \
	mcif_wb20->mcif_wb_shift->field_name, mcif_wb20->mcif_wb_mask->field_name

#define MCIF_ADDR(addr) (((unsigned long long)addr & 0xffffffffff) + 0xFE) >> 8
#define MCIF_ADDR_HIGH(addr) (unsigned long long)addr >> 40

 

static void mmhubbub2_config_mcif_buf(struct mcif_wb *mcif_wb,
		struct mcif_buf_params *params,
		unsigned int dest_height)
{
	struct dcn20_mmhubbub *mcif_wb20 = TO_DCN20_MMHUBBUB(mcif_wb);

	 
	REG_UPDATE(MCIF_WB_BUFMGR_SW_CONTROL, MCIF_WB_BUFMGR_SW_LOCK, params->swlock);

	 
	REG_UPDATE(MCIF_WB_BUF_1_ADDR_Y, MCIF_WB_BUF_1_ADDR_Y, MCIF_ADDR(params->luma_address[0]));
	REG_UPDATE(MCIF_WB_BUF_1_ADDR_Y_HIGH, MCIF_WB_BUF_1_ADDR_Y_HIGH, MCIF_ADDR_HIGH(params->luma_address[0]));
	 
	REG_UPDATE(MCIF_WB_BUF_1_ADDR_Y_OFFSET, MCIF_WB_BUF_1_ADDR_Y_OFFSET, 0);

	 
	REG_UPDATE(MCIF_WB_BUF_1_ADDR_C, MCIF_WB_BUF_1_ADDR_C, MCIF_ADDR(params->chroma_address[0]));
	REG_UPDATE(MCIF_WB_BUF_1_ADDR_C_HIGH, MCIF_WB_BUF_1_ADDR_C_HIGH, MCIF_ADDR_HIGH(params->chroma_address[0]));
	 
	REG_UPDATE(MCIF_WB_BUF_1_ADDR_C_OFFSET, MCIF_WB_BUF_1_ADDR_C_OFFSET, 0);

	 
	REG_UPDATE(MCIF_WB_BUF_2_ADDR_Y, MCIF_WB_BUF_2_ADDR_Y, MCIF_ADDR(params->luma_address[1]));
	REG_UPDATE(MCIF_WB_BUF_2_ADDR_Y_HIGH, MCIF_WB_BUF_2_ADDR_Y_HIGH, MCIF_ADDR_HIGH(params->luma_address[1]));
	 
	REG_UPDATE(MCIF_WB_BUF_2_ADDR_Y_OFFSET, MCIF_WB_BUF_2_ADDR_Y_OFFSET, 0);

	 
	REG_UPDATE(MCIF_WB_BUF_2_ADDR_C, MCIF_WB_BUF_2_ADDR_C, MCIF_ADDR(params->chroma_address[1]));
	REG_UPDATE(MCIF_WB_BUF_2_ADDR_C_HIGH, MCIF_WB_BUF_2_ADDR_C_HIGH, MCIF_ADDR_HIGH(params->chroma_address[1]));
	 
	REG_UPDATE(MCIF_WB_BUF_2_ADDR_C_OFFSET, MCIF_WB_BUF_2_ADDR_C_OFFSET, 0);

	 
	REG_UPDATE(MCIF_WB_BUF_3_ADDR_Y, MCIF_WB_BUF_3_ADDR_Y, MCIF_ADDR(params->luma_address[2]));
	REG_UPDATE(MCIF_WB_BUF_3_ADDR_Y_HIGH, MCIF_WB_BUF_3_ADDR_Y_HIGH, MCIF_ADDR_HIGH(params->luma_address[2]));
	 
	REG_UPDATE(MCIF_WB_BUF_3_ADDR_Y_OFFSET, MCIF_WB_BUF_3_ADDR_Y_OFFSET, 0);

	 
	REG_UPDATE(MCIF_WB_BUF_3_ADDR_C, MCIF_WB_BUF_3_ADDR_C, MCIF_ADDR(params->chroma_address[2]));
	REG_UPDATE(MCIF_WB_BUF_3_ADDR_C_HIGH, MCIF_WB_BUF_3_ADDR_C_HIGH, MCIF_ADDR_HIGH(params->chroma_address[2]));
	 
	REG_UPDATE(MCIF_WB_BUF_3_ADDR_C_OFFSET, MCIF_WB_BUF_3_ADDR_C_OFFSET, 0);

	 
	REG_UPDATE(MCIF_WB_BUF_4_ADDR_Y, MCIF_WB_BUF_4_ADDR_Y, MCIF_ADDR(params->luma_address[3]));
	REG_UPDATE(MCIF_WB_BUF_4_ADDR_Y_HIGH, MCIF_WB_BUF_4_ADDR_Y_HIGH, MCIF_ADDR_HIGH(params->luma_address[3]));
	 
	REG_UPDATE(MCIF_WB_BUF_4_ADDR_Y_OFFSET, MCIF_WB_BUF_4_ADDR_Y_OFFSET, 0);

	 
	REG_UPDATE(MCIF_WB_BUF_4_ADDR_C, MCIF_WB_BUF_4_ADDR_C, MCIF_ADDR(params->chroma_address[3]));
	REG_UPDATE(MCIF_WB_BUF_4_ADDR_C_HIGH, MCIF_WB_BUF_4_ADDR_C_HIGH, MCIF_ADDR_HIGH(params->chroma_address[3]));
	 
	REG_UPDATE(MCIF_WB_BUF_4_ADDR_C_OFFSET, MCIF_WB_BUF_4_ADDR_C_OFFSET, 0);

	 
	REG_UPDATE(MCIF_WB_BUF_LUMA_SIZE, MCIF_WB_BUF_LUMA_SIZE, (params->luma_pitch>>8) * dest_height);
	REG_UPDATE(MCIF_WB_BUF_CHROMA_SIZE, MCIF_WB_BUF_CHROMA_SIZE, (params->chroma_pitch>>8) * dest_height);

	 
	REG_UPDATE(MCIF_WB_BUFMGR_SW_CONTROL, MCIF_WB_BUF_ADDR_FENCE_EN, 1);

	 
	REG_UPDATE_2(MCIF_WB_BUF_PITCH, MCIF_WB_BUF_LUMA_PITCH, params->luma_pitch >> 8,
			MCIF_WB_BUF_CHROMA_PITCH, params->chroma_pitch >> 8);

	 
	 
	 
	REG_UPDATE(MCIF_WB_WARM_UP_CNTL, MCIF_WB_PITCH_SIZE_WARMUP, params->warmup_pitch);
}

static void mmhubbub2_config_mcif_arb(struct mcif_wb *mcif_wb,
		struct mcif_arb_params *params)
{
	struct dcn20_mmhubbub *mcif_wb20 = TO_DCN20_MMHUBBUB(mcif_wb);

	 
	REG_UPDATE(MCIF_WB_ARBITRATION_CONTROL, MCIF_WB_TIME_PER_PIXEL, params->time_per_pixel);

	 
	 
	 
	REG_UPDATE(MCIF_WB_SCLK_CHANGE, MCIF_WB_CLI_WATERMARK_MASK, 0x0);
	 
	REG_UPDATE(MCIF_WB_WATERMARK, MCIF_WB_CLI_WATERMARK,  params->cli_watermark[0]);
	REG_UPDATE(MCIF_WB_SCLK_CHANGE, MCIF_WB_CLI_WATERMARK_MASK, 0x1);
	 
	REG_UPDATE(MCIF_WB_WATERMARK, MCIF_WB_CLI_WATERMARK,  params->cli_watermark[1]);
	REG_UPDATE(MCIF_WB_SCLK_CHANGE, MCIF_WB_CLI_WATERMARK_MASK, 0x2);
	 
	REG_UPDATE(MCIF_WB_WATERMARK, MCIF_WB_CLI_WATERMARK,  params->cli_watermark[2]);
	REG_UPDATE(MCIF_WB_SCLK_CHANGE, MCIF_WB_CLI_WATERMARK_MASK, 0x3);
	 
	REG_UPDATE(MCIF_WB_WATERMARK, MCIF_WB_CLI_WATERMARK,  params->cli_watermark[3]);

	 
	 
	REG_UPDATE(MCIF_WB_NB_PSTATE_CONTROL, NB_PSTATE_CHANGE_WATERMARK_MASK, 0x0);
	REG_UPDATE(MCIF_WB_NB_PSTATE_LATENCY_WATERMARK,
			NB_PSTATE_CHANGE_REFRESH_WATERMARK, params->pstate_watermark[0]);
	 
	REG_UPDATE(MCIF_WB_NB_PSTATE_CONTROL, NB_PSTATE_CHANGE_WATERMARK_MASK, 0x1);
	REG_UPDATE(MCIF_WB_NB_PSTATE_LATENCY_WATERMARK,
			NB_PSTATE_CHANGE_REFRESH_WATERMARK, params->pstate_watermark[1]);
	 
	REG_UPDATE(MCIF_WB_NB_PSTATE_CONTROL, NB_PSTATE_CHANGE_WATERMARK_MASK, 0x2);
	REG_UPDATE(MCIF_WB_NB_PSTATE_LATENCY_WATERMARK,
			NB_PSTATE_CHANGE_REFRESH_WATERMARK, params->pstate_watermark[2]);
	 
	REG_UPDATE(MCIF_WB_NB_PSTATE_CONTROL, NB_PSTATE_CHANGE_WATERMARK_MASK, 0x3);
	REG_UPDATE(MCIF_WB_NB_PSTATE_LATENCY_WATERMARK,
			NB_PSTATE_CHANGE_REFRESH_WATERMARK, params->pstate_watermark[3]);

	 
	REG_UPDATE(MULTI_LEVEL_QOS_CTRL, MAX_SCALED_TIME_TO_URGENT, params->max_scaled_time);

	 
	REG_UPDATE(MCIF_WB_BUFMGR_VCE_CONTROL, MCIF_WB_BUFMGR_SLICE_SIZE, params->slice_lines-1);

	 
	 
	 
	REG_UPDATE(MCIF_WB_ARBITRATION_CONTROL, MCIF_WB_CLIENT_ARBITRATION_SLICE,  params->arbitration_slice);
}

void mmhubbub2_config_mcif_irq(struct mcif_wb *mcif_wb,
		struct mcif_irq_params *params)
{
	struct dcn20_mmhubbub *mcif_wb20 = TO_DCN20_MMHUBBUB(mcif_wb);

	 
	REG_UPDATE(MCIF_WB_BUFMGR_SW_CONTROL, MCIF_WB_BUFMGR_SW_INT_EN, params->sw_int_en);
	REG_UPDATE(MCIF_WB_BUFMGR_SW_CONTROL, MCIF_WB_BUFMGR_SW_SLICE_INT_EN, params->sw_slice_int_en);
	REG_UPDATE(MCIF_WB_BUFMGR_SW_CONTROL, MCIF_WB_BUFMGR_SW_OVERRUN_INT_EN,  params->sw_overrun_int_en);

	REG_UPDATE(MCIF_WB_BUFMGR_VCE_CONTROL, MCIF_WB_BUFMGR_VCE_INT_EN,  params->vce_int_en);
	if (mcif_wb20->mcif_wb_mask->MCIF_WB_BUFMGR_VCE_SLICE_INT_EN)
		REG_UPDATE(MCIF_WB_BUFMGR_VCE_CONTROL, MCIF_WB_BUFMGR_VCE_SLICE_INT_EN,  params->vce_slice_int_en);
}

void mmhubbub2_enable_mcif(struct mcif_wb *mcif_wb)
{
	struct dcn20_mmhubbub *mcif_wb20 = TO_DCN20_MMHUBBUB(mcif_wb);

	 
	REG_UPDATE(MCIF_WB_BUFMGR_SW_CONTROL, MCIF_WB_BUFMGR_ENABLE, 1);
}

void mmhubbub2_disable_mcif(struct mcif_wb *mcif_wb)
{
	struct dcn20_mmhubbub *mcif_wb20 = TO_DCN20_MMHUBBUB(mcif_wb);

	 
	REG_UPDATE(MCIF_WB_BUFMGR_SW_CONTROL, MCIF_WB_BUFMGR_ENABLE, 0);
}

 
 
 
 
 
 

void mcifwb2_dump_frame(struct mcif_wb *mcif_wb,
		struct mcif_buf_params *mcif_params,
		enum dwb_scaler_mode out_format,
		unsigned int dest_width,
		unsigned int dest_height,
		struct mcif_wb_frame_dump_info *dump_info,
		unsigned char *luma_buffer,
		unsigned char *chroma_buffer,
		unsigned char *dest_luma_buffer,
		unsigned char *dest_chroma_buffer)
{
	struct dcn20_mmhubbub *mcif_wb20 = TO_DCN20_MMHUBBUB(mcif_wb);

	REG_UPDATE(MCIF_WB_BUFMGR_SW_CONTROL, MCIF_WB_BUFMGR_SW_LOCK, 0xf);

	memcpy(dest_luma_buffer,   luma_buffer,   mcif_params->luma_pitch * dest_height);
	memcpy(dest_chroma_buffer, chroma_buffer, mcif_params->chroma_pitch * dest_height / 2);

	REG_UPDATE(MCIF_WB_BUFMGR_SW_CONTROL, MCIF_WB_BUFMGR_SW_LOCK, 0x0);

	dump_info->format	= out_format;
	dump_info->width	= dest_width;
	dump_info->height	= dest_height;
	dump_info->luma_pitch	= mcif_params->luma_pitch;
	dump_info->chroma_pitch	= mcif_params->chroma_pitch;
	dump_info->size		= dest_height * (mcif_params->luma_pitch + mcif_params->chroma_pitch);
}

static const struct mcif_wb_funcs dcn20_mmhubbub_funcs = {
	.enable_mcif		= mmhubbub2_enable_mcif,
	.disable_mcif		= mmhubbub2_disable_mcif,
	.config_mcif_buf	= mmhubbub2_config_mcif_buf,
	.config_mcif_arb	= mmhubbub2_config_mcif_arb,
	.config_mcif_irq	= mmhubbub2_config_mcif_irq,
	.dump_frame		= mcifwb2_dump_frame,
};

void dcn20_mmhubbub_construct(struct dcn20_mmhubbub *mcif_wb20,
		struct dc_context *ctx,
		const struct dcn20_mmhubbub_registers *mcif_wb_regs,
		const struct dcn20_mmhubbub_shift *mcif_wb_shift,
		const struct dcn20_mmhubbub_mask *mcif_wb_mask,
		int inst)
{
	mcif_wb20->base.ctx = ctx;

	mcif_wb20->base.inst = inst;
	mcif_wb20->base.funcs = &dcn20_mmhubbub_funcs;

	mcif_wb20->mcif_wb_regs = mcif_wb_regs;
	mcif_wb20->mcif_wb_shift = mcif_wb_shift;
	mcif_wb20->mcif_wb_mask = mcif_wb_mask;
}
