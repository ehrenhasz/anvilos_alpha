#ifndef __DC_DWBC_H__
#define __DC_DWBC_H__
#include "dal_types.h"
#include "dc_hw_types.h"
#define DWB_SW_V2	1
#define DWB_MCIF_BUF_COUNT 4
struct mcif_wb;
enum dwb_sw_version {
	dwb_ver_1_0 = 1,
	dwb_ver_2_0 = 2,
};
enum dwb_source {
	dwb_src_scl = 0,	 
	dwb_src_blnd,		 
	dwb_src_fmt,		 
	dwb_src_otg0 = 0x100,	 
	dwb_src_otg1,		 
	dwb_src_otg2,		 
	dwb_src_otg3,		 
};
enum dwb_pipe {
	dwb_pipe0 = 0,
	dwb_pipe1,
	dwb_pipe_max_num,
};
enum dwb_frame_capture_enable {
	DWB_FRAME_CAPTURE_DISABLE = 0,
	DWB_FRAME_CAPTURE_ENABLE = 1,
};
enum wbscl_coef_filter_type_sel {
	WBSCL_COEF_LUMA_VERT_FILTER = 0,
	WBSCL_COEF_CHROMA_VERT_FILTER = 1,
	WBSCL_COEF_LUMA_HORZ_FILTER = 2,
	WBSCL_COEF_CHROMA_HORZ_FILTER = 3
};
enum dwb_boundary_mode {
	DWBSCL_BOUNDARY_MODE_EDGE  = 0,
	DWBSCL_BOUNDARY_MODE_BLACK = 1
};
enum dwb_output_csc_mode {
	DWB_OUTPUT_CSC_DISABLE = 0,
	DWB_OUTPUT_CSC_COEF_A = 1,
	DWB_OUTPUT_CSC_COEF_B = 2
};
enum dwb_ogam_lut_mode {
	DWB_OGAM_MODE_BYPASS,
	DWB_OGAM_RAMA_LUT,
	DWB_OGAM_RAMB_LUT
};
enum dwb_color_volume {
	DWB_SRGB_BT709 = 0,	 
	DWB_PQ = 1,	 
	DWB_HLG = 2,	 
};
enum dwb_color_space {
	DWB_SRGB = 0,	 
	DWB_BT709 = 1,	 
	DWB_BT2020 = 2,	 
};
struct dwb_efc_hdr_metadata {
	unsigned int	chromaticity_green_x;
	unsigned int	chromaticity_green_y;
	unsigned int	chromaticity_blue_x;
	unsigned int	chromaticity_blue_y;
	unsigned int	chromaticity_red_x;
	unsigned int	chromaticity_red_y;
	unsigned int	chromaticity_white_point_x;
	unsigned int	chromaticity_white_point_y;
	unsigned int	min_luminance;
	unsigned int	max_luminance;
	unsigned int	maximum_content_light_level;
	unsigned int	maximum_frame_average_light_level;
};
struct dwb_efc_display_settings {
	unsigned int	inputColorVolume;
	unsigned int	inputColorSpace;
	unsigned int	inputBitDepthMinus8;
	struct dwb_efc_hdr_metadata	hdr_metadata;
	unsigned int	dwbOutputBlack;	 
};
struct dwb_warmup_params {
	bool	warmup_en;	 
	bool	warmup_mode;	 
	bool	warmup_depth;	 
	int	warmup_data;	 
	int	warmup_width;	 
	int	warmup_height;	 
};
struct dwb_caps {
	enum dce_version hw_version;	 
	enum dwb_sw_version sw_version;	 
	unsigned int	reserved[6];	 
	unsigned int	adapter_id;
	unsigned int	num_pipes;	 
	struct {
		unsigned int support_dwb	:1;
		unsigned int support_ogam	:1;
		unsigned int support_wbscl	:1;
		unsigned int support_ocsc	:1;
		unsigned int support_stereo :1;
	} caps;
	unsigned int	 reserved2[9];	 
};
struct dwbc {
	const struct dwbc_funcs *funcs;
	struct dc_context *ctx;
	int inst;
	struct mcif_wb *mcif;
	bool status;
	int inputSrcSelect;
	bool dwb_output_black;
	enum dc_transfer_func_predefined tf;
	enum dc_color_space output_color_space;
	bool dwb_is_efc_transition;
	bool dwb_is_drc;
	int wb_src_plane_inst; 
	uint32_t mask_id;
    int otg_inst;
    bool mvc_cfg;
};
struct dwbc_funcs {
	bool (*get_caps)(
		struct dwbc *dwbc,
		struct dwb_caps *caps);
	bool (*enable)(
		struct dwbc *dwbc,
		struct dc_dwb_params *params);
	bool (*disable)(struct dwbc *dwbc);
	bool (*update)(
		struct dwbc *dwbc,
		struct dc_dwb_params *params);
	bool (*is_enabled)(
		struct dwbc *dwbc);
	void (*set_stereo)(
		struct dwbc *dwbc,
		struct dwb_stereo_params *stereo_params);
	void (*set_new_content)(
		struct dwbc *dwbc,
		bool is_new_content);
	void (*set_warmup)(
		struct dwbc *dwbc,
		struct dwb_warmup_params *warmup_params);
#if defined(CONFIG_DRM_AMD_DC_FP)
	void (*dwb_program_output_csc)(
		struct dwbc *dwbc,
		enum dc_color_space color_space,
		enum dwb_output_csc_mode mode);
	bool (*dwb_ogam_set_output_transfer_func)(
		struct dwbc *dwbc,
		const struct dc_transfer_func *in_transfer_func_dwb_ogam);
	bool (*dwb_ogam_set_input_transfer_func)(
		struct dwbc *dwbc,
		const struct dc_transfer_func *in_transfer_func_dwb_ogam);
#endif
	bool (*get_dwb_status)(
		struct dwbc *dwbc);
	void (*dwb_set_scaler)(
		struct dwbc *dwbc,
		struct dc_dwb_params *params);
};
#endif
