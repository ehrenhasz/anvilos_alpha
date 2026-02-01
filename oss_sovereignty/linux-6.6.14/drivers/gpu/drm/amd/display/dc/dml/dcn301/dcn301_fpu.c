 
#include "resource.h"
#include "clk_mgr.h"
#include "dcn20/dcn20_resource.h"
#include "dcn301/dcn301_resource.h"
#include "clk_mgr/dcn301/vg_clk_mgr.h"

#include "dml/dcn20/dcn20_fpu.h"
#include "dcn301_fpu.h"

#define TO_DCN301_RES_POOL(pool)\
	container_of(pool, struct dcn301_resource_pool, base)

 
	pipes[0].clks_cfg.voltage = vlevel;
	pipes[0].clks_cfg.dcfclk_mhz = dml->soc.clock_limits[vlevel].dcfclk_mhz;
	pipes[0].clks_cfg.socclk_mhz = dml->soc.clock_limits[vlevel].socclk_mhz;

	dml->soc.dram_clock_change_latency_us = table_entry->pstate_latency_us;
	dml->soc.sr_exit_time_us = table_entry->sr_exit_time_us;
	dml->soc.sr_enter_plus_exit_time_us = table_entry->sr_enter_plus_exit_time_us;

	wm_set->urgent_ns = get_wm_urgent(dml, pipes, pipe_cnt) * 1000;
	wm_set->cstate_pstate.cstate_enter_plus_exit_ns = get_wm_stutter_enter_exit(dml, pipes, pipe_cnt) * 1000;
	wm_set->cstate_pstate.cstate_exit_ns = get_wm_stutter_exit(dml, pipes, pipe_cnt) * 1000;
	wm_set->cstate_pstate.pstate_change_ns = get_wm_dram_clock_change(dml, pipes, pipe_cnt) * 1000;
	wm_set->pte_meta_urgent_ns = get_wm_memory_trip(dml, pipes, pipe_cnt) * 1000;
	wm_set->frac_urg_bw_nom = get_fraction_of_urgent_bandwidth(dml, pipes, pipe_cnt) * 1000;
	wm_set->frac_urg_bw_flip = get_fraction_of_urgent_bandwidth_imm_flip(dml, pipes, pipe_cnt) * 1000;
	wm_set->urgent_latency_ns = get_urgent_latency(dml, pipes, pipe_cnt) * 1000;
	dml->soc.dram_clock_change_latency_us = dram_clock_change_latency_cached;

}

void dcn301_update_bw_bounding_box(struct dc *dc, struct clk_bw_params *bw_params)
{
	struct _vcs_dpi_voltage_scaling_st *s = dc->scratch.update_bw_bounding_box.clock_limits;
	struct dcn301_resource_pool *pool = TO_DCN301_RES_POOL(dc->res_pool);
	struct clk_limit_table *clk_table = &bw_params->clk_table;
	unsigned int i, closest_clk_lvl;
	int j;

	dc_assert_fp_enabled();

	memcpy(s, dcn3_01_soc.clock_limits, sizeof(dcn3_01_soc.clock_limits));

	 
	dcn3_01_ip.max_num_otg = pool->base.res_cap->num_timing_generator;
	dcn3_01_ip.max_num_dpp = pool->base.pipe_count;
	dcn3_01_soc.num_chans = bw_params->num_channels;

	ASSERT(clk_table->num_entries);
	for (i = 0; i < clk_table->num_entries; i++) {
		 
		for (closest_clk_lvl = 0, j = dcn3_01_soc.num_states - 1; j >= 0; j--) {
			if ((unsigned int) dcn3_01_soc.clock_limits[j].dcfclk_mhz <= clk_table->entries[i].dcfclk_mhz) {
				closest_clk_lvl = j;
				break;
			}
		}

		s[i].state = i;
		s[i].dcfclk_mhz = clk_table->entries[i].dcfclk_mhz;
		s[i].fabricclk_mhz = clk_table->entries[i].fclk_mhz;
		s[i].socclk_mhz = clk_table->entries[i].socclk_mhz;
		s[i].dram_speed_mts = clk_table->entries[i].memclk_mhz * 2;

		s[i].dispclk_mhz = dcn3_01_soc.clock_limits[closest_clk_lvl].dispclk_mhz;
		s[i].dppclk_mhz = dcn3_01_soc.clock_limits[closest_clk_lvl].dppclk_mhz;
		s[i].dram_bw_per_chan_gbps =
			dcn3_01_soc.clock_limits[closest_clk_lvl].dram_bw_per_chan_gbps;
		s[i].dscclk_mhz = dcn3_01_soc.clock_limits[closest_clk_lvl].dscclk_mhz;
		s[i].dtbclk_mhz = dcn3_01_soc.clock_limits[closest_clk_lvl].dtbclk_mhz;
		s[i].phyclk_d18_mhz =
			dcn3_01_soc.clock_limits[closest_clk_lvl].phyclk_d18_mhz;
		s[i].phyclk_mhz = dcn3_01_soc.clock_limits[closest_clk_lvl].phyclk_mhz;
	}

	if (clk_table->num_entries) {
		dcn3_01_soc.num_states = clk_table->num_entries;
		 
		s[dcn3_01_soc.num_states] =
			dcn3_01_soc.clock_limits[dcn3_01_soc.num_states - 1];
		s[dcn3_01_soc.num_states].state = dcn3_01_soc.num_states;
	}

	memcpy(dcn3_01_soc.clock_limits, s, sizeof(dcn3_01_soc.clock_limits));

	dcn3_01_soc.dispclk_dppclk_vco_speed_mhz = dc->clk_mgr->dentist_vco_freq_khz / 1000.0;
	dc->dml.soc.dispclk_dppclk_vco_speed_mhz = dc->clk_mgr->dentist_vco_freq_khz / 1000.0;

	if ((int)(dcn3_01_soc.dram_clock_change_latency_us * 1000)
				!= dc->debug.dram_clock_change_latency_ns
			&& dc->debug.dram_clock_change_latency_ns) {
		dcn3_01_soc.dram_clock_change_latency_us = dc->debug.dram_clock_change_latency_ns / 1000.0;
	}
	dml_init_instance(&dc->dml, &dcn3_01_soc, &dcn3_01_ip, DML_PROJECT_DCN30);
}

void dcn301_fpu_set_wm_ranges(int i,
	struct pp_smu_wm_range_sets *ranges,
	struct _vcs_dpi_soc_bounding_box_st *loaded_bb)
{
	dc_assert_fp_enabled();

	ranges->reader_wm_sets[i].min_fill_clk_mhz = (i > 0) ? (loaded_bb->clock_limits[i - 1].dram_speed_mts / 16) + 1 : 0;
	ranges->reader_wm_sets[i].max_fill_clk_mhz = loaded_bb->clock_limits[i].dram_speed_mts / 16;
}

void dcn301_fpu_init_soc_bounding_box(struct bp_soc_bb_info bb_info)
{
	dc_assert_fp_enabled();

	if (bb_info.dram_clock_change_latency_100ns > 0)
		dcn3_01_soc.dram_clock_change_latency_us = bb_info.dram_clock_change_latency_100ns * 10;

	if (bb_info.dram_sr_enter_exit_latency_100ns > 0)
		dcn3_01_soc.sr_enter_plus_exit_time_us = bb_info.dram_sr_enter_exit_latency_100ns * 10;

	if (bb_info.dram_sr_exit_latency_100ns > 0)
		dcn3_01_soc.sr_exit_time_us = bb_info.dram_sr_exit_latency_100ns * 10;
}

void dcn301_calculate_wm_and_dlg_fp(struct dc *dc,
		struct dc_state *context,
		display_e2e_pipe_params_st *pipes,
		int pipe_cnt,
		int vlevel_req)
{
	int i, pipe_idx;
	int vlevel, vlevel_max;
	struct wm_range_table_entry *table_entry;
	struct clk_bw_params *bw_params = dc->clk_mgr->bw_params;

	ASSERT(bw_params);
	dc_assert_fp_enabled();

	vlevel_max = bw_params->clk_table.num_entries - 1;

	 
	table_entry = &bw_params->wm_table.entries[WM_D];
	if (table_entry->wm_type == WM_TYPE_RETRAINING)
		vlevel = 0;
	else
		vlevel = vlevel_max;
	calculate_wm_set_for_vlevel(vlevel, table_entry, &context->bw_ctx.bw.dcn.watermarks.d,
						&context->bw_ctx.dml, pipes, pipe_cnt);
	 
	table_entry = &bw_params->wm_table.entries[WM_C];
	vlevel = min(max(vlevel_req, 2), vlevel_max);
	calculate_wm_set_for_vlevel(vlevel, table_entry, &context->bw_ctx.bw.dcn.watermarks.c,
						&context->bw_ctx.dml, pipes, pipe_cnt);
	 
	table_entry = &bw_params->wm_table.entries[WM_B];
	vlevel = min(max(vlevel_req, 1), vlevel_max);
	calculate_wm_set_for_vlevel(vlevel, table_entry, &context->bw_ctx.bw.dcn.watermarks.b,
						&context->bw_ctx.dml, pipes, pipe_cnt);

	 
	table_entry = &bw_params->wm_table.entries[WM_A];
	vlevel = min(vlevel_req, vlevel_max);
	calculate_wm_set_for_vlevel(vlevel, table_entry, &context->bw_ctx.bw.dcn.watermarks.a,
						&context->bw_ctx.dml, pipes, pipe_cnt);

	for (i = 0, pipe_idx = 0; i < dc->res_pool->pipe_count; i++) {
		if (!context->res_ctx.pipe_ctx[i].stream)
			continue;

		pipes[pipe_idx].clks_cfg.dispclk_mhz = get_dispclk_calculated(&context->bw_ctx.dml, pipes, pipe_cnt);
		pipes[pipe_idx].clks_cfg.dppclk_mhz = get_dppclk_calculated(&context->bw_ctx.dml, pipes, pipe_cnt, pipe_idx);

		if (dc->config.forced_clocks) {
			pipes[pipe_idx].clks_cfg.dispclk_mhz = context->bw_ctx.dml.soc.clock_limits[0].dispclk_mhz;
			pipes[pipe_idx].clks_cfg.dppclk_mhz = context->bw_ctx.dml.soc.clock_limits[0].dppclk_mhz;
		}
		if (dc->debug.min_disp_clk_khz > pipes[pipe_idx].clks_cfg.dispclk_mhz * 1000)
			pipes[pipe_idx].clks_cfg.dispclk_mhz = dc->debug.min_disp_clk_khz / 1000.0;
		if (dc->debug.min_dpp_clk_khz > pipes[pipe_idx].clks_cfg.dppclk_mhz * 1000)
			pipes[pipe_idx].clks_cfg.dppclk_mhz = dc->debug.min_dpp_clk_khz / 1000.0;
		pipe_idx++;
	}

	dcn20_calculate_dlg_params(dc, context, pipes, pipe_cnt, vlevel);
}
