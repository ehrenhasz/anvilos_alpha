 

#include "core_types.h"
#include "clk_mgr_internal.h"

#include "dce112/dce112_clk_mgr.h"
#include "dce110/dce110_clk_mgr.h"
#include "dce120_clk_mgr.h"
#include "dce100/dce_clk_mgr.h"
#include "dce120/dce120_hw_sequencer.h"

static const struct state_dependent_clocks dce120_max_clks_by_state[] = {
 
{ .display_clk_khz = 0, .pixel_clk_khz = 0 },
 
{ .display_clk_khz = 0, .pixel_clk_khz = 0 },
 
{ .display_clk_khz = 460000, .pixel_clk_khz = 400000 },
 
{ .display_clk_khz = 670000, .pixel_clk_khz = 600000 },
 
{ .display_clk_khz = 1133000, .pixel_clk_khz = 600000 } };

 
static void dce121_clock_patch_xgmi_ss_info(struct clk_mgr_internal *clk_mgr_dce)
{
	enum bp_result result;
	struct spread_spectrum_info info = { { 0 } };
	struct dc_bios *bp = clk_mgr_dce->base.ctx->dc_bios;

	clk_mgr_dce->xgmi_enabled = false;

	result = bp->funcs->get_spread_spectrum_info(bp, AS_SIGNAL_TYPE_XGMI,
						     0, &info);
	if (result == BP_RESULT_OK && info.spread_spectrum_percentage != 0) {
		clk_mgr_dce->xgmi_enabled = true;
		clk_mgr_dce->ss_on_dprefclk = true;
		clk_mgr_dce->dprefclk_ss_divider =
				info.spread_percentage_divider;

		if (info.type.CENTER_MODE == 0) {
			 
			clk_mgr_dce->dprefclk_ss_percentage =
					info.spread_spectrum_percentage;
		}
	}
}

static void dce12_update_clocks(struct clk_mgr *clk_mgr_base,
			struct dc_state *context,
			bool safe_to_lower)
{
	struct clk_mgr_internal *clk_mgr_dce = TO_CLK_MGR_INTERNAL(clk_mgr_base);
	struct dm_pp_clock_for_voltage_req clock_voltage_req = {0};
	int max_pix_clk = dce_get_max_pixel_clock_for_all_paths(context);
	int patched_disp_clk = context->bw_ctx.bw.dce.dispclk_khz;

	 
	if (!clk_mgr_dce->dfs_bypass_active)
		patched_disp_clk = patched_disp_clk * 115 / 100;

	if (should_set_clock(safe_to_lower, patched_disp_clk, clk_mgr_base->clks.dispclk_khz)) {
		clock_voltage_req.clk_type = DM_PP_CLOCK_TYPE_DISPLAY_CLK;
		 
		if (clk_mgr_dce->xgmi_enabled)
			patched_disp_clk = dce_adjust_dp_ref_freq_for_ss(
					clk_mgr_dce, patched_disp_clk);
		clock_voltage_req.clocks_in_khz = patched_disp_clk;
		clk_mgr_base->clks.dispclk_khz = dce112_set_clock(clk_mgr_base, patched_disp_clk);

		dm_pp_apply_clock_for_voltage_request(clk_mgr_base->ctx, &clock_voltage_req);
	}

	if (should_set_clock(safe_to_lower, max_pix_clk, clk_mgr_base->clks.phyclk_khz)) {
		clock_voltage_req.clk_type = DM_PP_CLOCK_TYPE_DISPLAYPHYCLK;
		clock_voltage_req.clocks_in_khz = max_pix_clk;
		clk_mgr_base->clks.phyclk_khz = max_pix_clk;

		dm_pp_apply_clock_for_voltage_request(clk_mgr_base->ctx, &clock_voltage_req);
	}
	dce11_pplib_apply_display_requirements(clk_mgr_base->ctx->dc, context);
}


static struct clk_mgr_funcs dce120_funcs = {
	.get_dp_ref_clk_frequency = dce12_get_dp_ref_freq_khz,
	.update_clocks = dce12_update_clocks
};

void dce120_clk_mgr_construct(struct dc_context *ctx, struct clk_mgr_internal *clk_mgr)
{
	dce_clk_mgr_construct(ctx, clk_mgr);

	memcpy(clk_mgr->max_clks_by_state,
		dce120_max_clks_by_state,
		sizeof(dce120_max_clks_by_state));

	clk_mgr->base.dprefclk_khz = 600000;
	clk_mgr->base.funcs = &dce120_funcs;
}

void dce121_clk_mgr_construct(struct dc_context *ctx, struct clk_mgr_internal *clk_mgr)
{
	dce120_clk_mgr_construct(ctx, clk_mgr);
	clk_mgr->base.dprefclk_khz = 625000;

	 
	if (dce121_xgmi_enabled(ctx->dc->hwseq))
		dce121_clock_patch_xgmi_ss_info(clk_mgr);

}

