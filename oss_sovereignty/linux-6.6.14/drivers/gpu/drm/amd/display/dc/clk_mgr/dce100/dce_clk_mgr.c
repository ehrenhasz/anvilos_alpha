 


#include "dccg.h"
#include "clk_mgr_internal.h"
#include "dce_clk_mgr.h"
#include "dce110/dce110_clk_mgr.h"
#include "dce112/dce112_clk_mgr.h"
#include "reg_helper.h"
#include "dmcu.h"
#include "core_types.h"
#include "dal_asic_id.h"

 
#include "dce/dce_8_0_d.h"
#include "dce/dce_8_0_sh_mask.h"

#define REG(reg) \
	(clk_mgr->regs->reg)

#undef FN
#define FN(reg_name, field_name) \
	clk_mgr->clk_mgr_shift->field_name, clk_mgr->clk_mgr_mask->field_name

static const struct clk_mgr_registers disp_clk_regs = {
		CLK_COMMON_REG_LIST_DCE_BASE()
};

static const struct clk_mgr_shift disp_clk_shift = {
		CLK_COMMON_MASK_SH_LIST_DCE_COMMON_BASE(__SHIFT)
};

static const struct clk_mgr_mask disp_clk_mask = {
		CLK_COMMON_MASK_SH_LIST_DCE_COMMON_BASE(_MASK)
};


 
static const struct state_dependent_clocks dce80_max_clks_by_state[] = {
 
{ .display_clk_khz = 0, .pixel_clk_khz = 0 },
 
{ .display_clk_khz = 0, .pixel_clk_khz = 0 },
 
{ .display_clk_khz = 352000, .pixel_clk_khz = 330000},
 
{ .display_clk_khz = 600000, .pixel_clk_khz = 400000 },
 
{ .display_clk_khz = 600000, .pixel_clk_khz = 400000 } };

int dentist_get_divider_from_did(int did)
{
	if (did < DENTIST_BASE_DID_1)
		did = DENTIST_BASE_DID_1;
	if (did > DENTIST_MAX_DID)
		did = DENTIST_MAX_DID;

	if (did < DENTIST_BASE_DID_2) {
		return DENTIST_DIVIDER_RANGE_1_START + DENTIST_DIVIDER_RANGE_1_STEP
							* (did - DENTIST_BASE_DID_1);
	} else if (did < DENTIST_BASE_DID_3) {
		return DENTIST_DIVIDER_RANGE_2_START + DENTIST_DIVIDER_RANGE_2_STEP
							* (did - DENTIST_BASE_DID_2);
	} else if (did < DENTIST_BASE_DID_4) {
		return DENTIST_DIVIDER_RANGE_3_START + DENTIST_DIVIDER_RANGE_3_STEP
							* (did - DENTIST_BASE_DID_3);
	} else {
		return DENTIST_DIVIDER_RANGE_4_START + DENTIST_DIVIDER_RANGE_4_STEP
							* (did - DENTIST_BASE_DID_4);
	}
}

 

int dce_adjust_dp_ref_freq_for_ss(struct clk_mgr_internal *clk_mgr_dce, int dp_ref_clk_khz)
{
	if (clk_mgr_dce->ss_on_dprefclk && clk_mgr_dce->dprefclk_ss_divider != 0) {
		struct fixed31_32 ss_percentage = dc_fixpt_div_int(
				dc_fixpt_from_fraction(clk_mgr_dce->dprefclk_ss_percentage,
							clk_mgr_dce->dprefclk_ss_divider), 200);
		struct fixed31_32 adj_dp_ref_clk_khz;

		ss_percentage = dc_fixpt_sub(dc_fixpt_one, ss_percentage);
		adj_dp_ref_clk_khz = dc_fixpt_mul_int(ss_percentage, dp_ref_clk_khz);
		dp_ref_clk_khz = dc_fixpt_floor(adj_dp_ref_clk_khz);
	}
	return dp_ref_clk_khz;
}

int dce_get_dp_ref_freq_khz(struct clk_mgr *clk_mgr_base)
{
	struct clk_mgr_internal *clk_mgr = TO_CLK_MGR_INTERNAL(clk_mgr_base);
	int dprefclk_wdivider;
	int dprefclk_src_sel;
	int dp_ref_clk_khz;
	int target_div;

	 
	REG_GET(DPREFCLK_CNTL, DPREFCLK_SRC_SEL, &dprefclk_src_sel);
	ASSERT(dprefclk_src_sel == 0);

	 
	REG_GET(DENTIST_DISPCLK_CNTL, DENTIST_DPREFCLK_WDIVIDER, &dprefclk_wdivider);

	 
	target_div = dentist_get_divider_from_did(dprefclk_wdivider);

	 
	dp_ref_clk_khz = (DENTIST_DIVIDER_RANGE_SCALE_FACTOR
		* clk_mgr->base.dentist_vco_freq_khz) / target_div;

	return dce_adjust_dp_ref_freq_for_ss(clk_mgr, dp_ref_clk_khz);
}

int dce12_get_dp_ref_freq_khz(struct clk_mgr *clk_mgr_base)
{
	struct clk_mgr_internal *clk_mgr_dce = TO_CLK_MGR_INTERNAL(clk_mgr_base);

	return dce_adjust_dp_ref_freq_for_ss(clk_mgr_dce, clk_mgr_base->dprefclk_khz);
}

 
uint32_t dce_get_max_pixel_clock_for_all_paths(struct dc_state *context)
{
	uint32_t max_pix_clk = 0;
	int i;

	for (i = 0; i < MAX_PIPES; i++) {
		struct pipe_ctx *pipe_ctx = &context->res_ctx.pipe_ctx[i];

		if (pipe_ctx->stream == NULL)
			continue;

		 
		if (pipe_ctx->top_pipe)
			continue;

		if (pipe_ctx->stream_res.pix_clk_params.requested_pix_clk_100hz / 10 > max_pix_clk)
			max_pix_clk = pipe_ctx->stream_res.pix_clk_params.requested_pix_clk_100hz / 10;

		 
		if (dc_is_dp_signal(pipe_ctx->stream->signal) &&
				pipe_ctx->stream_res.pix_clk_params.requested_sym_clk > max_pix_clk)
			max_pix_clk = pipe_ctx->stream_res.pix_clk_params.requested_sym_clk;
	}

	return max_pix_clk;
}

enum dm_pp_clocks_state dce_get_required_clocks_state(
	struct clk_mgr *clk_mgr_base,
	struct dc_state *context)
{
	struct clk_mgr_internal *clk_mgr_dce = TO_CLK_MGR_INTERNAL(clk_mgr_base);
	int i;
	enum dm_pp_clocks_state low_req_clk;
	int max_pix_clk = dce_get_max_pixel_clock_for_all_paths(context);

	 
	for (i = clk_mgr_dce->max_clks_state; i >= DM_PP_CLOCKS_STATE_ULTRA_LOW; i--)
		if (context->bw_ctx.bw.dce.dispclk_khz >
				clk_mgr_dce->max_clks_by_state[i].display_clk_khz
			|| max_pix_clk >
				clk_mgr_dce->max_clks_by_state[i].pixel_clk_khz)
			break;

	low_req_clk = i + 1;
	if (low_req_clk > clk_mgr_dce->max_clks_state) {
		 
		if (clk_mgr_dce->max_clks_by_state[clk_mgr_dce->max_clks_state].display_clk_khz
				< context->bw_ctx.bw.dce.dispclk_khz)
			low_req_clk = DM_PP_CLOCKS_STATE_INVALID;
		else
			low_req_clk = clk_mgr_dce->max_clks_state;
	}

	return low_req_clk;
}


 
int dce_set_clock(
	struct clk_mgr *clk_mgr_base,
	int requested_clk_khz)
{
	struct clk_mgr_internal *clk_mgr_dce = TO_CLK_MGR_INTERNAL(clk_mgr_base);
	struct bp_pixel_clock_parameters pxl_clk_params = { 0 };
	struct dc_bios *bp = clk_mgr_base->ctx->dc_bios;
	int actual_clock = requested_clk_khz;
	struct dmcu *dmcu = clk_mgr_dce->base.ctx->dc->res_pool->dmcu;

	 
	if (requested_clk_khz > 0)
		requested_clk_khz = max(requested_clk_khz,
				clk_mgr_dce->base.dentist_vco_freq_khz / 64);

	 
	pxl_clk_params.target_pixel_clock_100hz = requested_clk_khz * 10;
	pxl_clk_params.pll_id = CLOCK_SOURCE_ID_DFS;

	if (clk_mgr_dce->dfs_bypass_active)
		pxl_clk_params.flags.SET_DISPCLK_DFS_BYPASS = true;

	bp->funcs->program_display_engine_pll(bp, &pxl_clk_params);

	if (clk_mgr_dce->dfs_bypass_active) {
		 
		clk_mgr_dce->dfs_bypass_disp_clk =
			pxl_clk_params.dfs_bypass_display_clock;
		actual_clock = pxl_clk_params.dfs_bypass_display_clock;
	}

	 
	if (requested_clk_khz == 0)
		clk_mgr_dce->cur_min_clks_state = DM_PP_CLOCKS_STATE_NOMINAL;

	if (dmcu && dmcu->funcs->is_dmcu_initialized(dmcu))
		dmcu->funcs->set_psr_wait_loop(dmcu, actual_clock / 1000 / 7);

	return actual_clock;
}


static void dce_clock_read_integrated_info(struct clk_mgr_internal *clk_mgr_dce)
{
	struct dc_debug_options *debug = &clk_mgr_dce->base.ctx->dc->debug;
	struct dc_bios *bp = clk_mgr_dce->base.ctx->dc_bios;
	int i;

	if (bp->integrated_info)
		clk_mgr_dce->base.dentist_vco_freq_khz = bp->integrated_info->dentist_vco_freq;
	if (clk_mgr_dce->base.dentist_vco_freq_khz == 0) {
		clk_mgr_dce->base.dentist_vco_freq_khz = bp->fw_info.smu_gpu_pll_output_freq;
		if (clk_mgr_dce->base.dentist_vco_freq_khz == 0)
			clk_mgr_dce->base.dentist_vco_freq_khz = 3600000;
	}

	 
	for (i = 0; i < NUMBER_OF_DISP_CLK_VOLTAGE; ++i) {
		enum dm_pp_clocks_state clk_state = DM_PP_CLOCKS_STATE_INVALID;

		switch (i) {
		case 0:
			clk_state = DM_PP_CLOCKS_STATE_ULTRA_LOW;
			break;

		case 1:
			clk_state = DM_PP_CLOCKS_STATE_LOW;
			break;

		case 2:
			clk_state = DM_PP_CLOCKS_STATE_NOMINAL;
			break;

		case 3:
			clk_state = DM_PP_CLOCKS_STATE_PERFORMANCE;
			break;

		default:
			clk_state = DM_PP_CLOCKS_STATE_INVALID;
			break;
		}

		 
		if (bp->integrated_info)
			if (bp->integrated_info->disp_clk_voltage[i].max_supported_clk >= 100000)
				clk_mgr_dce->max_clks_by_state[clk_state].display_clk_khz =
					bp->integrated_info->disp_clk_voltage[i].max_supported_clk;
	}

	if (!debug->disable_dfs_bypass && bp->integrated_info)
		if (bp->integrated_info->gpu_cap_info & DFS_BYPASS_ENABLE)
			clk_mgr_dce->dfs_bypass_enabled = true;
}

void dce_clock_read_ss_info(struct clk_mgr_internal *clk_mgr_dce)
{
	struct dc_bios *bp = clk_mgr_dce->base.ctx->dc_bios;
	int ss_info_num = bp->funcs->get_ss_entry_number(
			bp, AS_SIGNAL_TYPE_GPU_PLL);

	if (ss_info_num) {
		struct spread_spectrum_info info = { { 0 } };
		enum bp_result result = bp->funcs->get_spread_spectrum_info(
				bp, AS_SIGNAL_TYPE_GPU_PLL, 0, &info);

		 
		if (result == BP_RESULT_OK &&
				info.spread_spectrum_percentage != 0) {
			clk_mgr_dce->ss_on_dprefclk = true;
			clk_mgr_dce->dprefclk_ss_divider = info.spread_percentage_divider;

			if (info.type.CENTER_MODE == 0) {
				 
				clk_mgr_dce->dprefclk_ss_percentage =
						info.spread_spectrum_percentage;
			}

			return;
		}

		result = bp->funcs->get_spread_spectrum_info(
				bp, AS_SIGNAL_TYPE_DISPLAY_PORT, 0, &info);

		 
		if (result == BP_RESULT_OK &&
				info.spread_spectrum_percentage != 0) {
			clk_mgr_dce->ss_on_dprefclk = true;
			clk_mgr_dce->dprefclk_ss_divider = info.spread_percentage_divider;

			if (info.type.CENTER_MODE == 0) {
				 
				clk_mgr_dce->dprefclk_ss_percentage =
						info.spread_spectrum_percentage;
			}
			if (clk_mgr_dce->base.ctx->dc->config.ignore_dpref_ss)
				clk_mgr_dce->dprefclk_ss_percentage = 0;
		}
	}
}

static void dce_pplib_apply_display_requirements(
	struct dc *dc,
	struct dc_state *context)
{
	struct dm_pp_display_configuration *pp_display_cfg = &context->pp_display_cfg;

	pp_display_cfg->avail_mclk_switch_time_us = dce110_get_min_vblank_time_us(context);

	dce110_fill_display_configs(context, pp_display_cfg);

	if (memcmp(&dc->current_state->pp_display_cfg, pp_display_cfg, sizeof(*pp_display_cfg)) !=  0)
		dm_pp_apply_display_requirements(dc->ctx, pp_display_cfg);
}

static void dce_update_clocks(struct clk_mgr *clk_mgr_base,
			struct dc_state *context,
			bool safe_to_lower)
{
	struct clk_mgr_internal *clk_mgr_dce = TO_CLK_MGR_INTERNAL(clk_mgr_base);
	struct dm_pp_power_level_change_request level_change_req;
	int patched_disp_clk = context->bw_ctx.bw.dce.dispclk_khz;

	 
	if (!clk_mgr_dce->dfs_bypass_active)
		patched_disp_clk = patched_disp_clk * 115 / 100;

	level_change_req.power_level = dce_get_required_clocks_state(clk_mgr_base, context);
	 
	if ((level_change_req.power_level < clk_mgr_dce->cur_min_clks_state && safe_to_lower)
			|| level_change_req.power_level > clk_mgr_dce->cur_min_clks_state) {
		if (dm_pp_apply_power_level_change_request(clk_mgr_base->ctx, &level_change_req))
			clk_mgr_dce->cur_min_clks_state = level_change_req.power_level;
	}

	if (should_set_clock(safe_to_lower, patched_disp_clk, clk_mgr_base->clks.dispclk_khz)) {
		patched_disp_clk = dce_set_clock(clk_mgr_base, patched_disp_clk);
		clk_mgr_base->clks.dispclk_khz = patched_disp_clk;
	}
	dce_pplib_apply_display_requirements(clk_mgr_base->ctx->dc, context);
}








static struct clk_mgr_funcs dce_funcs = {
	.get_dp_ref_clk_frequency = dce_get_dp_ref_freq_khz,
	.update_clocks = dce_update_clocks
};

void dce_clk_mgr_construct(
		struct dc_context *ctx,
		struct clk_mgr_internal *clk_mgr)
{
	struct clk_mgr *base = &clk_mgr->base;
	struct dm_pp_static_clock_info static_clk_info = {0};

	memcpy(clk_mgr->max_clks_by_state,
		dce80_max_clks_by_state,
		sizeof(dce80_max_clks_by_state));

	base->ctx = ctx;
	base->funcs = &dce_funcs;

	clk_mgr->regs = &disp_clk_regs;
	clk_mgr->clk_mgr_shift = &disp_clk_shift;
	clk_mgr->clk_mgr_mask = &disp_clk_mask;
	clk_mgr->dfs_bypass_disp_clk = 0;

	clk_mgr->dprefclk_ss_percentage = 0;
	clk_mgr->dprefclk_ss_divider = 1000;
	clk_mgr->ss_on_dprefclk = false;

	if (dm_pp_get_static_clocks(ctx, &static_clk_info))
		clk_mgr->max_clks_state = static_clk_info.max_clocks_state;
	else
		clk_mgr->max_clks_state = DM_PP_CLOCKS_STATE_NOMINAL;
	clk_mgr->cur_min_clks_state = DM_PP_CLOCKS_STATE_INVALID;

	dce_clock_read_integrated_info(clk_mgr);
	dce_clock_read_ss_info(clk_mgr);
}

