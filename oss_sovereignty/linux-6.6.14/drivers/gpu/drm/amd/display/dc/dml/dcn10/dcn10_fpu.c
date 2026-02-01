
 

#include "dcn10/dcn10_resource.h"

#include "dcn10_fpu.h"
#include "resource.h"
#include "amdgpu_dm/dc_fpu.h"

 

struct _vcs_dpi_ip_params_st dcn1_0_ip = {
	.rob_buffer_size_kbytes = 64,
	.det_buffer_size_kbytes = 164,
	.dpte_buffer_size_in_pte_reqs_luma = 42,
	.dpp_output_buffer_pixels = 2560,
	.opp_output_buffer_lines = 1,
	.pixel_chunk_size_kbytes = 8,
	.pte_enable = 1,
	.pte_chunk_size_kbytes = 2,
	.meta_chunk_size_kbytes = 2,
	.writeback_chunk_size_kbytes = 2,
	.line_buffer_size_bits = 589824,
	.max_line_buffer_lines = 12,
	.IsLineBufferBppFixed = 0,
	.LineBufferFixedBpp = -1,
	.writeback_luma_buffer_size_kbytes = 12,
	.writeback_chroma_buffer_size_kbytes = 8,
	.max_num_dpp = 4,
	.max_num_wb = 2,
	.max_dchub_pscl_bw_pix_per_clk = 4,
	.max_pscl_lb_bw_pix_per_clk = 2,
	.max_lb_vscl_bw_pix_per_clk = 4,
	.max_vscl_hscl_bw_pix_per_clk = 4,
	.max_hscl_ratio = 4,
	.max_vscl_ratio = 4,
	.hscl_mults = 4,
	.vscl_mults = 4,
	.max_hscl_taps = 8,
	.max_vscl_taps = 8,
	.dispclk_ramp_margin_percent = 1,
	.underscan_factor = 1.10,
	.min_vblank_lines = 14,
	.dppclk_delay_subtotal = 90,
	.dispclk_delay_subtotal = 42,
	.dcfclk_cstate_latency = 10,
	.max_inter_dcn_tile_repeaters = 8,
	.can_vstartup_lines_exceed_vsync_plus_back_porch_lines_minus_one = 0,
	.bug_forcing_LC_req_same_size_fixed = 0,
};

struct _vcs_dpi_soc_bounding_box_st dcn1_0_soc = {
	.sr_exit_time_us = 9.0,
	.sr_enter_plus_exit_time_us = 11.0,
	.urgent_latency_us = 4.0,
	.writeback_latency_us = 12.0,
	.ideal_dram_bw_after_urgent_percent = 80.0,
	.max_request_size_bytes = 256,
	.downspread_percent = 0.5,
	.dram_page_open_time_ns = 50.0,
	.dram_rw_turnaround_time_ns = 17.5,
	.dram_return_buffer_per_channel_bytes = 8192,
	.round_trip_ping_latency_dcfclk_cycles = 128,
	.urgent_out_of_order_return_per_channel_bytes = 256,
	.channel_interleave_bytes = 256,
	.num_banks = 8,
	.num_chans = 2,
	.vmm_page_size_bytes = 4096,
	.dram_clock_change_latency_us = 17.0,
	.writeback_dram_clock_change_latency_us = 23.0,
	.return_bus_width_bytes = 64,
};

void dcn10_resource_construct_fp(struct dc *dc)
{
	dc_assert_fp_enabled();
	if (dc->ctx->dce_version == DCN_VERSION_1_01) {
		struct dcn_soc_bounding_box *dcn_soc = dc->dcn_soc;
		struct dcn_ip_params *dcn_ip = dc->dcn_ip;
		struct display_mode_lib *dml = &dc->dml;

		dml->ip.max_num_dpp = 3;
		 
		dcn_soc->dram_clock_change_latency = 23;
		dcn_ip->max_num_dpp = 3;
	}
	if (ASICREV_IS_RV1_F0(dc->ctx->asic_id.hw_internal_rev)) {
		dc->dcn_soc->urgent_latency = 3;
		dc->debug.disable_dmcu = true;
		dc->dcn_soc->fabric_and_dram_bandwidth_vmax0p9 = 41.60f;
	}

	dc->dcn_soc->number_of_channels = dc->ctx->asic_id.vram_width / ddr4_dram_width;
	ASSERT(dc->dcn_soc->number_of_channels < 3);
	if (dc->dcn_soc->number_of_channels == 0) 
		dc->dcn_soc->number_of_channels = 2;

	if (dc->dcn_soc->number_of_channels == 1) {
		dc->dcn_soc->fabric_and_dram_bandwidth_vmax0p9 = 19.2f;
		dc->dcn_soc->fabric_and_dram_bandwidth_vnom0p8 = 17.066f;
		dc->dcn_soc->fabric_and_dram_bandwidth_vmid0p72 = 14.933f;
		dc->dcn_soc->fabric_and_dram_bandwidth_vmin0p65 = 12.8f;
		if (ASICREV_IS_RV1_F0(dc->ctx->asic_id.hw_internal_rev))
			dc->dcn_soc->fabric_and_dram_bandwidth_vmax0p9 = 20.80f;
	}
}
