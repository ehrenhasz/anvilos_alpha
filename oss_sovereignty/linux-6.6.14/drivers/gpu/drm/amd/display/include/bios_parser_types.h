 

#ifndef __DAL_BIOS_PARSER_TYPES_H__

#define __DAL_BIOS_PARSER_TYPES_H__

#include "dm_services.h"
#include "include/signal_types.h"
#include "include/grph_object_ctrl_defs.h"
#include "include/gpio_types.h"
#include "include/link_service_types.h"

 
enum as_signal_type {
	AS_SIGNAL_TYPE_NONE = 0L,  
	AS_SIGNAL_TYPE_DVI,
	AS_SIGNAL_TYPE_HDMI,
	AS_SIGNAL_TYPE_LVDS,
	AS_SIGNAL_TYPE_DISPLAY_PORT,
	AS_SIGNAL_TYPE_GPU_PLL,
	AS_SIGNAL_TYPE_XGMI,
	AS_SIGNAL_TYPE_UNKNOWN
};

enum bp_result {
	BP_RESULT_OK = 0,  
	BP_RESULT_BADINPUT,  
	BP_RESULT_BADBIOSTABLE,  
	BP_RESULT_UNSUPPORTED,  
	BP_RESULT_NORECORD,  
	BP_RESULT_FAILURE
};

enum bp_encoder_control_action {
	 
	ENCODER_CONTROL_DISABLE = 0,
	ENCODER_CONTROL_ENABLE,
	ENCODER_CONTROL_SETUP,
	ENCODER_CONTROL_INIT
};

enum bp_transmitter_control_action {
	 
	TRANSMITTER_CONTROL_DISABLE = 0,
	TRANSMITTER_CONTROL_ENABLE,
	TRANSMITTER_CONTROL_BACKLIGHT_OFF,
	TRANSMITTER_CONTROL_BACKLIGHT_ON,
	TRANSMITTER_CONTROL_BACKLIGHT_BRIGHTNESS,
	TRANSMITTER_CONTROL_LCD_SETF_TEST_START,
	TRANSMITTER_CONTROL_LCD_SELF_TEST_STOP,
	TRANSMITTER_CONTROL_INIT,
	TRANSMITTER_CONTROL_DEACTIVATE,
	TRANSMITTER_CONTROL_ACTIAVATE,
	TRANSMITTER_CONTROL_SETUP,
	TRANSMITTER_CONTROL_SET_VOLTAGE_AND_PREEMPASIS,
	 
	TRANSMITTER_CONTROL_POWER_ON,
	 
	TRANSMITTER_CONTROL_POWER_OFF
};

enum bp_external_encoder_control_action {
	EXTERNAL_ENCODER_CONTROL_DISABLE = 0,
	EXTERNAL_ENCODER_CONTROL_ENABLE = 1,
	EXTERNAL_ENCODER_CONTROL_INIT = 0x7,
	EXTERNAL_ENCODER_CONTROL_SETUP = 0xf,
	EXTERNAL_ENCODER_CONTROL_UNBLANK = 0x10,
	EXTERNAL_ENCODER_CONTROL_BLANK = 0x11,
};

enum bp_pipe_control_action {
	ASIC_PIPE_DISABLE = 0,
	ASIC_PIPE_ENABLE,
	ASIC_PIPE_INIT
};

enum bp_lvtma_control_action {
	LVTMA_CONTROL_LCD_BLOFF = 2,
	LVTMA_CONTROL_LCD_BLON = 3,
	LVTMA_CONTROL_POWER_ON = 12,
	LVTMA_CONTROL_POWER_OFF = 13
};

struct bp_encoder_control {
	enum bp_encoder_control_action action;
	enum engine_id engine_id;
	enum transmitter transmitter;
	enum signal_type signal;
	enum dc_lane_count lanes_number;
	enum dc_color_depth color_depth;
	bool enable_dp_audio;
	uint32_t pixel_clock;  
};

struct bp_external_encoder_control {
	enum bp_external_encoder_control_action action;
	enum engine_id engine_id;
	enum dc_link_rate link_rate;
	enum dc_lane_count lanes_number;
	enum signal_type signal;
	enum dc_color_depth color_depth;
	bool coherent;
	struct graphics_object_id encoder_id;
	struct graphics_object_id connector_obj_id;
	uint32_t pixel_clock;  
};

struct bp_crtc_source_select {
	enum engine_id engine_id;
	enum controller_id controller_id;
	 
	enum signal_type signal;
	 
	enum signal_type sink_signal;
	enum display_output_bit_depth display_output_bit_depth;
	bool enable_dp_audio;
};

struct bp_transmitter_control {
	enum bp_transmitter_control_action action;
	enum engine_id engine_id;
	enum transmitter transmitter;  
	enum dc_lane_count lanes_number;
	enum clock_source_id pll_id;  
	enum signal_type signal;
	enum dc_color_depth color_depth;  
	enum hpd_source_id hpd_sel;  
	enum tx_ffe_id txffe_sel;  
	enum engine_id hpo_engine_id;  
	struct graphics_object_id connector_obj_id;
	 
	uint32_t pixel_clock;
	uint32_t lane_select;
	uint32_t lane_settings;
	bool coherent;
	bool multi_path;
	bool single_pll_mode;
};

struct bp_hw_crtc_timing_parameters {
	enum controller_id controller_id;
	 
	uint32_t h_total;
	uint32_t h_addressable;
	uint32_t h_overscan_left;
	uint32_t h_overscan_right;
	uint32_t h_sync_start;
	uint32_t h_sync_width;

	 
	uint32_t v_total;
	uint32_t v_addressable;
	uint32_t v_overscan_top;
	uint32_t v_overscan_bottom;
	uint32_t v_sync_start;
	uint32_t v_sync_width;

	struct timing_flags {
		uint32_t INTERLACE:1;
		uint32_t PIXEL_REPETITION:4;
		uint32_t HSYNC_POSITIVE_POLARITY:1;
		uint32_t VSYNC_POSITIVE_POLARITY:1;
		uint32_t HORZ_COUNT_BY_TWO:1;
	} flags;
};

struct bp_adjust_pixel_clock_parameters {
	 
	enum signal_type signal_type;
	 
	struct graphics_object_id encoder_object_id;
	 
	uint32_t pixel_clock;
	 
	uint32_t adjusted_pixel_clock;
	 
	uint32_t reference_divider;
	 
	uint32_t pixel_clock_post_divider;
	 
	bool ss_enable;
};

struct bp_pixel_clock_parameters {
	enum controller_id controller_id;  
	enum clock_source_id pll_id;  
	 
	enum signal_type signal_type;
	 
	uint32_t target_pixel_clock_100hz;
	 
	uint32_t reference_divider;
	 
	uint32_t feedback_divider;
	 
	uint32_t fractional_feedback_divider;
	 
	uint32_t pixel_clock_post_divider;
	struct graphics_object_id encoder_object_id;  
	 
	uint32_t dfs_bypass_display_clock;
	 
	enum transmitter_color_depth color_depth;

	struct program_pixel_clock_flags {
		uint32_t FORCE_PROGRAMMING_OF_PLL:1;
		 
		uint32_t USE_E_CLOCK_AS_SOURCE_FOR_D_CLOCK:1;
		 
		uint32_t SET_EXTERNAL_REF_DIV_SRC:1;
		 
		uint32_t SET_DISPCLK_DFS_BYPASS:1;
		 
		uint32_t PROGRAM_PHY_PLL_ONLY:1;
		 
		uint32_t SUPPORT_YUV_420:1;
		 
		uint32_t SET_XTALIN_REF_SRC:1;
		 
		uint32_t SET_GENLOCK_REF_DIV_SRC:1;
	} flags;
};

enum bp_dce_clock_type {
	DCECLOCK_TYPE_DISPLAY_CLOCK = 0,
	DCECLOCK_TYPE_DPREFCLK      = 1
};

 
struct bp_set_dce_clock_parameters {
	enum clock_source_id pll_id;  
	 
	uint32_t target_clock_frequency;
	 
	enum bp_dce_clock_type clock_type;

	struct set_dce_clock_flags {
		uint32_t USE_GENERICA_AS_SOURCE_FOR_DPREFCLK:1;
		 
		uint32_t USE_XTALIN_AS_SOURCE_FOR_DPREFCLK:1;
		 
		uint32_t USE_PCIE_AS_SOURCE_FOR_DPREFCLK:1;
		 
		uint32_t USE_GENLOCK_AS_SOURCE_FOR_DPREFCLK:1;
	} flags;
};

struct spread_spectrum_flags {
	 
	uint32_t CENTER_SPREAD:1;
	 
	uint32_t EXTERNAL_SS:1;
	 
	uint32_t DS_TYPE:1;
};

struct bp_spread_spectrum_parameters {
	enum clock_source_id pll_id;
	uint32_t percentage;
	uint32_t ds_frac_amount;

	union {
		struct {
			uint32_t step;
			uint32_t delay;
			uint32_t range;  
		} ver1;
		struct {
			uint32_t feedback_amount;
			uint32_t nfrac_amount;
			uint32_t ds_frac_size;
		} ds;
	};

	struct spread_spectrum_flags flags;
};

struct bp_disp_connector_caps_info {
	uint32_t INTERNAL_DISPLAY    : 1;
	uint32_t INTERNAL_DISPLAY_BL : 1;
};

struct bp_encoder_cap_info {
	uint32_t DP_HBR2_CAP:1;
	uint32_t DP_HBR2_EN:1;
	uint32_t DP_HBR3_EN:1;
	uint32_t HDMI_6GB_EN:1;
	uint32_t IS_DP2_CAPABLE:1;
	uint32_t DP_UHBR10_EN:1;
	uint32_t DP_UHBR13_5_EN:1;
	uint32_t DP_UHBR20_EN:1;
	uint32_t DP_IS_USB_C:1;
	uint32_t RESERVED:27;
};

struct bp_soc_bb_info {
	uint32_t dram_clock_change_latency_100ns;
	uint32_t dram_sr_exit_latency_100ns;
	uint32_t dram_sr_enter_exit_latency_100ns;
};

struct bp_connector_speed_cap_info {
	uint32_t DP_HBR2_EN:1;
	uint32_t DP_HBR3_EN:1;
	uint32_t HDMI_6GB_EN:1;
	uint32_t DP_UHBR10_EN:1;
	uint32_t DP_UHBR13_5_EN:1;
	uint32_t DP_UHBR20_EN:1;
	uint32_t DP_IS_USB_C:1;
	uint32_t RESERVED:28;
};

#endif  
