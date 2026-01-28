#ifndef __DC_MPCC_H__
#define __DC_MPCC_H__
#include "dc_hw_types.h"
#include "hw_shared.h"
#include "transform.h"
#define MAX_MPCC 6
#define MAX_OPP 6
#define MAX_DWB		2
enum mpc_output_csc_mode {
	MPC_OUTPUT_CSC_DISABLE = 0,
	MPC_OUTPUT_CSC_COEF_A,
	MPC_OUTPUT_CSC_COEF_B
};
enum mpcc_blend_mode {
	MPCC_BLEND_MODE_BYPASS,
	MPCC_BLEND_MODE_TOP_LAYER_PASSTHROUGH,
	MPCC_BLEND_MODE_TOP_LAYER_ONLY,
	MPCC_BLEND_MODE_TOP_BOT_BLENDING
};
enum mpcc_alpha_blend_mode {
	MPCC_ALPHA_BLEND_MODE_PER_PIXEL_ALPHA,
	MPCC_ALPHA_BLEND_MODE_PER_PIXEL_ALPHA_COMBINED_GLOBAL_GAIN,
	MPCC_ALPHA_BLEND_MODE_GLOBAL_ALPHA
};
struct mpcc_blnd_cfg {
	struct tg_color black_color;	 
	enum mpcc_alpha_blend_mode alpha_mode;	 
	bool pre_multiplied_alpha;	 
	int global_gain;
	int global_alpha;
	bool overlap_only;
	int bottom_gain_mode;
	int background_color_bpc;
	int top_gain;
	int bottom_inside_gain;
	int bottom_outside_gain;
};
struct mpc_grph_gamut_adjustment {
	struct fixed31_32 temperature_matrix[CSC_TEMPERATURE_MATRIX_SIZE];
	enum graphics_gamut_adjust_type gamut_adjust_type;
};
struct mpcc_sm_cfg {
	bool enable;
	int sm_mode;
	bool frame_alt;
	bool field_alt;
	int force_next_frame_porlarity;
	int force_next_field_polarity;
};
struct mpc_denorm_clamp {
	int clamp_max_r_cr;
	int clamp_min_r_cr;
	int clamp_max_g_y;
	int clamp_min_g_y;
	int clamp_max_b_cb;
	int clamp_min_b_cb;
};
struct mpc_dwb_flow_control {
	int flow_ctrl_mode;
	int flow_ctrl_cnt0;
	int flow_ctrl_cnt1;
};
struct mpcc {
	int mpcc_id;			 
	int dpp_id;			 
	struct mpcc *mpcc_bot;		 
	struct mpcc_blnd_cfg blnd_cfg;	 
	struct mpcc_sm_cfg sm_cfg;	 
	bool shared_bottom;		 
};
struct mpc_tree {
	int opp_id;			 
	struct mpcc *opp_list;		 
};
struct mpc {
	const struct mpc_funcs *funcs;
	struct dc_context *ctx;
	struct mpcc mpcc_array[MAX_MPCC];
	struct pwl_params blender_params;
	bool cm_bypass_mode;
};
struct mpcc_state {
	uint32_t opp_id;
	uint32_t dpp_id;
	uint32_t bot_mpcc_id;
	uint32_t mode;
	uint32_t alpha_mode;
	uint32_t pre_multiplied_alpha;
	uint32_t overlap_only;
	uint32_t idle;
	uint32_t busy;
};
struct mpc_funcs {
	void (*read_mpcc_state)(
			struct mpc *mpc,
			int mpcc_inst,
			struct mpcc_state *s);
	struct mpcc* (*insert_plane)(
			struct mpc *mpc,
			struct mpc_tree *tree,
			struct mpcc_blnd_cfg *blnd_cfg,
			struct mpcc_sm_cfg *sm_cfg,
			struct mpcc *insert_above_mpcc,
			int dpp_id,
			int mpcc_id);
	void (*remove_mpcc)(
			struct mpc *mpc,
			struct mpc_tree *tree,
			struct mpcc *mpcc);
	void (*mpc_init)(struct mpc *mpc);
	void (*mpc_init_single_inst)(
			struct mpc *mpc,
			unsigned int mpcc_id);
	void (*update_blending)(
		struct mpc *mpc,
		struct mpcc_blnd_cfg *blnd_cfg,
		int mpcc_id);
	void (*cursor_lock)(
			struct mpc *mpc,
			int opp_id,
			bool lock);
	struct mpcc* (*insert_plane_to_secondary)(
			struct mpc *mpc,
			struct mpc_tree *tree,
			struct mpcc_blnd_cfg *blnd_cfg,
			struct mpcc_sm_cfg *sm_cfg,
			struct mpcc *insert_above_mpcc,
			int dpp_id,
			int mpcc_id);
	void (*remove_mpcc_from_secondary)(
			struct mpc *mpc,
			struct mpc_tree *tree,
			struct mpcc *mpcc);
	struct mpcc* (*get_mpcc_for_dpp_from_secondary)(
			struct mpc_tree *tree,
			int dpp_id);
	struct mpcc* (*get_mpcc_for_dpp)(
			struct mpc_tree *tree,
			int dpp_id);
	void (*wait_for_idle)(struct mpc *mpc, int id);
	void (*assert_mpcc_idle_before_connect)(struct mpc *mpc, int mpcc_id);
	void (*init_mpcc_list_from_hw)(
		struct mpc *mpc,
		struct mpc_tree *tree);
	void (*set_denorm)(struct mpc *mpc,
			int opp_id,
			enum dc_color_depth output_depth);
	void (*set_denorm_clamp)(
			struct mpc *mpc,
			int opp_id,
			struct mpc_denorm_clamp denorm_clamp);
	void (*set_output_csc)(struct mpc *mpc,
			int opp_id,
			const uint16_t *regval,
			enum mpc_output_csc_mode ocsc_mode);
	void (*set_ocsc_default)(struct mpc *mpc,
			int opp_id,
			enum dc_color_space color_space,
			enum mpc_output_csc_mode ocsc_mode);
	void (*set_output_gamma)(
			struct mpc *mpc,
			int mpcc_id,
			const struct pwl_params *params);
	void (*power_on_mpc_mem_pwr)(
			struct mpc *mpc,
			int mpcc_id,
			bool power_on);
	void (*set_dwb_mux)(
			struct mpc *mpc,
			int dwb_id,
			int mpcc_id);
	void (*disable_dwb_mux)(
		struct mpc *mpc,
		int dwb_id);
	bool (*is_dwb_idle)(
		struct mpc *mpc,
		int dwb_id);
	void (*set_out_rate_control)(
		struct mpc *mpc,
		int opp_id,
		bool enable,
		bool rate_2x_mode,
		struct mpc_dwb_flow_control *flow_control);
	void (*set_gamut_remap)(
			struct mpc *mpc,
			int mpcc_id,
			const struct mpc_grph_gamut_adjustment *adjust);
	bool (*program_1dlut)(
			struct mpc *mpc,
			const struct pwl_params *params,
			uint32_t rmu_idx);
	bool (*program_shaper)(
			struct mpc *mpc,
			const struct pwl_params *params,
			uint32_t rmu_idx);
	uint32_t (*acquire_rmu)(struct mpc *mpc, int mpcc_id, int rmu_idx);
	bool (*program_3dlut)(
			struct mpc *mpc,
			const struct tetrahedral_params *params,
			int rmu_idx);
	int (*release_rmu)(struct mpc *mpc, int mpcc_id);
	unsigned int (*get_mpc_out_mux)(
			struct mpc *mpc,
			int opp_id);
	void (*set_bg_color)(struct mpc *mpc,
			struct tg_color *bg_color,
			int mpcc_id);
	void (*set_mpc_mem_lp_mode)(struct mpc *mpc);
};
#endif
