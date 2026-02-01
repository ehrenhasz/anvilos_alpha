 


#include "dccg.h"
#include "clk_mgr_internal.h"
#include "dce100/dce_clk_mgr.h"
#include "dce110/dce110_clk_mgr.h"
#include "dce60_clk_mgr.h"
#include "reg_helper.h"
#include "dmcu.h"
#include "core_types.h"
#include "dal_asic_id.h"

 
#include "dce/dce_6_0_d.h"
#include "dce/dce_6_0_sh_mask.h"

#define REG(reg) \
	(clk_mgr->regs->reg)

#undef FN
#define FN(reg_name, field_name) \
	clk_mgr->clk_mgr_shift->field_name, clk_mgr->clk_mgr_mask->field_name

 
#define SR(reg_name)\
	.reg_name = mm ## reg_name

static const struct clk_mgr_registers disp_clk_regs = {
		CLK_COMMON_REG_LIST_DCE60_BASE()
};

static const struct clk_mgr_shift disp_clk_shift = {
		CLK_COMMON_MASK_SH_LIST_DCE60_COMMON_BASE(__SHIFT)
};

static const struct clk_mgr_mask disp_clk_mask = {
		CLK_COMMON_MASK_SH_LIST_DCE60_COMMON_BASE(_MASK)
};


 
static const struct state_dependent_clocks dce60_max_clks_by_state[] = {
 
{ .display_clk_khz = 0, .pixel_clk_khz = 0 },
 
{ .display_clk_khz = 0, .pixel_clk_khz = 0 },
 
{ .display_clk_khz = 352000, .pixel_clk_khz = 330000},
 
{ .display_clk_khz = 600000, .pixel_clk_khz = 400000 },
 
{ .display_clk_khz = 600000, .pixel_clk_khz = 400000 } };

static int dce60_get_dp_ref_freq_khz(struct clk_mgr *clk_mgr_base)
{
	struct clk_mgr_internal *clk_mgr = TO_CLK_MGR_INTERNAL(clk_mgr_base);
	int dprefclk_wdivider;
	int dp_ref_clk_khz;
	int target_div;

	 

	 
	REG_GET(DENTIST_DISPCLK_CNTL, DENTIST_DPREFCLK_WDIVIDER, &dprefclk_wdivider);

	 
	target_div = dentist_get_divider_from_did(dprefclk_wdivider);

	 
	dp_ref_clk_khz = (DENTIST_DIVIDER_RANGE_SCALE_FACTOR
		* clk_mgr->base.dentist_vco_freq_khz) / target_div;

	return dce_adjust_dp_ref_freq_for_ss(clk_mgr, dp_ref_clk_khz);
}

static void dce60_pplib_apply_display_requirements(
	struct dc *dc,
	struct dc_state *context)
{
	struct dm_pp_display_configuration *pp_display_cfg = &context->pp_display_cfg;

	pp_display_cfg->avail_mclk_switch_time_us = dce110_get_min_vblank_time_us(context);

	dce110_fill_display_configs(context, pp_display_cfg);

	if (memcmp(&dc->current_state->pp_display_cfg, pp_display_cfg, sizeof(*pp_display_cfg)) !=  0)
		dm_pp_apply_display_requirements(dc->ctx, pp_display_cfg);
}

static void dce60_update_clocks(struct clk_mgr *clk_mgr_base,
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
	dce60_pplib_apply_display_requirements(clk_mgr_base->ctx->dc, context);
}








static struct clk_mgr_funcs dce60_funcs = {
	.get_dp_ref_clk_frequency = dce60_get_dp_ref_freq_khz,
	.update_clocks = dce60_update_clocks
};

void dce60_clk_mgr_construct(
		struct dc_context *ctx,
		struct clk_mgr_internal *clk_mgr)
{
	dce_clk_mgr_construct(ctx, clk_mgr);

	memcpy(clk_mgr->max_clks_by_state,
		dce60_max_clks_by_state,
		sizeof(dce60_max_clks_by_state));

	clk_mgr->regs = &disp_clk_regs;
	clk_mgr->clk_mgr_shift = &disp_clk_shift;
	clk_mgr->clk_mgr_mask = &disp_clk_mask;
	clk_mgr->base.funcs = &dce60_funcs;
}

