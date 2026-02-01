 

#ifndef __DAL_LINK_SERVICE_TYPES_H__
#define __DAL_LINK_SERVICE_TYPES_H__

#include "grph_object_id.h"
#include "dal_types.h"
#include "irq_types.h"

 
struct ddc;
struct irq_manager;

enum dp_power_state {
	DP_POWER_STATE_D0 = 1,
	DP_POWER_STATE_D3
};

enum edp_revision {
	 
	EDP_REVISION_11 = 0x00,
	 
	EDP_REVISION_12 = 0x01,
	 
	EDP_REVISION_13 = 0x02
};

enum {
	LINK_RATE_REF_FREQ_IN_KHZ = 27000,  
	BITS_PER_DP_BYTE = 10,
	DATA_EFFICIENCY_8b_10b_x10000 = 8000,  
	DATA_EFFICIENCY_8b_10b_FEC_EFFICIENCY_x100 = 97,  
	DATA_EFFICIENCY_128b_132b_x10000 = 9641,  
};

enum lttpr_mode {
	LTTPR_MODE_UNKNOWN,
	LTTPR_MODE_NON_LTTPR,
	LTTPR_MODE_TRANSPARENT,
	LTTPR_MODE_NON_TRANSPARENT,
};

struct link_training_settings {
	struct dc_link_settings link_settings;

	 
	enum dc_voltage_swing *voltage_swing;
	enum dc_pre_emphasis *pre_emphasis;
	enum dc_post_cursor2 *post_cursor2;
	bool should_set_fec_ready;
	 
	union dc_dp_ffe_preset *ffe_preset;

	uint16_t cr_pattern_time;
	uint16_t eq_pattern_time;
	uint16_t cds_pattern_time;
	enum dc_dp_training_pattern pattern_for_cr;
	enum dc_dp_training_pattern pattern_for_eq;
	enum dc_dp_training_pattern pattern_for_cds;

	uint32_t eq_wait_time_limit;
	uint8_t eq_loop_count_limit;
	uint32_t cds_wait_time_limit;

	bool enhanced_framing;
	enum lttpr_mode lttpr_mode;

	 
	bool disallow_per_lane_settings;
	 
	bool always_match_dpcd_with_hw_lane_settings;

	 
	 
	struct dc_lane_settings hw_lane_settings[LANE_COUNT_DP_MAX];
	union dpcd_training_lane dpcd_lane_settings[LANE_COUNT_DP_MAX];
};

 
 
enum dp_test_pattern {
	 
	DP_TEST_PATTERN_VIDEO_MODE = 0,

	 
	DP_TEST_PATTERN_PHY_PATTERN_BEGIN,
	DP_TEST_PATTERN_D102 = DP_TEST_PATTERN_PHY_PATTERN_BEGIN,
	DP_TEST_PATTERN_SYMBOL_ERROR,
	DP_TEST_PATTERN_PRBS7,
	DP_TEST_PATTERN_80BIT_CUSTOM,
	DP_TEST_PATTERN_CP2520_1,
	DP_TEST_PATTERN_CP2520_2,
	DP_TEST_PATTERN_HBR2_COMPLIANCE_EYE = DP_TEST_PATTERN_CP2520_2,
	DP_TEST_PATTERN_CP2520_3,
	DP_TEST_PATTERN_128b_132b_TPS1,
	DP_TEST_PATTERN_128b_132b_TPS2,
	DP_TEST_PATTERN_PRBS9,
	DP_TEST_PATTERN_PRBS11,
	DP_TEST_PATTERN_PRBS15,
	DP_TEST_PATTERN_PRBS23,
	DP_TEST_PATTERN_PRBS31,
	DP_TEST_PATTERN_264BIT_CUSTOM,
	DP_TEST_PATTERN_SQUARE_BEGIN,
	DP_TEST_PATTERN_SQUARE = DP_TEST_PATTERN_SQUARE_BEGIN,
	DP_TEST_PATTERN_SQUARE_PRESHOOT_DISABLED,
	DP_TEST_PATTERN_SQUARE_DEEMPHASIS_DISABLED,
	DP_TEST_PATTERN_SQUARE_PRESHOOT_DEEMPHASIS_DISABLED,
	DP_TEST_PATTERN_SQUARE_END = DP_TEST_PATTERN_SQUARE_PRESHOOT_DEEMPHASIS_DISABLED,

	 
	DP_TEST_PATTERN_TRAINING_PATTERN1,
	DP_TEST_PATTERN_TRAINING_PATTERN2,
	DP_TEST_PATTERN_TRAINING_PATTERN3,
	DP_TEST_PATTERN_TRAINING_PATTERN4,
	DP_TEST_PATTERN_128b_132b_TPS1_TRAINING_MODE,
	DP_TEST_PATTERN_128b_132b_TPS2_TRAINING_MODE,
	DP_TEST_PATTERN_PHY_PATTERN_END = DP_TEST_PATTERN_128b_132b_TPS2_TRAINING_MODE,

	 
	DP_TEST_PATTERN_COLOR_SQUARES,
	DP_TEST_PATTERN_COLOR_SQUARES_CEA,
	DP_TEST_PATTERN_VERTICAL_BARS,
	DP_TEST_PATTERN_HORIZONTAL_BARS,
	DP_TEST_PATTERN_COLOR_RAMP,

	 
	DP_TEST_PATTERN_AUDIO_OPERATOR_DEFINED,
	DP_TEST_PATTERN_AUDIO_SAWTOOTH,

	DP_TEST_PATTERN_UNSUPPORTED
};

enum dp_test_pattern_color_space {
	DP_TEST_PATTERN_COLOR_SPACE_RGB,
	DP_TEST_PATTERN_COLOR_SPACE_YCBCR601,
	DP_TEST_PATTERN_COLOR_SPACE_YCBCR709,
	DP_TEST_PATTERN_COLOR_SPACE_UNDEFINED
};

enum dp_panel_mode {
	 
	DP_PANEL_MODE_DEFAULT,
	 
	DP_PANEL_MODE_EDP,
	 
	DP_PANEL_MODE_SPECIAL
};

enum dpcd_source_sequence {
	DPCD_SOURCE_SEQ_AFTER_CONNECT_DIG_FE_OTG = 1,  
	DPCD_SOURCE_SEQ_AFTER_DP_STREAM_ATTR,          
	DPCD_SOURCE_SEQ_AFTER_UPDATE_INFO_FRAME,       
	DPCD_SOURCE_SEQ_AFTER_CONNECT_DIG_FE_BE,       
	DPCD_SOURCE_SEQ_AFTER_ENABLE_LINK_PHY,         
	DPCD_SOURCE_SEQ_AFTER_SET_SOURCE_PATTERN,      
	DPCD_SOURCE_SEQ_AFTER_ENABLE_AUDIO_STREAM,     
	DPCD_SOURCE_SEQ_AFTER_ENABLE_DP_VID_STREAM,    
	DPCD_SOURCE_SEQ_AFTER_DISABLE_DP_VID_STREAM,   
	DPCD_SOURCE_SEQ_AFTER_FIFO_STEER_RESET,        
	DPCD_SOURCE_SEQ_AFTER_DISABLE_AUDIO_STREAM,    
	DPCD_SOURCE_SEQ_AFTER_DISABLE_LINK_PHY,        
	DPCD_SOURCE_SEQ_AFTER_DISCONNECT_DIG_FE_BE,    
};

 
union dpcd_training_lane_set {
	struct {
#if defined(LITTLEENDIAN_CPU)
		uint8_t VOLTAGE_SWING_SET:2;
		uint8_t MAX_SWING_REACHED:1;
		uint8_t PRE_EMPHASIS_SET:2;
		uint8_t MAX_PRE_EMPHASIS_REACHED:1;
		 
		uint8_t POST_CURSOR2_SET:2;
#elif defined(BIGENDIAN_CPU)
		uint8_t POST_CURSOR2_SET:2;
		uint8_t MAX_PRE_EMPHASIS_REACHED:1;
		uint8_t PRE_EMPHASIS_SET:2;
		uint8_t MAX_SWING_REACHED:1;
		uint8_t VOLTAGE_SWING_SET:2;
#else
	#error ARCH not defined!
#endif
	} bits;

	uint8_t raw;
};


 

struct drm_dp_mst_port;

 
struct dc_dp_mst_stream_allocation {
	uint8_t vcp_id;
	 
	uint8_t slot_count;
};

 
struct dc_dp_mst_stream_allocation_table {
	 
	int stream_count;
	 
	struct dc_dp_mst_stream_allocation stream_allocations[MAX_CONTROLLER_NUM];
};

#endif  
