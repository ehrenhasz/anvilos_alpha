 

#include "dm_services.h"

#include "dce/dce_11_2_d.h"
#include "dce/dce_11_2_sh_mask.h"
#include "gmc/gmc_8_1_sh_mask.h"
#include "gmc/gmc_8_1_d.h"

#include "include/logger_interface.h"

#include "dce112_compressor.h"
#define DC_LOGGER \
		cp110->base.ctx->logger
#define DCP_REG(reg)\
	(reg + cp110->offsets.dcp_offset)
#define DMIF_REG(reg)\
	(reg + cp110->offsets.dmif_offset)

static const struct dce112_compressor_reg_offsets reg_offsets[] = {
{
	.dcp_offset = (mmDCP0_GRPH_CONTROL - mmDCP0_GRPH_CONTROL),
	.dmif_offset =
		(mmDMIF_PG0_DPG_PIPE_DPM_CONTROL
			- mmDMIF_PG0_DPG_PIPE_DPM_CONTROL),
},
{
	.dcp_offset = (mmDCP1_GRPH_CONTROL - mmDCP0_GRPH_CONTROL),
	.dmif_offset =
		(mmDMIF_PG1_DPG_PIPE_DPM_CONTROL
			- mmDMIF_PG0_DPG_PIPE_DPM_CONTROL),
},
{
	.dcp_offset = (mmDCP2_GRPH_CONTROL - mmDCP0_GRPH_CONTROL),
	.dmif_offset =
		(mmDMIF_PG2_DPG_PIPE_DPM_CONTROL
			- mmDMIF_PG0_DPG_PIPE_DPM_CONTROL),
}
};

static const uint32_t dce11_one_lpt_channel_max_resolution = 2560 * 1600;

enum fbc_idle_force {
	 
	FBC_IDLE_FORCE_DISPLAY_REGISTER_UPDATE = 0x00000001,

	 
	FBC_IDLE_FORCE_GRPH_COMP_EN = 0x00000002,
	 
	FBC_IDLE_FORCE_SRC_SEL_CHANGE = 0x00000004,
	 
	FBC_IDLE_FORCE_MIN_COMPRESSION_CHANGE = 0x00000008,
	 
	FBC_IDLE_FORCE_ALPHA_COMP_EN = 0x00000010,
	 
	FBC_IDLE_FORCE_ZERO_ALPHA_CHUNK_SKIP_EN = 0x00000020,
	 
	FBC_IDLE_FORCE_FORCE_COPY_TO_COMP_BUF = 0x00000040,

	 
	FBC_IDLE_FORCE_MEMORY_WRITE_TO_REGION0 = 0x01000000,
	 
	FBC_IDLE_FORCE_MEMORY_WRITE_TO_REGION1 = 0x02000000,
	 
	FBC_IDLE_FORCE_MEMORY_WRITE_TO_REGION2 = 0x04000000,
	 
	FBC_IDLE_FORCE_MEMORY_WRITE_TO_REGION3 = 0x08000000,

	 
	FBC_IDLE_FORCE_MEMORY_WRITE_OTHER_THAN_MCIF = 0x10000000,
	 
	FBC_IDLE_FORCE_CG_STATIC_SCREEN_IS_INACTIVE = 0x20000000,
};

static uint32_t lpt_size_alignment(struct dce112_compressor *cp110)
{
	 
	return cp110->base.raw_size * cp110->base.banks_num *
		cp110->base.dram_channels_num;
}

static uint32_t lpt_memory_control_config(struct dce112_compressor *cp110,
	uint32_t lpt_control)
{
	 
	if (cp110->base.options.bits.LPT_MC_CONFIG == 1) {
		 
		switch (cp110->base.dram_channels_num) {
		case 2:
			set_reg_field_value(
				lpt_control,
				1,
				LOW_POWER_TILING_CONTROL,
				LOW_POWER_TILING_NUM_PIPES);
			break;
		case 1:
			set_reg_field_value(
				lpt_control,
				0,
				LOW_POWER_TILING_CONTROL,
				LOW_POWER_TILING_NUM_PIPES);
			break;
		default:
			DC_LOG_WARNING(
				"%s: Invalid LPT NUM_PIPES!!!",
				__func__);
			break;
		}

		 
		switch (cp110->base.banks_num) {
		case 16:
			set_reg_field_value(
				lpt_control,
				3,
				LOW_POWER_TILING_CONTROL,
				LOW_POWER_TILING_NUM_BANKS);
			break;
		case 8:
			set_reg_field_value(
				lpt_control,
				2,
				LOW_POWER_TILING_CONTROL,
				LOW_POWER_TILING_NUM_BANKS);
			break;
		case 4:
			set_reg_field_value(
				lpt_control,
				1,
				LOW_POWER_TILING_CONTROL,
				LOW_POWER_TILING_NUM_BANKS);
			break;
		case 2:
			set_reg_field_value(
				lpt_control,
				0,
				LOW_POWER_TILING_CONTROL,
				LOW_POWER_TILING_NUM_BANKS);
			break;
		default:
			DC_LOG_WARNING(
				"%s: Invalid LPT NUM_BANKS!!!",
				__func__);
			break;
		}

		 
		switch (cp110->base.channel_interleave_size) {
		case 256:  
			set_reg_field_value(
				lpt_control,
				0,
				LOW_POWER_TILING_CONTROL,
				LOW_POWER_TILING_PIPE_INTERLEAVE_SIZE);
			break;
		case 512:  
			set_reg_field_value(
				lpt_control,
				1,
				LOW_POWER_TILING_CONTROL,
				LOW_POWER_TILING_PIPE_INTERLEAVE_SIZE);
			break;
		default:
			DC_LOG_WARNING(
				"%s: Invalid LPT INTERLEAVE_SIZE!!!",
				__func__);
			break;
		}

		 
		switch (cp110->base.raw_size) {
		case 4096:  
			set_reg_field_value(
				lpt_control,
				2,
				LOW_POWER_TILING_CONTROL,
				LOW_POWER_TILING_ROW_SIZE);
			break;
		case 2048:
			set_reg_field_value(
				lpt_control,
				1,
				LOW_POWER_TILING_CONTROL,
				LOW_POWER_TILING_ROW_SIZE);
			break;
		case 1024:
			set_reg_field_value(
				lpt_control,
				0,
				LOW_POWER_TILING_CONTROL,
				LOW_POWER_TILING_ROW_SIZE);
			break;
		default:
			DC_LOG_WARNING(
				"%s: Invalid LPT ROW_SIZE!!!",
				__func__);
			break;
		}
	} else {
		DC_LOG_WARNING(
			"%s: LPT MC Configuration is not provided",
			__func__);
	}

	return lpt_control;
}

static bool is_source_bigger_than_epanel_size(
	struct dce112_compressor *cp110,
	uint32_t source_view_width,
	uint32_t source_view_height)
{
	if (cp110->base.embedded_panel_h_size != 0 &&
		cp110->base.embedded_panel_v_size != 0 &&
		((source_view_width * source_view_height) >
		(cp110->base.embedded_panel_h_size *
			cp110->base.embedded_panel_v_size)))
		return true;

	return false;
}

static uint32_t align_to_chunks_number_per_line(
	struct dce112_compressor *cp110,
	uint32_t pixels)
{
	return 256 * ((pixels + 255) / 256);
}

static void wait_for_fbc_state_changed(
	struct dce112_compressor *cp110,
	bool enabled)
{
	uint8_t counter = 0;
	uint32_t addr = mmFBC_STATUS;
	uint32_t value;

	while (counter < 10) {
		value = dm_read_reg(cp110->base.ctx, addr);
		if (get_reg_field_value(
			value,
			FBC_STATUS,
			FBC_ENABLE_STATUS) == enabled)
			break;
		udelay(10);
		counter++;
	}

	if (counter == 10) {
		DC_LOG_WARNING(
			"%s: wait counter exceeded, changes to HW not applied",
			__func__);
	}
}

void dce112_compressor_power_up_fbc(struct compressor *compressor)
{
	uint32_t value;
	uint32_t addr;

	addr = mmFBC_CNTL;
	value = dm_read_reg(compressor->ctx, addr);
	set_reg_field_value(value, 0, FBC_CNTL, FBC_GRPH_COMP_EN);
	set_reg_field_value(value, 1, FBC_CNTL, FBC_EN);
	set_reg_field_value(value, 2, FBC_CNTL, FBC_COHERENCY_MODE);
	if (compressor->options.bits.CLK_GATING_DISABLED == 1) {
		 
		set_reg_field_value(
			value,
			0,
			FBC_CNTL,
			FBC_COMP_CLK_GATE_EN);
	}
	dm_write_reg(compressor->ctx, addr, value);

	addr = mmFBC_COMP_MODE;
	value = dm_read_reg(compressor->ctx, addr);
	set_reg_field_value(value, 1, FBC_COMP_MODE, FBC_RLE_EN);
	set_reg_field_value(value, 1, FBC_COMP_MODE, FBC_DPCM4_RGB_EN);
	set_reg_field_value(value, 1, FBC_COMP_MODE, FBC_IND_EN);
	dm_write_reg(compressor->ctx, addr, value);

	addr = mmFBC_COMP_CNTL;
	value = dm_read_reg(compressor->ctx, addr);
	set_reg_field_value(value, 1, FBC_COMP_CNTL, FBC_DEPTH_RGB08_EN);
	dm_write_reg(compressor->ctx, addr, value);
	 
	 
	 
	 
	set_reg_field_value(value, 0xF, FBC_COMP_CNTL, FBC_MIN_COMPRESSION);
	dm_write_reg(compressor->ctx, addr, value);
	compressor->min_compress_ratio = FBC_COMPRESS_RATIO_1TO1;

	value = 0;
	dm_write_reg(compressor->ctx, mmFBC_IND_LUT0, value);

	value = 0xFFFFFF;
	dm_write_reg(compressor->ctx, mmFBC_IND_LUT1, value);
}

void dce112_compressor_enable_fbc(
	struct compressor *compressor,
	uint32_t paths_num,
	struct compr_addr_and_pitch_params *params)
{
	struct dce112_compressor *cp110 = TO_DCE112_COMPRESSOR(compressor);

	if (compressor->options.bits.FBC_SUPPORT &&
		(compressor->options.bits.DUMMY_BACKEND == 0) &&
		(!dce112_compressor_is_fbc_enabled_in_hw(compressor, NULL)) &&
		(!is_source_bigger_than_epanel_size(
			cp110,
			params->source_view_width,
			params->source_view_height))) {

		uint32_t addr;
		uint32_t value;

		 
		if (compressor->options.bits.LPT_SUPPORT && (paths_num < 2) &&
			(params->source_view_width *
				params->source_view_height <=
				dce11_one_lpt_channel_max_resolution)) {
			dce112_compressor_enable_lpt(compressor);
		}

		addr = mmFBC_CNTL;
		value = dm_read_reg(compressor->ctx, addr);
		set_reg_field_value(value, 1, FBC_CNTL, FBC_GRPH_COMP_EN);
		set_reg_field_value(
			value,
			params->inst,
			FBC_CNTL, FBC_SRC_SEL);
		dm_write_reg(compressor->ctx, addr, value);

		 
		compressor->is_enabled = true;
		compressor->attached_inst = params->inst;
		cp110->offsets = reg_offsets[params->inst];

		 
		set_reg_field_value(value, 0, FBC_CNTL, FBC_GRPH_COMP_EN);
		dm_write_reg(compressor->ctx, addr, value);
		set_reg_field_value(value, 1, FBC_CNTL, FBC_GRPH_COMP_EN);
		dm_write_reg(compressor->ctx, addr, value);

		wait_for_fbc_state_changed(cp110, true);
	}
}

void dce112_compressor_disable_fbc(struct compressor *compressor)
{
	struct dce112_compressor *cp110 = TO_DCE112_COMPRESSOR(compressor);

	if (compressor->options.bits.FBC_SUPPORT &&
		dce112_compressor_is_fbc_enabled_in_hw(compressor, NULL)) {
		uint32_t reg_data;
		 
		reg_data = dm_read_reg(compressor->ctx, mmFBC_CNTL);
		set_reg_field_value(reg_data, 0, FBC_CNTL, FBC_GRPH_COMP_EN);
		dm_write_reg(compressor->ctx, mmFBC_CNTL, reg_data);

		 
		compressor->attached_inst = 0;
		compressor->is_enabled = false;

		 
		if (compressor->options.bits.LPT_SUPPORT)
			dce112_compressor_disable_lpt(compressor);

		wait_for_fbc_state_changed(cp110, false);
	}
}

bool dce112_compressor_is_fbc_enabled_in_hw(
	struct compressor *compressor,
	uint32_t *inst)
{
	 
	uint32_t value;

	value = dm_read_reg(compressor->ctx, mmFBC_STATUS);
	if (get_reg_field_value(value, FBC_STATUS, FBC_ENABLE_STATUS)) {
		if (inst != NULL)
			*inst = compressor->attached_inst;
		return true;
	}

	value = dm_read_reg(compressor->ctx, mmFBC_MISC);
	if (get_reg_field_value(value, FBC_MISC, FBC_STOP_ON_HFLIP_EVENT)) {
		value = dm_read_reg(compressor->ctx, mmFBC_CNTL);

		if (get_reg_field_value(value, FBC_CNTL, FBC_GRPH_COMP_EN)) {
			if (inst != NULL)
				*inst =
					compressor->attached_inst;
			return true;
		}
	}
	return false;
}

bool dce112_compressor_is_lpt_enabled_in_hw(struct compressor *compressor)
{
	 
	uint32_t value = dm_read_reg(compressor->ctx,
		mmLOW_POWER_TILING_CONTROL);

	return get_reg_field_value(
		value,
		LOW_POWER_TILING_CONTROL,
		LOW_POWER_TILING_ENABLE);
}

void dce112_compressor_program_compressed_surface_address_and_pitch(
	struct compressor *compressor,
	struct compr_addr_and_pitch_params *params)
{
	struct dce112_compressor *cp110 = TO_DCE112_COMPRESSOR(compressor);
	uint32_t value = 0;
	uint32_t fbc_pitch = 0;
	uint32_t compressed_surf_address_low_part =
		compressor->compr_surface_address.addr.low_part;

	 
	dm_write_reg(
		compressor->ctx,
		DCP_REG(mmGRPH_COMPRESS_SURFACE_ADDRESS_HIGH),
		0);
	dm_write_reg(compressor->ctx,
		DCP_REG(mmGRPH_COMPRESS_SURFACE_ADDRESS), 0);

	if (compressor->options.bits.LPT_SUPPORT) {
		uint32_t lpt_alignment = lpt_size_alignment(cp110);

		if (lpt_alignment != 0) {
			compressed_surf_address_low_part =
				((compressed_surf_address_low_part
					+ (lpt_alignment - 1)) / lpt_alignment)
					* lpt_alignment;
		}
	}

	 
	dm_write_reg(compressor->ctx,
		DCP_REG(mmGRPH_COMPRESS_SURFACE_ADDRESS_HIGH),
		compressor->compr_surface_address.addr.high_part);
	dm_write_reg(compressor->ctx,
		DCP_REG(mmGRPH_COMPRESS_SURFACE_ADDRESS),
		compressed_surf_address_low_part);

	fbc_pitch = align_to_chunks_number_per_line(
		cp110,
		params->source_view_width);

	if (compressor->min_compress_ratio == FBC_COMPRESS_RATIO_1TO1)
		fbc_pitch = fbc_pitch / 8;
	else
		DC_LOG_WARNING(
			"%s: Unexpected DCE11 compression ratio",
			__func__);

	 
	dm_write_reg(compressor->ctx, DCP_REG(mmGRPH_COMPRESS_PITCH), 0);

	 
	set_reg_field_value(
		value,
		fbc_pitch,
		GRPH_COMPRESS_PITCH,
		GRPH_COMPRESS_PITCH);
	dm_write_reg(compressor->ctx, DCP_REG(mmGRPH_COMPRESS_PITCH), value);

}

void dce112_compressor_disable_lpt(struct compressor *compressor)
{
	struct dce112_compressor *cp110 = TO_DCE112_COMPRESSOR(compressor);
	uint32_t value;
	uint32_t addr;
	uint32_t inx;

	 
	for (inx = 0; inx < 3; inx++) {
		value =
			dm_read_reg(
				compressor->ctx,
				DMIF_REG(mmDPG_PIPE_STUTTER_CONTROL_NONLPTCH));
		set_reg_field_value(
			value,
			0,
			DPG_PIPE_STUTTER_CONTROL_NONLPTCH,
			STUTTER_ENABLE_NONLPTCH);
		dm_write_reg(
			compressor->ctx,
			DMIF_REG(mmDPG_PIPE_STUTTER_CONTROL_NONLPTCH),
			value);
	}
	 
	addr = mmDPGV0_PIPE_STUTTER_CONTROL_NONLPTCH;
	value = dm_read_reg(compressor->ctx, addr);
	set_reg_field_value(
		value,
		0,
		DPGV0_PIPE_STUTTER_CONTROL_NONLPTCH,
		STUTTER_ENABLE_NONLPTCH);
	dm_write_reg(compressor->ctx, addr, value);

	 
	addr = mmLOW_POWER_TILING_CONTROL;
	value = dm_read_reg(compressor->ctx, addr);
	set_reg_field_value(
		value,
		0,
		LOW_POWER_TILING_CONTROL,
		LOW_POWER_TILING_ENABLE);
	dm_write_reg(compressor->ctx, addr, value);

	 
	addr = mmGMCON_LPT_TARGET;
	value = dm_read_reg(compressor->ctx, addr);
	set_reg_field_value(
		value,
		0xFFFFFFFF,
		GMCON_LPT_TARGET,
		STCTRL_LPT_TARGET);
	dm_write_reg(compressor->ctx, mmGMCON_LPT_TARGET, value);
}

void dce112_compressor_enable_lpt(struct compressor *compressor)
{
	struct dce112_compressor *cp110 = TO_DCE112_COMPRESSOR(compressor);
	uint32_t value;
	uint32_t addr;
	uint32_t value_control;
	uint32_t channels;

	 
	value = dm_read_reg(compressor->ctx,
		DMIF_REG(mmDPG_PIPE_STUTTER_CONTROL_NONLPTCH));
	set_reg_field_value(
		value,
		1,
		DPG_PIPE_STUTTER_CONTROL_NONLPTCH,
		STUTTER_ENABLE_NONLPTCH);
	dm_write_reg(compressor->ctx,
		DMIF_REG(mmDPG_PIPE_STUTTER_CONTROL_NONLPTCH), value);

	 
	addr = mmDPGV0_PIPE_STUTTER_CONTROL_NONLPTCH;
	value = dm_read_reg(compressor->ctx, addr);
	set_reg_field_value(
		value,
		1,
		DPGV0_PIPE_STUTTER_CONTROL_NONLPTCH,
		STUTTER_ENABLE_NONLPTCH);
	dm_write_reg(compressor->ctx, addr, value);

	 
	addr = mmLOW_POWER_TILING_CONTROL;
	value_control = dm_read_reg(compressor->ctx, addr);
	channels = get_reg_field_value(value_control,
			LOW_POWER_TILING_CONTROL,
			LOW_POWER_TILING_MODE);

	addr = mmGMCON_LPT_TARGET;
	value = dm_read_reg(compressor->ctx, addr);
	set_reg_field_value(
		value,
		channels + 1,  
		GMCON_LPT_TARGET,
		STCTRL_LPT_TARGET);
	dm_write_reg(compressor->ctx, addr, value);

	 
	addr = mmLOW_POWER_TILING_CONTROL;
	value = dm_read_reg(compressor->ctx, addr);
	set_reg_field_value(
		value,
		1,
		LOW_POWER_TILING_CONTROL,
		LOW_POWER_TILING_ENABLE);
	dm_write_reg(compressor->ctx, addr, value);
}

void dce112_compressor_program_lpt_control(
	struct compressor *compressor,
	struct compr_addr_and_pitch_params *params)
{
	struct dce112_compressor *cp110 = TO_DCE112_COMPRESSOR(compressor);
	uint32_t rows_per_channel;
	uint32_t lpt_alignment;
	uint32_t source_view_width;
	uint32_t source_view_height;
	uint32_t lpt_control = 0;

	if (!compressor->options.bits.LPT_SUPPORT)
		return;

	lpt_control = dm_read_reg(compressor->ctx,
		mmLOW_POWER_TILING_CONTROL);

	 
	switch (compressor->lpt_channels_num) {
	 
	case 1:
		 
		set_reg_field_value(
			lpt_control,
			0,
			LOW_POWER_TILING_CONTROL,
			LOW_POWER_TILING_MODE);
		break;
	default:
		DC_LOG_WARNING(
			"%s: Invalid selected DRAM channels for LPT!!!",
			__func__);
		break;
	}

	lpt_control = lpt_memory_control_config(cp110, lpt_control);

	 
	rows_per_channel = 0;
	lpt_alignment = lpt_size_alignment(cp110);
	source_view_width =
		align_to_chunks_number_per_line(
			cp110,
			params->source_view_width);
	source_view_height = (params->source_view_height + 1) & (~0x1);

	if (lpt_alignment != 0) {
		rows_per_channel = source_view_width * source_view_height * 4;
		rows_per_channel =
			(rows_per_channel % lpt_alignment) ?
				(rows_per_channel / lpt_alignment + 1) :
				rows_per_channel / lpt_alignment;
	}

	set_reg_field_value(
		lpt_control,
		rows_per_channel,
		LOW_POWER_TILING_CONTROL,
		LOW_POWER_TILING_ROWS_PER_CHAN);

	dm_write_reg(compressor->ctx,
		mmLOW_POWER_TILING_CONTROL, lpt_control);
}

 

void dce112_compressor_set_fbc_invalidation_triggers(
	struct compressor *compressor,
	uint32_t fbc_trigger)
{
	 
	uint32_t addr = mmFBC_CLIENT_REGION_MASK;
	uint32_t value = dm_read_reg(compressor->ctx, addr);

	set_reg_field_value(
		value,
		0,
		FBC_CLIENT_REGION_MASK,
		FBC_MEMORY_REGION_MASK);
	dm_write_reg(compressor->ctx, addr, value);

	 
	addr = mmFBC_IDLE_FORCE_CLEAR_MASK;
	value = dm_read_reg(compressor->ctx, addr);
	set_reg_field_value(
		value,
		fbc_trigger |
		FBC_IDLE_FORCE_GRPH_COMP_EN |
		FBC_IDLE_FORCE_SRC_SEL_CHANGE |
		FBC_IDLE_FORCE_MIN_COMPRESSION_CHANGE |
		FBC_IDLE_FORCE_ALPHA_COMP_EN |
		FBC_IDLE_FORCE_ZERO_ALPHA_CHUNK_SKIP_EN |
		FBC_IDLE_FORCE_FORCE_COPY_TO_COMP_BUF,
		FBC_IDLE_FORCE_CLEAR_MASK,
		FBC_IDLE_FORCE_CLEAR_MASK);
	dm_write_reg(compressor->ctx, addr, value);
}

void dce112_compressor_construct(struct dce112_compressor *compressor,
	struct dc_context *ctx)
{
	struct dc_bios *bp = ctx->dc_bios;
	struct embedded_panel_info panel_info;

	compressor->base.options.raw = 0;
	compressor->base.options.bits.FBC_SUPPORT = true;
	compressor->base.options.bits.LPT_SUPPORT = true;
	  
	compressor->base.lpt_channels_num = 1;
	compressor->base.options.bits.DUMMY_BACKEND = false;

	 
	if (compressor->base.memory_bus_width == 64)
		compressor->base.options.bits.LPT_SUPPORT = false;

	compressor->base.options.bits.CLK_GATING_DISABLED = false;

	compressor->base.ctx = ctx;
	compressor->base.embedded_panel_h_size = 0;
	compressor->base.embedded_panel_v_size = 0;
	compressor->base.memory_bus_width = ctx->asic_id.vram_width;
	compressor->base.allocated_size = 0;
	compressor->base.preferred_requested_size = 0;
	compressor->base.min_compress_ratio = FBC_COMPRESS_RATIO_INVALID;
	compressor->base.banks_num = 0;
	compressor->base.raw_size = 0;
	compressor->base.channel_interleave_size = 0;
	compressor->base.dram_channels_num = 0;
	compressor->base.lpt_channels_num = 0;
	compressor->base.attached_inst = 0;
	compressor->base.is_enabled = false;

	if (BP_RESULT_OK ==
			bp->funcs->get_embedded_panel_info(bp, &panel_info)) {
		compressor->base.embedded_panel_h_size =
			panel_info.lcd_timing.horizontal_addressable;
		compressor->base.embedded_panel_v_size =
			panel_info.lcd_timing.vertical_addressable;
	}
}

struct compressor *dce112_compressor_create(struct dc_context *ctx)
{
	struct dce112_compressor *cp110 =
		kzalloc(sizeof(struct dce112_compressor), GFP_KERNEL);

	if (!cp110)
		return NULL;

	dce112_compressor_construct(cp110, ctx);
	return &cp110->base;
}

void dce112_compressor_destroy(struct compressor **compressor)
{
	kfree(TO_DCE112_COMPRESSOR(*compressor));
	*compressor = NULL;
}
