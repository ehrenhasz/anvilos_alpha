#ifndef DC_STREAM_H_
#define DC_STREAM_H_
#include "dc_types.h"
#include "grph_object_defs.h"
struct timing_sync_info {
	int group_id;
	int group_size;
	bool master;
};
struct dc_stream_status {
	int primary_otg_inst;
	int stream_enc_inst;
	int plane_count;
	int audio_inst;
	struct timing_sync_info timing_sync_info;
	struct dc_plane_state *plane_states[MAX_SURFACE_NUM];
	bool is_abm_supported;
};
enum hubp_dmdata_mode {
	DMDATA_SW_MODE,
	DMDATA_HW_MODE
};
struct dc_dmdata_attributes {
	enum hubp_dmdata_mode dmdata_mode;
	bool dmdata_repeat;
	uint32_t dmdata_size;
	bool dmdata_updated;
	PHYSICAL_ADDRESS_LOC address;
	bool dmdata_qos_mode;
	uint32_t dmdata_qos_level;
	uint32_t dmdata_dl_delta;
	uint32_t *dmdata_sw_data;
};
struct dc_writeback_info {
	bool wb_enabled;
	int dwb_pipe_inst;
	struct dc_dwb_params dwb_params;
	struct mcif_buf_params mcif_buf_params;
	struct mcif_warmup_params mcif_warmup_params;
	struct dc_plane_state *writeback_source_plane;
	int mpcc_inst;
};
struct dc_writeback_update {
	unsigned int num_wb_info;
	struct dc_writeback_info writeback_info[MAX_DWB_PIPES];
};
enum vertical_interrupt_ref_point {
	START_V_UPDATE = 0,
	START_V_SYNC,
	INVALID_POINT
};
struct periodic_interrupt_config {
	enum vertical_interrupt_ref_point ref_point;
	int lines_offset;
};
struct dc_mst_stream_bw_update {
	bool is_increase;  
	uint32_t mst_stream_bw;  
};
union stream_update_flags {
	struct {
		uint32_t scaling:1;
		uint32_t out_tf:1;
		uint32_t out_csc:1;
		uint32_t abm_level:1;
		uint32_t dpms_off:1;
		uint32_t gamut_remap:1;
		uint32_t wb_update:1;
		uint32_t dsc_changed : 1;
		uint32_t mst_bw : 1;
		uint32_t crtc_timing_adjust : 1;
		uint32_t fams_changed : 1;
	} bits;
	uint32_t raw;
};
struct test_pattern {
	enum dp_test_pattern type;
	enum dp_test_pattern_color_space color_space;
	struct link_training_settings const *p_link_settings;
	unsigned char const *p_custom_pattern;
	unsigned int cust_pattern_size;
};
#define SUBVP_DRR_MARGIN_US 100  
enum mall_stream_type {
	SUBVP_NONE,  
	SUBVP_MAIN,  
	SUBVP_PHANTOM,  
};
struct mall_stream_config {
	enum mall_stream_type type;
	struct dc_stream_state *paired_stream;	 
};
struct mall_temp_config {
	struct mall_stream_config mall_stream_config[MAX_PIPES];
	bool is_phantom_plane[MAX_PIPES];
};
struct dc_stream_debug_options {
	char force_odm_combine_segments;
};
struct dc_stream_state {
	struct dc_sink *sink;
	struct dc_link *link;
	struct link_encoder *link_enc;
	struct dc_stream_debug_options debug;
	struct dc_panel_patch sink_patches;
	union display_content_support content_support;
	struct dc_crtc_timing timing;
	struct dc_crtc_timing_adjust adjust;
	struct dc_info_packet vrr_infopacket;
	struct dc_info_packet vsc_infopacket;
	struct dc_info_packet vsp_infopacket;
	struct dc_info_packet hfvsif_infopacket;
	struct dc_info_packet vtem_infopacket;
	struct dc_info_packet adaptive_sync_infopacket;
	uint8_t dsc_packed_pps[128];
	struct rect src;  
	struct rect dst;  
	struct audio_info audio_info;
	struct dc_info_packet hdr_static_metadata;
	PHYSICAL_ADDRESS_LOC dmdata_address;
	bool   use_dynamic_meta;
	struct dc_transfer_func *out_transfer_func;
	struct colorspace_transform gamut_remap_matrix;
	struct dc_csc_transform csc_color_matrix;
	enum dc_color_space output_color_space;
	enum dc_dither_option dither_option;
	enum view_3d_format view_format;
	bool use_vsc_sdp_for_colorimetry;
	bool ignore_msa_timing_param;
	bool allow_freesync;
	bool vrr_active_variable;
	bool freesync_on_desktop;
	bool vrr_active_fixed;
	bool converter_disable_audio;
	uint8_t qs_bit;
	uint8_t qy_bit;
	unsigned int abm_level;
	struct periodic_interrupt_config periodic_interrupt;
	struct dc_context *ctx;
	struct bit_depth_reduction_params bit_depth_params;
	struct clamping_and_pixel_encoding_params clamping;
	int phy_pix_clk;
	enum signal_type signal;
	bool dpms_off;
	void *dm_stream_context;
	struct dc_cursor_attributes cursor_attributes;
	struct dc_cursor_position cursor_position;
	uint32_t sdr_white_level;  
	struct kref refcount;
	struct crtc_trigger_info triggered_crtc_reset;
	unsigned int num_wb_info;
	struct dc_writeback_info writeback_info[MAX_DWB_PIPES];
	const struct dc_transfer_func *func_shaper;
	const struct dc_3dlut *lut3d_func;
	bool mode_changed : 1;
	struct {
		uint8_t otg_offset;
	} out;
	bool apply_edp_fast_boot_optimization;
	bool apply_seamless_boot_optimization;
	uint32_t apply_boot_odm_mode;
	uint32_t stream_id;
	struct test_pattern test_pattern;
	union stream_update_flags update_flags;
	bool has_non_synchronizable_pclk;
	bool vblank_synchronized;
	bool fpo_in_use;
	struct mall_stream_config mall_stream_config;
};
#define ABM_LEVEL_IMMEDIATE_DISABLE 255
struct dc_stream_update {
	struct dc_stream_state *stream;
	struct rect src;
	struct rect dst;
	struct dc_transfer_func *out_transfer_func;
	struct dc_info_packet *hdr_static_metadata;
	unsigned int *abm_level;
	struct periodic_interrupt_config *periodic_interrupt;
	struct dc_info_packet *vrr_infopacket;
	struct dc_info_packet *vsc_infopacket;
	struct dc_info_packet *vsp_infopacket;
	struct dc_info_packet *hfvsif_infopacket;
	struct dc_info_packet *vtem_infopacket;
	struct dc_info_packet *adaptive_sync_infopacket;
	bool *dpms_off;
	bool integer_scaling_update;
	bool *allow_freesync;
	bool *vrr_active_variable;
	bool *vrr_active_fixed;
	struct colorspace_transform *gamut_remap;
	enum dc_color_space *output_color_space;
	enum dc_dither_option *dither_option;
	struct dc_csc_transform *output_csc_transform;
	struct dc_writeback_update *wb_update;
	struct dc_dsc_config *dsc_config;
	struct dc_mst_stream_bw_update *mst_bw_update;
	struct dc_transfer_func *func_shaper;
	struct dc_3dlut *lut3d_func;
	struct test_pattern *pending_test_pattern;
	struct dc_crtc_timing_adjust *crtc_timing_adjust;
};
bool dc_is_stream_unchanged(
	struct dc_stream_state *old_stream, struct dc_stream_state *stream);
bool dc_is_stream_scaling_unchanged(
	struct dc_stream_state *old_stream, struct dc_stream_state *stream);
bool dc_update_planes_and_stream(struct dc *dc,
		struct dc_surface_update *surface_updates, int surface_count,
		struct dc_stream_state *dc_stream,
		struct dc_stream_update *stream_update);
void dc_commit_updates_for_stream(struct dc *dc,
		struct dc_surface_update *srf_updates,
		int surface_count,
		struct dc_stream_state *stream,
		struct dc_stream_update *stream_update,
		struct dc_state *state);
void dc_stream_log(const struct dc *dc, const struct dc_stream_state *stream);
uint8_t dc_get_current_stream_count(struct dc *dc);
struct dc_stream_state *dc_get_stream_at_index(struct dc *dc, uint8_t i);
uint32_t dc_stream_get_vblank_counter(const struct dc_stream_state *stream);
bool dc_stream_send_dp_sdp(const struct dc_stream_state *stream,
		const uint8_t *custom_sdp_message,
		unsigned int sdp_message_size);
bool dc_stream_get_scanoutpos(const struct dc_stream_state *stream,
				  uint32_t *v_blank_start,
				  uint32_t *v_blank_end,
				  uint32_t *h_position,
				  uint32_t *v_position);
enum dc_status dc_add_stream_to_ctx(
			struct dc *dc,
		struct dc_state *new_ctx,
		struct dc_stream_state *stream);
enum dc_status dc_remove_stream_from_ctx(
		struct dc *dc,
			struct dc_state *new_ctx,
			struct dc_stream_state *stream);
bool dc_add_plane_to_context(
		const struct dc *dc,
		struct dc_stream_state *stream,
		struct dc_plane_state *plane_state,
		struct dc_state *context);
bool dc_remove_plane_from_context(
		const struct dc *dc,
		struct dc_stream_state *stream,
		struct dc_plane_state *plane_state,
		struct dc_state *context);
bool dc_rem_all_planes_for_stream(
		const struct dc *dc,
		struct dc_stream_state *stream,
		struct dc_state *context);
bool dc_add_all_planes_for_stream(
		const struct dc *dc,
		struct dc_stream_state *stream,
		struct dc_plane_state * const *plane_states,
		int plane_count,
		struct dc_state *context);
bool dc_stream_add_writeback(struct dc *dc,
		struct dc_stream_state *stream,
		struct dc_writeback_info *wb_info);
bool dc_stream_remove_writeback(struct dc *dc,
		struct dc_stream_state *stream,
		uint32_t dwb_pipe_inst);
enum dc_status dc_stream_add_dsc_to_resource(struct dc *dc,
		struct dc_state *state,
		struct dc_stream_state *stream);
bool dc_stream_warmup_writeback(struct dc *dc,
		int num_dwb,
		struct dc_writeback_info *wb_info);
bool dc_stream_dmdata_status_done(struct dc *dc, struct dc_stream_state *stream);
bool dc_stream_set_dynamic_metadata(struct dc *dc,
		struct dc_stream_state *stream,
		struct dc_dmdata_attributes *dmdata_attr);
enum dc_status dc_validate_stream(struct dc *dc, struct dc_stream_state *stream);
void dc_enable_stereo(
	struct dc *dc,
	struct dc_state *context,
	struct dc_stream_state *streams[],
	uint8_t stream_count);
void dc_trigger_sync(struct dc *dc, struct dc_state *context);
enum surface_update_type dc_check_update_surfaces_for_stream(
		struct dc *dc,
		struct dc_surface_update *updates,
		int surface_count,
		struct dc_stream_update *stream_update,
		const struct dc_stream_status *stream_status);
struct dc_stream_state *dc_create_stream_for_sink(struct dc_sink *dc_sink);
struct dc_stream_state *dc_copy_stream(const struct dc_stream_state *stream);
void update_stream_signal(struct dc_stream_state *stream, struct dc_sink *sink);
void dc_stream_retain(struct dc_stream_state *dc_stream);
void dc_stream_release(struct dc_stream_state *dc_stream);
struct dc_stream_status *dc_stream_get_status_from_state(
	struct dc_state *state,
	struct dc_stream_state *stream);
struct dc_stream_status *dc_stream_get_status(
	struct dc_stream_state *dc_stream);
#ifndef TRIM_FSFT
bool dc_optimize_timing_for_fsft(
	struct dc_stream_state *pStream,
	unsigned int max_input_rate_in_khz);
#endif
bool dc_stream_set_cursor_attributes(
	struct dc_stream_state *stream,
	const struct dc_cursor_attributes *attributes);
bool dc_stream_set_cursor_position(
	struct dc_stream_state *stream,
	const struct dc_cursor_position *position);
bool dc_stream_adjust_vmin_vmax(struct dc *dc,
				struct dc_stream_state *stream,
				struct dc_crtc_timing_adjust *adjust);
bool dc_stream_get_last_used_drr_vtotal(struct dc *dc,
		struct dc_stream_state *stream,
		uint32_t *refresh_rate);
bool dc_stream_get_crtc_position(struct dc *dc,
				 struct dc_stream_state **stream,
				 int num_streams,
				 unsigned int *v_pos,
				 unsigned int *nom_v_pos);
#if defined(CONFIG_DRM_AMD_SECURE_DISPLAY)
bool dc_stream_forward_crc_window(struct dc_stream_state *stream,
		struct rect *rect,
		bool is_stop);
#endif
bool dc_stream_configure_crc(struct dc *dc,
			     struct dc_stream_state *stream,
			     struct crc_params *crc_window,
			     bool enable,
			     bool continuous);
bool dc_stream_get_crc(struct dc *dc,
		       struct dc_stream_state *stream,
		       uint32_t *r_cr,
		       uint32_t *g_y,
		       uint32_t *b_cb);
void dc_stream_set_static_screen_params(struct dc *dc,
					struct dc_stream_state **stream,
					int num_streams,
					const struct dc_static_screen_params *params);
void dc_stream_set_dyn_expansion(struct dc *dc, struct dc_stream_state *stream,
		enum dc_dynamic_expansion option);
void dc_stream_set_dither_option(struct dc_stream_state *stream,
				 enum dc_dither_option option);
bool dc_stream_set_gamut_remap(struct dc *dc,
			       const struct dc_stream_state *stream);
bool dc_stream_program_csc_matrix(struct dc *dc,
				  struct dc_stream_state *stream);
bool dc_stream_get_crtc_position(struct dc *dc,
				 struct dc_stream_state **stream,
				 int num_streams,
				 unsigned int *v_pos,
				 unsigned int *nom_v_pos);
struct pipe_ctx *dc_stream_get_pipe_ctx(struct dc_stream_state *stream);
void dc_dmub_update_dirty_rect(struct dc *dc,
			       int surface_count,
			       struct dc_stream_state *stream,
			       struct dc_surface_update *srf_updates,
			       struct dc_state *context);
#endif  
