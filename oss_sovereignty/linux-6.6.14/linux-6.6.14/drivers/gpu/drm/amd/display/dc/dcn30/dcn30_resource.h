#ifndef _DCN30_RESOURCE_H_
#define _DCN30_RESOURCE_H_
#include "core_types.h"
#define TO_DCN30_RES_POOL(pool)\
	container_of(pool, struct dcn30_resource_pool, base)
struct dc;
struct resource_pool;
struct _vcs_dpi_display_pipe_params_st;
extern struct _vcs_dpi_ip_params_st dcn3_0_ip;
extern struct _vcs_dpi_soc_bounding_box_st dcn3_0_soc;
struct dcn30_resource_pool {
	struct resource_pool base;
};
struct resource_pool *dcn30_create_resource_pool(
		const struct dc_init_data *init_data,
		struct dc *dc);
void dcn30_set_mcif_arb_params(
		struct dc *dc,
		struct dc_state *context,
		display_e2e_pipe_params_st *pipes,
		int pipe_cnt);
unsigned int dcn30_calc_max_scaled_time(
		unsigned int time_per_pixel,
		enum mmhubbub_wbif_mode mode,
		unsigned int urgent_watermark);
bool dcn30_validate_bandwidth(struct dc *dc, struct dc_state *context,
		bool fast_validate);
bool dcn30_internal_validate_bw(
		struct dc *dc,
		struct dc_state *context,
		display_e2e_pipe_params_st *pipes,
		int *pipe_cnt_out,
		int *vlevel_out,
		bool fast_validate,
		bool allow_self_refresh_only);
void dcn30_calculate_wm_and_dlg(
		struct dc *dc, struct dc_state *context,
		display_e2e_pipe_params_st *pipes,
		int pipe_cnt,
		int vlevel);
void dcn30_update_soc_for_wm_a(struct dc *dc, struct dc_state *context);
void dcn30_populate_dml_writeback_from_context(
		struct dc *dc, struct resource_context *res_ctx, display_e2e_pipe_params_st *pipes);
int dcn30_populate_dml_pipes_from_context(
	struct dc *dc, struct dc_state *context,
	display_e2e_pipe_params_st *pipes,
	bool fast_validate);
bool dcn30_acquire_post_bldn_3dlut(
		struct resource_context *res_ctx,
		const struct resource_pool *pool,
		int mpcc_id,
		struct dc_3dlut **lut,
		struct dc_transfer_func **shaper);
bool dcn30_release_post_bldn_3dlut(
		struct resource_context *res_ctx,
		const struct resource_pool *pool,
		struct dc_3dlut **lut,
		struct dc_transfer_func **shaper);
enum dc_status dcn30_add_stream_to_ctx(
		struct dc *dc,
		struct dc_state *new_ctx,
		struct dc_stream_state *dc_stream);
void dcn30_update_bw_bounding_box(struct dc *dc, struct clk_bw_params *bw_params);
bool dcn30_can_support_mclk_switch_using_fw_based_vblank_stretch(struct dc *dc, struct dc_state *context);
void dcn30_setup_mclk_switch_using_fw_based_vblank_stretch(struct dc *dc, struct dc_state *context);
int dcn30_find_dummy_latency_index_for_fw_based_mclk_switch(struct dc *dc, struct dc_state *context,
		display_e2e_pipe_params_st *pipes, int pipe_cnt, int vlevel);
#endif  
