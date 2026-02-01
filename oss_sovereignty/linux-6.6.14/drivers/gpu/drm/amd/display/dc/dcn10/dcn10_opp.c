 

#include "dm_services.h"
#include "dcn10_opp.h"
#include "reg_helper.h"

#define REG(reg) \
	(oppn10->regs->reg)

#undef FN
#define FN(reg_name, field_name) \
	oppn10->opp_shift->field_name, oppn10->opp_mask->field_name

#define CTX \
	oppn10->base.ctx

 
static void opp1_set_truncation(
		struct dcn10_opp *oppn10,
		const struct bit_depth_reduction_params *params)
{
	REG_UPDATE_3(FMT_BIT_DEPTH_CONTROL,
		FMT_TRUNCATE_EN, params->flags.TRUNCATE_ENABLED,
		FMT_TRUNCATE_DEPTH, params->flags.TRUNCATE_DEPTH,
		FMT_TRUNCATE_MODE, params->flags.TRUNCATE_MODE);
}

static void opp1_set_spatial_dither(
	struct dcn10_opp *oppn10,
	const struct bit_depth_reduction_params *params)
{
	 
	REG_UPDATE_7(FMT_BIT_DEPTH_CONTROL,
			FMT_SPATIAL_DITHER_EN, 0,
			FMT_SPATIAL_DITHER_MODE, 0,
			FMT_SPATIAL_DITHER_DEPTH, 0,
			FMT_TEMPORAL_DITHER_EN, 0,
			FMT_HIGHPASS_RANDOM_ENABLE, 0,
			FMT_FRAME_RANDOM_ENABLE, 0,
			FMT_RGB_RANDOM_ENABLE, 0);


	 
	if (params->flags.FRAME_RANDOM == 1) {
		if (params->flags.SPATIAL_DITHER_DEPTH == 0 || params->flags.SPATIAL_DITHER_DEPTH == 1) {
			REG_UPDATE_2(FMT_CONTROL,
					FMT_SPATIAL_DITHER_FRAME_COUNTER_MAX, 15,
					FMT_SPATIAL_DITHER_FRAME_COUNTER_BIT_SWAP, 2);
		} else if (params->flags.SPATIAL_DITHER_DEPTH == 2) {
			REG_UPDATE_2(FMT_CONTROL,
					FMT_SPATIAL_DITHER_FRAME_COUNTER_MAX, 3,
					FMT_SPATIAL_DITHER_FRAME_COUNTER_BIT_SWAP, 1);
		} else {
			return;
		}
	} else {
		REG_UPDATE_2(FMT_CONTROL,
				FMT_SPATIAL_DITHER_FRAME_COUNTER_MAX, 0,
				FMT_SPATIAL_DITHER_FRAME_COUNTER_BIT_SWAP, 0);
	}

	 

	REG_SET(FMT_DITHER_RAND_R_SEED, 0,
			FMT_RAND_R_SEED, params->r_seed_value);

	REG_SET(FMT_DITHER_RAND_G_SEED, 0,
			FMT_RAND_G_SEED, params->g_seed_value);

	REG_SET(FMT_DITHER_RAND_B_SEED, 0,
			FMT_RAND_B_SEED, params->b_seed_value);

	 
	 
	 

	REG_UPDATE_6(FMT_BIT_DEPTH_CONTROL,
			 
			FMT_SPATIAL_DITHER_EN, params->flags.SPATIAL_DITHER_ENABLED,
			 
			FMT_SPATIAL_DITHER_MODE, params->flags.SPATIAL_DITHER_MODE,
			 
			FMT_SPATIAL_DITHER_DEPTH, params->flags.SPATIAL_DITHER_DEPTH,
			 
			FMT_HIGHPASS_RANDOM_ENABLE, params->flags.HIGHPASS_RANDOM,
			 
			FMT_FRAME_RANDOM_ENABLE, params->flags.FRAME_RANDOM,
			 
			FMT_RGB_RANDOM_ENABLE, params->flags.RGB_RANDOM);
}

void opp1_program_bit_depth_reduction(
	struct output_pixel_processor *opp,
	const struct bit_depth_reduction_params *params)
{
	struct dcn10_opp *oppn10 = TO_DCN10_OPP(opp);

	opp1_set_truncation(oppn10, params);
	opp1_set_spatial_dither(oppn10, params);
	 
}

 
static void opp1_set_pixel_encoding(
	struct dcn10_opp *oppn10,
	const struct clamping_and_pixel_encoding_params *params)
{
	switch (params->pixel_encoding)	{

	case PIXEL_ENCODING_RGB:
	case PIXEL_ENCODING_YCBCR444:
		REG_UPDATE(FMT_CONTROL, FMT_PIXEL_ENCODING, 0);
		break;
	case PIXEL_ENCODING_YCBCR422:
		REG_UPDATE_3(FMT_CONTROL,
				FMT_PIXEL_ENCODING, 1,
				FMT_SUBSAMPLING_MODE, 2,
				FMT_CBCR_BIT_REDUCTION_BYPASS, 0);
		break;
	case PIXEL_ENCODING_YCBCR420:
		REG_UPDATE(FMT_CONTROL, FMT_PIXEL_ENCODING, 2);
		break;
	default:
		break;
	}
}

 
static void opp1_set_clamping(
	struct dcn10_opp *oppn10,
	const struct clamping_and_pixel_encoding_params *params)
{
	REG_UPDATE_2(FMT_CLAMP_CNTL,
			FMT_CLAMP_DATA_EN, 0,
			FMT_CLAMP_COLOR_FORMAT, 0);

	switch (params->clamping_level) {
	case CLAMPING_FULL_RANGE:
		REG_UPDATE_2(FMT_CLAMP_CNTL,
				FMT_CLAMP_DATA_EN, 1,
				FMT_CLAMP_COLOR_FORMAT, 0);
		break;
	case CLAMPING_LIMITED_RANGE_8BPC:
		REG_UPDATE_2(FMT_CLAMP_CNTL,
				FMT_CLAMP_DATA_EN, 1,
				FMT_CLAMP_COLOR_FORMAT, 1);
		break;
	case CLAMPING_LIMITED_RANGE_10BPC:
		REG_UPDATE_2(FMT_CLAMP_CNTL,
				FMT_CLAMP_DATA_EN, 1,
				FMT_CLAMP_COLOR_FORMAT, 2);

		break;
	case CLAMPING_LIMITED_RANGE_12BPC:
		REG_UPDATE_2(FMT_CLAMP_CNTL,
				FMT_CLAMP_DATA_EN, 1,
				FMT_CLAMP_COLOR_FORMAT, 3);
		break;
	case CLAMPING_LIMITED_RANGE_PROGRAMMABLE:
		 
	default:
		break;
	}

}

void opp1_set_dyn_expansion(
	struct output_pixel_processor *opp,
	enum dc_color_space color_sp,
	enum dc_color_depth color_dpth,
	enum signal_type signal)
{
	struct dcn10_opp *oppn10 = TO_DCN10_OPP(opp);

	REG_UPDATE_2(FMT_DYNAMIC_EXP_CNTL,
			FMT_DYNAMIC_EXP_EN, 0,
			FMT_DYNAMIC_EXP_MODE, 0);

	if (opp->dyn_expansion == DYN_EXPANSION_DISABLE)
		return;

	 
	 
	if (signal == SIGNAL_TYPE_HDMI_TYPE_A ||
		signal == SIGNAL_TYPE_DISPLAY_PORT ||
		signal == SIGNAL_TYPE_DISPLAY_PORT_MST ||
		signal == SIGNAL_TYPE_VIRTUAL) {
		switch (color_dpth) {
		case COLOR_DEPTH_888:
			REG_UPDATE_2(FMT_DYNAMIC_EXP_CNTL,
				FMT_DYNAMIC_EXP_EN, 1,
				FMT_DYNAMIC_EXP_MODE, 1);
			break;
		case COLOR_DEPTH_101010:
			REG_UPDATE_2(FMT_DYNAMIC_EXP_CNTL,
				FMT_DYNAMIC_EXP_EN, 1,
				FMT_DYNAMIC_EXP_MODE, 0);
			break;
		case COLOR_DEPTH_121212:
			REG_UPDATE_2(FMT_DYNAMIC_EXP_CNTL,
				FMT_DYNAMIC_EXP_EN, 1, 
				FMT_DYNAMIC_EXP_MODE, 0);
			break;
		default:
			break;
		}
	}
}

static void opp1_program_clamping_and_pixel_encoding(
	struct output_pixel_processor *opp,
	const struct clamping_and_pixel_encoding_params *params)
{
	struct dcn10_opp *oppn10 = TO_DCN10_OPP(opp);

	opp1_set_clamping(oppn10, params);
	opp1_set_pixel_encoding(oppn10, params);
}

void opp1_program_fmt(
	struct output_pixel_processor *opp,
	struct bit_depth_reduction_params *fmt_bit_depth,
	struct clamping_and_pixel_encoding_params *clamping)
{
	struct dcn10_opp *oppn10 = TO_DCN10_OPP(opp);

	if (clamping->pixel_encoding == PIXEL_ENCODING_YCBCR420)
		REG_UPDATE(FMT_MAP420_MEMORY_CONTROL, FMT_MAP420MEM_PWR_FORCE, 0);

	 
	opp1_program_bit_depth_reduction(
		opp,
		fmt_bit_depth);

	opp1_program_clamping_and_pixel_encoding(
		opp,
		clamping);

	return;
}

void opp1_program_stereo(
	struct output_pixel_processor *opp,
	bool enable,
	const struct dc_crtc_timing *timing)
{
	struct dcn10_opp *oppn10 = TO_DCN10_OPP(opp);

	uint32_t active_width = timing->h_addressable - timing->h_border_right - timing->h_border_right;
	uint32_t space1_size = timing->v_total - timing->v_addressable;
	 
	uint32_t space2_size = timing->v_total - timing->v_addressable;

	if (!enable) {
		active_width = 0;
		space1_size = 0;
		space2_size = 0;
	}

	 
	REG_UPDATE(FMT_CONTROL, FMT_STEREOSYNC_OVERRIDE, 0);

	REG_UPDATE(OPPBUF_CONTROL, OPPBUF_ACTIVE_WIDTH, active_width);

	 
	if (timing->timing_3d_format == TIMING_3D_FORMAT_FRAME_ALTERNATE)
		REG_UPDATE(OPPBUF_3D_PARAMETERS_0, OPPBUF_3D_VACT_SPACE2_SIZE, space2_size);
	else
		REG_UPDATE(OPPBUF_3D_PARAMETERS_0, OPPBUF_3D_VACT_SPACE1_SIZE, space1_size);

	 
	 
}

void opp1_pipe_clock_control(struct output_pixel_processor *opp, bool enable)
{
	struct dcn10_opp *oppn10 = TO_DCN10_OPP(opp);
	uint32_t regval = enable ? 1 : 0;

	REG_UPDATE(OPP_PIPE_CONTROL, OPP_PIPE_CLOCK_EN, regval);
}

 
 
 

void opp1_destroy(struct output_pixel_processor **opp)
{
	kfree(TO_DCN10_OPP(*opp));
	*opp = NULL;
}

static const struct opp_funcs dcn10_opp_funcs = {
		.opp_set_dyn_expansion = opp1_set_dyn_expansion,
		.opp_program_fmt = opp1_program_fmt,
		.opp_program_bit_depth_reduction = opp1_program_bit_depth_reduction,
		.opp_program_stereo = opp1_program_stereo,
		.opp_pipe_clock_control = opp1_pipe_clock_control,
		.opp_set_disp_pattern_generator = NULL,
		.opp_program_dpg_dimensions = NULL,
		.dpg_is_blanked = NULL,
		.opp_destroy = opp1_destroy
};

void dcn10_opp_construct(struct dcn10_opp *oppn10,
	struct dc_context *ctx,
	uint32_t inst,
	const struct dcn10_opp_registers *regs,
	const struct dcn10_opp_shift *opp_shift,
	const struct dcn10_opp_mask *opp_mask)
{

	oppn10->base.ctx = ctx;
	oppn10->base.inst = inst;
	oppn10->base.funcs = &dcn10_opp_funcs;

	oppn10->regs = regs;
	oppn10->opp_shift = opp_shift;
	oppn10->opp_mask = opp_mask;
}
