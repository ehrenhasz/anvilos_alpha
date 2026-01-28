#ifndef DMUB_CMD_H
#define DMUB_CMD_H
#if defined(_TEST_HARNESS) || defined(FPGA_USB4)
#include "dmub_fw_types.h"
#include "include_legacy/atomfirmware.h"
#if defined(_TEST_HARNESS)
#include <string.h>
#endif
#else
#include <asm/byteorder.h>
#include <linux/types.h>
#include <linux/string.h>
#include <linux/delay.h>
#include "atomfirmware.h"
#endif  
#define __forceinline inline
#define SET_ABM_PIPE_GRADUALLY_DISABLE           0
#define SET_ABM_PIPE_IMMEDIATELY_DISABLE         255
#define SET_ABM_PIPE_IMMEDIATE_KEEP_GAIN_DISABLE 254
#define SET_ABM_PIPE_NORMAL                      1
#define NUM_AMBI_LEVEL                  5
#define NUM_AGGR_LEVEL                  4
#define NUM_POWER_FN_SEGS               8
#define NUM_BL_CURVE_SEGS               16
#define DMUB_MAX_SUBVP_STREAMS 2
#define DMUB_MAX_FPO_STREAMS 4
#define DMUB_MAX_STREAMS 6
#define DMUB_MAX_PLANES 6
#define TRACE_BUFFER_ENTRY_OFFSET  16
#define DMUB_MAX_DIRTY_RECTS 3
#define DMUB_CMD_PSR_CONTROL_VERSION_UNKNOWN 0x0
#define DMUB_CMD_PSR_CONTROL_VERSION_1 0x1
#define DMUB_CMD_ABM_CONTROL_VERSION_UNKNOWN 0x0
#define DMUB_CMD_ABM_CONTROL_VERSION_1 0x1
#ifndef PHYSICAL_ADDRESS_LOC
#define PHYSICAL_ADDRESS_LOC union large_integer
#endif
#ifndef dmub_memcpy
#define dmub_memcpy(dest, source, bytes) memcpy((dest), (source), (bytes))
#endif
#ifndef dmub_memset
#define dmub_memset(dest, val, bytes) memset((dest), (val), (bytes))
#endif
#if defined(__cplusplus)
extern "C" {
#endif
#ifndef dmub_udelay
#define dmub_udelay(microseconds) udelay(microseconds)
#endif
#pragma pack(push, 1)
#define ABM_NUM_OF_ACE_SEGMENTS         5
union abm_flags {
	struct {
		unsigned int abm_enabled : 1;
		unsigned int disable_abm_requested : 1;
		unsigned int disable_abm_immediately : 1;
		unsigned int disable_abm_immediate_keep_gain : 1;
		unsigned int fractional_pwm : 1;
		unsigned int abm_gradual_bl_change : 1;
	} bitfields;
	unsigned int u32All;
};
struct abm_save_restore {
	union abm_flags flags;
	uint32_t pause;
	uint32_t next_ace_slope[ABM_NUM_OF_ACE_SEGMENTS];
	uint32_t next_ace_thresh[ABM_NUM_OF_ACE_SEGMENTS];
	uint32_t next_ace_offset[ABM_NUM_OF_ACE_SEGMENTS];
	uint32_t knee_threshold;
	uint32_t current_gain;
	uint16_t curr_bl_level;
	uint16_t curr_user_bl_level;
};
union dmub_addr {
	struct {
		uint32_t low_part;  
		uint32_t high_part;  
	} u;  
	uint64_t quad_part;  
};
#pragma pack(pop)
struct dmub_rect {
	uint32_t x;
	uint32_t y;
	uint32_t width;
	uint32_t height;
};
union dmub_psr_debug_flags {
	struct {
		uint32_t visual_confirm : 1;
		uint32_t force_full_frame_update : 1;
		uint32_t use_hw_lock_mgr : 1;
		uint32_t force_wakeup_by_tps3 : 1;
		uint32_t back_to_back_flip : 1;
	} bitfields;
	uint32_t u32All;
};
union replay_debug_flags {
	struct {
		uint32_t visual_confirm : 1;
		uint32_t skip_crc : 1;
		uint32_t force_link_power_on : 1;
		uint32_t force_phy_power_on : 1;
		uint32_t timing_resync_disabled : 1;
		uint32_t skip_crtc_disabled : 1;
		uint32_t force_defer_one_frame_update : 1;
		uint32_t disable_delay_alpm_on : 1;
		uint32_t disable_desync_error_check : 1;
		uint32_t disable_dmub_save_restore : 1;
		uint32_t reserved : 22;
	} bitfields;
	uint32_t u32All;
};
union replay_hw_flags {
	struct {
		uint32_t allow_alpm_fw_standby_mode : 1;
		uint32_t dsc_enable_status : 1;
		uint32_t fec_enable_status : 1;
		uint32_t smu_optimizations_en : 1;
		uint32_t otg_powered_down : 1;
		uint32_t phy_power_state : 1;
		uint32_t link_power_state : 1;
		uint32_t force_wakeup_by_tps3 : 1;
	} bitfields;
	uint32_t u32All;
};
struct dmub_feature_caps {
	uint8_t psr;
	uint8_t fw_assisted_mclk_switch;
	uint8_t reserved[4];
	uint8_t subvp_psr_support;
	uint8_t gecc_enable;
};
struct dmub_visual_confirm_color {
	uint16_t color_r_cr;
	uint16_t color_g_y;
	uint16_t color_b_cb;
	uint16_t panel_inst;
};
#if defined(__cplusplus)
}
#endif
#pragma pack(push, 1)
#define DMUB_FW_META_MAGIC 0x444D5542
#define DMUB_FW_META_OFFSET 0x24
struct dmub_fw_meta_info {
	uint32_t magic_value;  
	uint32_t fw_region_size;  
	uint32_t trace_buffer_size;  
	uint32_t fw_version;  
	uint8_t dal_fw;  
	uint8_t reserved[3];  
};
union dmub_fw_meta {
	struct dmub_fw_meta_info info;  
	uint8_t reserved[64];  
};
#pragma pack(pop)
typedef uint32_t dmub_trace_code_t;
struct dmcub_trace_buf_entry {
	dmub_trace_code_t trace_code;  
	uint32_t tick_count;  
	uint32_t param0;  
	uint32_t param1;  
};
union dmub_fw_boot_status {
	struct {
		uint32_t dal_fw : 1;  
		uint32_t mailbox_rdy : 1;  
		uint32_t optimized_init_done : 1;  
		uint32_t restore_required : 1;  
		uint32_t defer_load : 1;  
		uint32_t fams_enabled : 1;  
		uint32_t detection_required: 1;  
		uint32_t hw_power_init_done: 1;  
	} bits;  
	uint32_t all;  
};
enum dmub_fw_boot_status_bit {
	DMUB_FW_BOOT_STATUS_BIT_DAL_FIRMWARE = (1 << 0),  
	DMUB_FW_BOOT_STATUS_BIT_MAILBOX_READY = (1 << 1),  
	DMUB_FW_BOOT_STATUS_BIT_OPTIMIZED_INIT_DONE = (1 << 2),  
	DMUB_FW_BOOT_STATUS_BIT_RESTORE_REQUIRED = (1 << 3),  
	DMUB_FW_BOOT_STATUS_BIT_DEFERRED_LOADED = (1 << 4),  
	DMUB_FW_BOOT_STATUS_BIT_FAMS_ENABLED = (1 << 5),  
	DMUB_FW_BOOT_STATUS_BIT_DETECTION_REQUIRED = (1 << 6),  
	DMUB_FW_BOOT_STATUS_BIT_HW_POWER_INIT_DONE = (1 << 7),  
};
union dmub_lvtma_status {
	struct {
		uint32_t psp_ok : 1;
		uint32_t edp_on : 1;
		uint32_t reserved : 30;
	} bits;
	uint32_t all;
};
enum dmub_lvtma_status_bit {
	DMUB_LVTMA_STATUS_BIT_PSP_OK = (1 << 0),
	DMUB_LVTMA_STATUS_BIT_EDP_ON = (1 << 1),
};
enum dmub_ips_disable_type {
	DMUB_IPS_DISABLE_IPS1 = 1,
	DMUB_IPS_DISABLE_IPS2 = 2,
	DMUB_IPS_DISABLE_IPS2_Z10 = 3,
};
union dmub_fw_boot_options {
	struct {
		uint32_t pemu_env : 1;  
		uint32_t fpga_env : 1;  
		uint32_t optimized_init : 1;  
		uint32_t skip_phy_access : 1;  
		uint32_t disable_clk_gate: 1;  
		uint32_t skip_phy_init_panel_sequence: 1;  
		uint32_t z10_disable: 1;  
		uint32_t enable_dpia: 1;  
		uint32_t invalid_vbios_data: 1;  
		uint32_t dpia_supported: 1;  
		uint32_t sel_mux_phy_c_d_phy_f_g: 1;  
		uint32_t power_optimization: 1;
		uint32_t diag_env: 1;  
		uint32_t gpint_scratch8: 1;  
		uint32_t usb4_cm_version: 1;  
		uint32_t dpia_hpd_int_enable_supported: 1;  
		uint32_t usb4_dpia_bw_alloc_supported: 1;  
		uint32_t disable_clk_ds: 1;  
		uint32_t disable_timeout_recovery : 1;  
		uint32_t ips_pg_disable: 1;  
		uint32_t ips_disable: 2;  
		uint32_t reserved : 10;  
	} bits;  
	uint32_t all;  
};
enum dmub_fw_boot_options_bit {
	DMUB_FW_BOOT_OPTION_BIT_PEMU_ENV = (1 << 0),  
	DMUB_FW_BOOT_OPTION_BIT_FPGA_ENV = (1 << 1),  
	DMUB_FW_BOOT_OPTION_BIT_OPTIMIZED_INIT_DONE = (1 << 2),  
};
enum dmub_cmd_vbios_type {
	DMUB_CMD__VBIOS_DIGX_ENCODER_CONTROL = 0,
	DMUB_CMD__VBIOS_DIG1_TRANSMITTER_CONTROL = 1,
	DMUB_CMD__VBIOS_SET_PIXEL_CLOCK = 2,
	DMUB_CMD__VBIOS_ENABLE_DISP_POWER_GATING = 3,
	DMUB_CMD__VBIOS_LVTMA_CONTROL = 15,
	DMUB_CMD__VBIOS_TRANSMITTER_QUERY_DP_ALT  = 26,
	DMUB_CMD__VBIOS_DOMAIN_CONTROL = 28,
};
#define DMUB_GPINT_DATA_PARAM_MASK 0xFFFF
#define DMUB_GPINT_DATA_PARAM_SHIFT 0
#define DMUB_GPINT_DATA_COMMAND_CODE_MASK 0xFFF
#define DMUB_GPINT_DATA_COMMAND_CODE_SHIFT 16
#define DMUB_GPINT_DATA_STATUS_MASK 0xF
#define DMUB_GPINT_DATA_STATUS_SHIFT 28
#define DMUB_GPINT__STOP_FW_RESPONSE 0xDEADDEAD
union dmub_gpint_data_register {
	struct {
		uint32_t param : 16;  
		uint32_t command_code : 12;  
		uint32_t status : 4;  
	} bits;  
	uint32_t all;  
};
enum dmub_gpint_command {
	DMUB_GPINT__INVALID_COMMAND = 0,
	DMUB_GPINT__GET_FW_VERSION = 1,
	DMUB_GPINT__STOP_FW = 2,
	DMUB_GPINT__GET_PSR_STATE = 7,
	DMUB_GPINT__IDLE_OPT_NOTIFY_STREAM_MASK = 8,
	DMUB_GPINT__PSR_RESIDENCY = 9,
	DMUB_GPINT__GET_REPLAY_STATE = 13,
	DMUB_GPINT__REPLAY_RESIDENCY = 14,
	DMUB_GPINT__NOTIFY_DETECTION_DONE = 12,
	DMUB_GPINT__UPDATE_TRACE_BUFFER_MASK = 101,
	DMUB_GPINT__SET_TRACE_BUFFER_MASK_WORD0 = 102,
	DMUB_GPINT__SET_TRACE_BUFFER_MASK_WORD1 = 103,
};
union dmub_inbox0_cmd_common {
	struct {
		uint32_t command_code: 8;  
		uint32_t param: 24;  
	} bits;
	uint32_t all;
};
union dmub_inbox0_cmd_lock_hw {
	struct {
		uint32_t command_code: 8;
		uint32_t hw_lock_client: 2;
		uint32_t otg_inst: 3;
		uint32_t opp_inst: 3;
		uint32_t dig_inst: 3;
		uint32_t lock_pipe: 1;
		uint32_t lock_cursor: 1;
		uint32_t lock_dig: 1;
		uint32_t triple_buffer_lock: 1;
		uint32_t lock: 1;				 
		uint32_t should_release: 1;		 
		uint32_t reserved: 7; 			 
	} bits;
	uint32_t all;
};
union dmub_inbox0_data_register {
	union dmub_inbox0_cmd_common inbox0_cmd_common;
	union dmub_inbox0_cmd_lock_hw inbox0_cmd_lock_hw;
};
enum dmub_inbox0_command {
	DMUB_INBOX0_CMD__INVALID_COMMAND = 0,
	DMUB_INBOX0_CMD__HW_LOCK = 1,
};
#define DMUB_RB_CMD_SIZE 64
#define DMUB_RB_MAX_ENTRY 128
#define DMUB_RB_SIZE (DMUB_RB_CMD_SIZE * DMUB_RB_MAX_ENTRY)
#define REG_SET_MASK 0xFFFF
enum dmub_cmd_type {
	DMUB_CMD__NULL = 0,
	DMUB_CMD__REG_SEQ_READ_MODIFY_WRITE = 1,
	DMUB_CMD__REG_SEQ_FIELD_UPDATE_SEQ = 2,
	DMUB_CMD__REG_SEQ_BURST_WRITE = 3,
	DMUB_CMD__REG_REG_WAIT = 4,
	DMUB_CMD__PLAT_54186_WA = 5,
	DMUB_CMD__QUERY_FEATURE_CAPS = 6,
	DMUB_CMD__GET_VISUAL_CONFIRM_COLOR = 8,
	DMUB_CMD__PSR = 64,
	DMUB_CMD__MALL = 65,
	DMUB_CMD__ABM = 66,
	DMUB_CMD__UPDATE_DIRTY_RECT = 67,
	DMUB_CMD__UPDATE_CURSOR_INFO = 68,
	DMUB_CMD__HW_LOCK = 69,
	DMUB_CMD__DP_AUX_ACCESS = 70,
	DMUB_CMD__OUTBOX1_ENABLE = 71,
	DMUB_CMD__IDLE_OPT = 72,
	DMUB_CMD__CLK_MGR = 73,
	DMUB_CMD__PANEL_CNTL = 74,
	DMUB_CMD__CAB_FOR_SS = 75,
	DMUB_CMD__FW_ASSISTED_MCLK_SWITCH = 76,
	DMUB_CMD__DPIA = 77,
	DMUB_CMD__EDID_CEA = 79,
	DMUB_CMD_GET_USBC_CABLE_ID = 81,
	DMUB_CMD__QUERY_HPD_STATE = 82,
	DMUB_CMD__REPLAY = 83,
	DMUB_CMD__SECURE_DISPLAY = 85,
	DMUB_CMD__DPIA_HPD_INT_ENABLE = 86,
	DMUB_CMD__VBIOS = 128,
};
enum dmub_out_cmd_type {
	DMUB_OUT_CMD__NULL = 0,
	DMUB_OUT_CMD__DP_AUX_REPLY = 1,
	DMUB_OUT_CMD__DP_HPD_NOTIFY = 2,
	DMUB_OUT_CMD__SET_CONFIG_REPLY = 3,
	DMUB_OUT_CMD__DPIA_NOTIFICATION = 5,
};
enum dmub_cmd_dpia_type {
	DMUB_CMD__DPIA_DIG1_DPIA_CONTROL = 0,
	DMUB_CMD__DPIA_SET_CONFIG_ACCESS = 1,
	DMUB_CMD__DPIA_MST_ALLOC_SLOTS = 2,
};
enum dmub_cmd_dpia_notification_type {
	DPIA_NOTIFY__BW_ALLOCATION = 0,
};
#pragma pack(push, 1)
struct dmub_cmd_header {
	unsigned int type : 8;  
	unsigned int sub_type : 8;  
	unsigned int ret_status : 1;  
	unsigned int multi_cmd_pending : 1;  
	unsigned int reserved0 : 6;  
	unsigned int payload_bytes : 6;   
	unsigned int reserved1 : 2;  
};
struct dmub_cmd_read_modify_write_sequence {
	uint32_t addr;  
	uint32_t modify_mask;  
	uint32_t modify_value;  
};
#define DMUB_READ_MODIFY_WRITE_SEQ__MAX 5
struct dmub_rb_cmd_read_modify_write {
	struct dmub_cmd_header header;   
	struct dmub_cmd_read_modify_write_sequence seq[DMUB_READ_MODIFY_WRITE_SEQ__MAX];
};
struct dmub_cmd_reg_field_update_sequence {
	uint32_t modify_mask;  
	uint32_t modify_value;  
};
#define DMUB_REG_FIELD_UPDATE_SEQ__MAX 7
struct dmub_rb_cmd_reg_field_update_sequence {
	struct dmub_cmd_header header;  
	uint32_t addr;  
	struct dmub_cmd_reg_field_update_sequence seq[DMUB_REG_FIELD_UPDATE_SEQ__MAX];
};
#define DMUB_BURST_WRITE_VALUES__MAX  14
struct dmub_rb_cmd_burst_write {
	struct dmub_cmd_header header;  
	uint32_t addr;  
	uint32_t write_values[DMUB_BURST_WRITE_VALUES__MAX];
};
struct dmub_rb_cmd_common {
	struct dmub_cmd_header header;  
	uint8_t cmd_buffer[DMUB_RB_CMD_SIZE - sizeof(struct dmub_cmd_header)];
};
struct dmub_cmd_reg_wait_data {
	uint32_t addr;  
	uint32_t mask;  
	uint32_t condition_field_value;  
	uint32_t time_out_us;  
};
struct dmub_rb_cmd_reg_wait {
	struct dmub_cmd_header header;  
	struct dmub_cmd_reg_wait_data reg_wait;  
};
struct dmub_cmd_PLAT_54186_wa {
	uint32_t DCSURF_SURFACE_CONTROL;  
	uint32_t DCSURF_PRIMARY_SURFACE_ADDRESS_HIGH;  
	uint32_t DCSURF_PRIMARY_SURFACE_ADDRESS;  
	uint32_t DCSURF_PRIMARY_SURFACE_ADDRESS_HIGH_C;  
	uint32_t DCSURF_PRIMARY_SURFACE_ADDRESS_C;  
	struct {
		uint8_t hubp_inst : 4;  
		uint8_t tmz_surface : 1;  
		uint8_t immediate :1;  
		uint8_t vmid : 4;  
		uint8_t grph_stereo : 1;  
		uint32_t reserved : 21;  
	} flip_params;  
	uint32_t reserved[9];  
};
struct dmub_rb_cmd_PLAT_54186_wa {
	struct dmub_cmd_header header;  
	struct dmub_cmd_PLAT_54186_wa flip;  
};
struct dmub_rb_cmd_mall {
	struct dmub_cmd_header header;  
	union dmub_addr cursor_copy_src;  
	union dmub_addr cursor_copy_dst;  
	uint32_t tmr_delay;  
	uint32_t tmr_scale;  
	uint16_t cursor_width;  
	uint16_t cursor_pitch;  
	uint16_t cursor_height;  
	uint8_t cursor_bpp;  
	uint8_t debug_bits;  
	uint8_t reserved1;  
	uint8_t reserved2;  
};
enum dmub_cmd_cab_type {
	DMUB_CMD__CAB_NO_IDLE_OPTIMIZATION = 0,
	DMUB_CMD__CAB_NO_DCN_REQ = 1,
	DMUB_CMD__CAB_DCN_SS_FIT_IN_CAB = 2,
};
struct dmub_rb_cmd_cab_for_ss {
	struct dmub_cmd_header header;
	uint8_t cab_alloc_ways;  
	uint8_t debug_bits;      
};
enum mclk_switch_mode {
	NONE = 0,
	FPO = 1,
	SUBVP = 2,
	VBLANK = 3,
};
struct dmub_cmd_fw_assisted_mclk_switch_pipe_data_v2 {
	union {
		struct {
			uint32_t pix_clk_100hz;
			uint16_t main_vblank_start;
			uint16_t main_vblank_end;
			uint16_t mall_region_lines;
			uint16_t prefetch_lines;
			uint16_t prefetch_to_mall_start_lines;
			uint16_t processing_delay_lines;
			uint16_t htotal;  
			uint16_t vtotal;
			uint8_t main_pipe_index;
			uint8_t phantom_pipe_index;
			uint8_t scale_factor_numerator;
			uint8_t scale_factor_denominator;
			uint8_t is_drr;
			uint8_t main_split_pipe_index;
			uint8_t phantom_split_pipe_index;
		} subvp_data;
		struct {
			uint32_t pix_clk_100hz;
			uint16_t vblank_start;
			uint16_t vblank_end;
			uint16_t vstartup_start;
			uint16_t vtotal;
			uint16_t htotal;
			uint8_t vblank_pipe_index;
			uint8_t padding[1];
			struct {
				uint8_t drr_in_use;
				uint8_t drr_window_size_ms;	 
				uint16_t min_vtotal_supported;	 
				uint16_t max_vtotal_supported;	 
				uint8_t use_ramping;		 
				uint8_t drr_vblank_start_margin;
			} drr_info;				 
		} vblank_data;
	} pipe_config;
	uint8_t mode;  
};
struct dmub_cmd_fw_assisted_mclk_switch_config_v2 {
	uint16_t watermark_a_cache;
	uint8_t vertical_int_margin_us;
	uint8_t pstate_allow_width_us;
	struct dmub_cmd_fw_assisted_mclk_switch_pipe_data_v2 pipe_data[DMUB_MAX_SUBVP_STREAMS];
};
struct dmub_rb_cmd_fw_assisted_mclk_switch_v2 {
	struct dmub_cmd_header header;
	struct dmub_cmd_fw_assisted_mclk_switch_config_v2 config_data;
};
enum dmub_cmd_idle_opt_type {
	DMUB_CMD__IDLE_OPT_DCN_RESTORE = 0,
	DMUB_CMD__IDLE_OPT_DCN_SAVE_INIT = 1,
	DMUB_CMD__IDLE_OPT_DCN_NOTIFY_IDLE = 2
};
struct dmub_rb_cmd_idle_opt_dcn_restore {
	struct dmub_cmd_header header;  
};
struct dmub_dcn_notify_idle_cntl_data {
	uint8_t driver_idle;
	uint8_t pad[1];
};
struct dmub_rb_cmd_idle_opt_dcn_notify_idle {
	struct dmub_cmd_header header;  
	struct dmub_dcn_notify_idle_cntl_data cntl_data;
};
struct dmub_clocks {
	uint32_t dispclk_khz;  
	uint32_t dppclk_khz;  
	uint32_t dcfclk_khz;  
	uint32_t dcfclk_deep_sleep_khz;  
};
enum dmub_cmd_clk_mgr_type {
	DMUB_CMD__CLK_MGR_NOTIFY_CLOCKS = 0,
};
struct dmub_rb_cmd_clk_mgr_notify_clocks {
	struct dmub_cmd_header header;  
	struct dmub_clocks clocks;  
};
struct dmub_cmd_digx_encoder_control_data {
	union dig_encoder_control_parameters_v1_5 dig;  
};
struct dmub_rb_cmd_digx_encoder_control {
	struct dmub_cmd_header header;   
	struct dmub_cmd_digx_encoder_control_data encoder_control;  
};
struct dmub_cmd_set_pixel_clock_data {
	struct set_pixel_clock_parameter_v1_7 clk;  
};
struct dmub_rb_cmd_set_pixel_clock {
	struct dmub_cmd_header header;  
	struct dmub_cmd_set_pixel_clock_data pixel_clock;  
};
struct dmub_cmd_enable_disp_power_gating_data {
	struct enable_disp_power_gating_parameters_v2_1 pwr;  
};
struct dmub_rb_cmd_enable_disp_power_gating {
	struct dmub_cmd_header header;  
	struct dmub_cmd_enable_disp_power_gating_data power_gating;   
};
struct dmub_dig_transmitter_control_data_v1_7 {
	uint8_t phyid;  
	uint8_t action;  
	union {
		uint8_t digmode;  
		uint8_t dplaneset;  
	} mode_laneset;
	uint8_t lanenum;  
	union {
		uint32_t symclk_10khz;  
	} symclk_units;
	uint8_t hpdsel;  
	uint8_t digfe_sel;  
	uint8_t connobj_id;  
	uint8_t HPO_instance;  
	uint8_t reserved1;  
	uint8_t reserved2[3];  
	uint32_t reserved3[11];  
};
union dmub_cmd_dig1_transmitter_control_data {
	struct dig_transmitter_control_parameters_v1_6 dig;  
	struct dmub_dig_transmitter_control_data_v1_7 dig_v1_7;   
};
struct dmub_rb_cmd_dig1_transmitter_control {
	struct dmub_cmd_header header;  
	union dmub_cmd_dig1_transmitter_control_data transmitter_control;  
};
struct dmub_rb_cmd_domain_control_data {
	uint8_t inst : 6;  
	uint8_t power_gate : 1;  
	uint8_t reserved[3];  
};
struct dmub_rb_cmd_domain_control {
	struct dmub_cmd_header header;  
	struct dmub_rb_cmd_domain_control_data data;  
};
struct dmub_cmd_dig_dpia_control_data {
	uint8_t enc_id;          
	uint8_t action;          
	union {
		uint8_t digmode;     
		uint8_t dplaneset;   
	} mode_laneset;
	uint8_t lanenum;         
	uint32_t symclk_10khz;   
	uint8_t hpdsel;          
	uint8_t digfe_sel;       
	uint8_t dpia_id;         
	uint8_t fec_rdy : 1;
	uint8_t reserved : 7;
	uint32_t reserved1;
};
struct dmub_rb_cmd_dig1_dpia_control {
	struct dmub_cmd_header header;
	struct dmub_cmd_dig_dpia_control_data dpia_control;
};
struct set_config_cmd_payload {
	uint8_t msg_type;  
	uint8_t msg_data;  
};
struct dmub_cmd_set_config_control_data {
	struct set_config_cmd_payload cmd_pkt;
	uint8_t instance;  
	uint8_t immed_status;  
};
struct dmub_rb_cmd_set_config_access {
	struct dmub_cmd_header header;  
	struct dmub_cmd_set_config_control_data set_config_control;  
};
struct dmub_cmd_mst_alloc_slots_control_data {
	uint8_t mst_alloc_slots;  
	uint8_t instance;  
	uint8_t immed_status;  
	uint8_t mst_slots_in_use;  
};
struct dmub_rb_cmd_set_mst_alloc_slots {
	struct dmub_cmd_header header;  
	struct dmub_cmd_mst_alloc_slots_control_data mst_slots_control;  
};
struct dmub_rb_cmd_dpia_hpd_int_enable {
	struct dmub_cmd_header header;  
	uint32_t enable;  
};
struct dmub_rb_cmd_dpphy_init {
	struct dmub_cmd_header header;  
	uint8_t reserved[60];  
};
enum dp_aux_request_action {
	DP_AUX_REQ_ACTION_I2C_WRITE		= 0x00,
	DP_AUX_REQ_ACTION_I2C_READ		= 0x10,
	DP_AUX_REQ_ACTION_I2C_STATUS_REQ	= 0x20,
	DP_AUX_REQ_ACTION_I2C_WRITE_MOT		= 0x40,
	DP_AUX_REQ_ACTION_I2C_READ_MOT		= 0x50,
	DP_AUX_REQ_ACTION_I2C_STATUS_REQ_MOT	= 0x60,
	DP_AUX_REQ_ACTION_DPCD_WRITE		= 0x80,
	DP_AUX_REQ_ACTION_DPCD_READ		= 0x90
};
enum aux_return_code_type {
	AUX_RET_SUCCESS = 0,
	AUX_RET_ERROR_UNKNOWN,
	AUX_RET_ERROR_INVALID_REPLY,
	AUX_RET_ERROR_TIMEOUT,
	AUX_RET_ERROR_HPD_DISCON,
	AUX_RET_ERROR_ENGINE_ACQUIRE,
	AUX_RET_ERROR_INVALID_OPERATION,
	AUX_RET_ERROR_PROTOCOL_ERROR,
};
enum aux_channel_type {
	AUX_CHANNEL_LEGACY_DDC,
	AUX_CHANNEL_DPIA
};
struct aux_transaction_parameters {
	uint8_t is_i2c_over_aux;  
	uint8_t action;  
	uint8_t length;  
	uint8_t reserved;  
	uint32_t address;  
	uint8_t data[16];  
};
struct dmub_cmd_dp_aux_control_data {
	uint8_t instance;  
	uint8_t manual_acq_rel_enable;  
	uint8_t sw_crc_enabled;  
	uint8_t reserved0;  
	uint16_t timeout;  
	uint16_t reserved1;  
	enum aux_channel_type type;  
	struct aux_transaction_parameters dpaux;  
};
struct dmub_rb_cmd_dp_aux_access {
	struct dmub_cmd_header header;
	struct dmub_cmd_dp_aux_control_data aux_control;
};
struct dmub_rb_cmd_outbox1_enable {
	struct dmub_cmd_header header;
	uint32_t enable;
};
struct aux_reply_data {
	uint8_t command;
	uint8_t length;
	uint8_t pad[2];
	uint8_t data[16];
};
struct aux_reply_control_data {
	uint32_t handle;
	uint8_t instance;
	uint8_t result;
	uint16_t pad;
};
struct dmub_rb_cmd_dp_aux_reply {
	struct dmub_cmd_header header;
	struct aux_reply_control_data control;
	struct aux_reply_data reply_data;
};
enum dp_hpd_type {
	DP_HPD = 0,
	DP_IRQ
};
enum dp_hpd_status {
	DP_HPD_UNPLUG = 0,
	DP_HPD_PLUG
};
struct dp_hpd_data {
	uint8_t instance;
	uint8_t hpd_type;
	uint8_t hpd_status;
	uint8_t pad;
};
struct dmub_rb_cmd_dp_hpd_notify {
	struct dmub_cmd_header header;
	struct dp_hpd_data hpd_data;
};
enum set_config_status {
	SET_CONFIG_PENDING = 0,
	SET_CONFIG_ACK_RECEIVED,
	SET_CONFIG_RX_TIMEOUT,
	SET_CONFIG_UNKNOWN_ERROR,
};
struct set_config_reply_control_data {
	uint8_t instance;  
	uint8_t status;  
	uint16_t pad;  
};
struct dmub_rb_cmd_dp_set_config_reply {
	struct dmub_cmd_header header;
	struct set_config_reply_control_data set_config_reply_control;
};
struct dpia_notification_header {
	uint8_t instance;  
	uint8_t reserved[3];
	enum dmub_cmd_dpia_notification_type type;  
};
struct dpia_notification_common {
	uint8_t cmd_buffer[DMUB_RB_CMD_SIZE - sizeof(struct dmub_cmd_header)
								- sizeof(struct dpia_notification_header)];
};
struct dpia_bw_allocation_notify_data {
	union {
		struct {
			uint16_t cm_bw_alloc_support: 1;  
			uint16_t bw_request_failed: 1;  
			uint16_t bw_request_succeeded: 1;  
			uint16_t est_bw_changed: 1;  
			uint16_t bw_alloc_cap_changed: 1;  
			uint16_t reserved: 11;  
		} bits;
		uint16_t flags;
	};
	uint8_t cm_id;  
	uint8_t group_id;  
	uint8_t granularity;  
	uint8_t estimated_bw;  
	uint8_t allocated_bw;  
	uint8_t reserved;
};
union dpia_notification_data {
	struct dpia_notification_common common_data;
	struct dpia_bw_allocation_notify_data dpia_bw_alloc;
};
struct dpia_notification_payload {
	struct dpia_notification_header header;
	union dpia_notification_data data;  
};
struct dmub_rb_cmd_dpia_notification {
	struct dmub_cmd_header header;  
	struct dpia_notification_payload payload;  
};
struct dmub_cmd_hpd_state_query_data {
	uint8_t instance;  
	uint8_t result;  
	uint16_t pad;  
	enum aux_channel_type ch_type;  
	enum aux_return_code_type status;  
};
struct dmub_rb_cmd_query_hpd_state {
	struct dmub_cmd_header header;
	struct dmub_cmd_hpd_state_query_data data;
};
enum dmub_cmd_psr_type {
	DMUB_CMD__PSR_SET_VERSION		= 0,
	DMUB_CMD__PSR_COPY_SETTINGS		= 1,
	DMUB_CMD__PSR_ENABLE			= 2,
	DMUB_CMD__PSR_DISABLE			= 3,
	DMUB_CMD__PSR_SET_LEVEL			= 4,
	DMUB_CMD__PSR_FORCE_STATIC		= 5,
	DMUB_CMD__SET_SINK_VTOTAL_IN_PSR_ACTIVE = 6,
	DMUB_CMD__SET_PSR_POWER_OPT = 7,
};
enum dmub_cmd_fams_type {
	DMUB_CMD__FAMS_SETUP_FW_CTRL	= 0,
	DMUB_CMD__FAMS_DRR_UPDATE		= 1,
	DMUB_CMD__HANDLE_SUBVP_CMD	= 2,  
	DMUB_CMD__FAMS_SET_MANUAL_TRIGGER = 3,
};
enum psr_version {
	PSR_VERSION_1				= 0,
	PSR_VERSION_SU_1			= 1,
	PSR_VERSION_UNSUPPORTED			= 0xFFFFFFFF,
};
enum dmub_cmd_mall_type {
	DMUB_CMD__MALL_ACTION_ALLOW = 0,
	DMUB_CMD__MALL_ACTION_DISALLOW = 1,
	DMUB_CMD__MALL_ACTION_COPY_CURSOR = 2,
	DMUB_CMD__MALL_ACTION_NO_DF_REQ = 3,
};
enum phy_link_rate {
	PHY_RATE_UNKNOWN = 0,
	PHY_RATE_162 = 1,
	PHY_RATE_216 = 2,
	PHY_RATE_243 = 3,
	PHY_RATE_270 = 4,
	PHY_RATE_324 = 5,
	PHY_RATE_432 = 6,
	PHY_RATE_540 = 7,
	PHY_RATE_810 = 8,
	PHY_RATE_1000 = 9,
	PHY_RATE_1350 = 10,
	PHY_RATE_2000 = 11,
};
enum dmub_phy_fsm_state {
	DMUB_PHY_FSM_POWER_UP_DEFAULT = 0,
	DMUB_PHY_FSM_RESET,
	DMUB_PHY_FSM_RESET_RELEASED,
	DMUB_PHY_FSM_SRAM_LOAD_DONE,
	DMUB_PHY_FSM_INITIALIZED,
	DMUB_PHY_FSM_CALIBRATED,
	DMUB_PHY_FSM_CALIBRATED_LP,
	DMUB_PHY_FSM_CALIBRATED_PG,
	DMUB_PHY_FSM_POWER_DOWN,
	DMUB_PHY_FSM_PLL_EN,
	DMUB_PHY_FSM_TX_EN,
	DMUB_PHY_FSM_FAST_LP,
	DMUB_PHY_FSM_P2_PLL_OFF_CPM,
	DMUB_PHY_FSM_P2_PLL_OFF_PG,
	DMUB_PHY_FSM_P2_PLL_OFF,
	DMUB_PHY_FSM_P2_PLL_ON,
};
struct dmub_cmd_psr_copy_settings_data {
	union dmub_psr_debug_flags debug;
	uint16_t psr_level;
	uint8_t dpp_inst;
	uint8_t mpcc_inst;
	uint8_t opp_inst;
	uint8_t otg_inst;
	uint8_t digfe_inst;
	uint8_t digbe_inst;
	uint8_t dpphy_inst;
	uint8_t aux_inst;
	uint8_t smu_optimizations_en;
	uint8_t frame_delay;
	uint8_t frame_cap_ind;
	uint8_t su_y_granularity;
	uint8_t line_capture_indication;
	uint8_t multi_disp_optimizations_en;
	uint16_t init_sdp_deadline;
	uint8_t rate_control_caps ;
	uint8_t force_ffu_mode;
	uint32_t line_time_in_us;
	uint8_t fec_enable_status;
	uint8_t fec_enable_delay_in100us;
	uint8_t cmd_version;
	uint8_t panel_inst;
	uint8_t dsc_enable_status;
	uint8_t use_phy_fsm;
	uint8_t relock_delay_frame_cnt;
	uint8_t pad3;
	uint16_t dsc_slice_height;
	uint16_t pad;
};
struct dmub_rb_cmd_psr_copy_settings {
	struct dmub_cmd_header header;
	struct dmub_cmd_psr_copy_settings_data psr_copy_settings_data;
};
struct dmub_cmd_psr_set_level_data {
	uint16_t psr_level;
	uint8_t cmd_version;
	uint8_t panel_inst;
};
struct dmub_rb_cmd_psr_set_level {
	struct dmub_cmd_header header;
	struct dmub_cmd_psr_set_level_data psr_set_level_data;
};
struct dmub_rb_cmd_psr_enable_data {
	uint8_t cmd_version;
	uint8_t panel_inst;
	uint8_t phy_fsm_state;
	uint8_t phy_rate;
};
struct dmub_rb_cmd_psr_enable {
	struct dmub_cmd_header header;
	struct dmub_rb_cmd_psr_enable_data data;
};
struct dmub_cmd_psr_set_version_data {
	enum psr_version version;
	uint8_t cmd_version;
	uint8_t panel_inst;
	uint8_t pad[2];
};
struct dmub_rb_cmd_psr_set_version {
	struct dmub_cmd_header header;
	struct dmub_cmd_psr_set_version_data psr_set_version_data;
};
struct dmub_cmd_psr_force_static_data {
	uint8_t cmd_version;
	uint8_t panel_inst;
	uint8_t pad[2];
};
struct dmub_rb_cmd_psr_force_static {
	struct dmub_cmd_header header;
	struct dmub_cmd_psr_force_static_data psr_force_static_data;
};
union dmub_psr_su_debug_flags {
	struct {
		uint8_t update_dirty_rect_only : 1;
		uint8_t reset_state : 1;
	} bitfields;
	uint32_t u32All;
};
struct dmub_cmd_update_dirty_rect_data {
	struct dmub_rect src_dirty_rects[DMUB_MAX_DIRTY_RECTS];
	union dmub_psr_su_debug_flags debug_flags;
	uint8_t pipe_idx;
	uint8_t dirty_rect_count;
	uint8_t cmd_version;
	uint8_t panel_inst;
};
struct dmub_rb_cmd_update_dirty_rect {
	struct dmub_cmd_header header;
	struct dmub_cmd_update_dirty_rect_data update_dirty_rect_data;
};
union dmub_reg_cursor_control_cfg {
	struct {
		uint32_t     cur_enable: 1;
		uint32_t         reser0: 3;
		uint32_t cur_2x_magnify: 1;
		uint32_t         reser1: 3;
		uint32_t           mode: 3;
		uint32_t         reser2: 5;
		uint32_t          pitch: 2;
		uint32_t         reser3: 6;
		uint32_t line_per_chunk: 5;
		uint32_t         reser4: 3;
	} bits;
	uint32_t raw;
};
struct dmub_cursor_position_cache_hubp {
	union dmub_reg_cursor_control_cfg cur_ctl;
	union dmub_reg_position_cfg {
		struct {
			uint32_t cur_x_pos: 16;
			uint32_t cur_y_pos: 16;
		} bits;
		uint32_t raw;
	} position;
	union dmub_reg_hot_spot_cfg {
		struct {
			uint32_t hot_x: 16;
			uint32_t hot_y: 16;
		} bits;
		uint32_t raw;
	} hot_spot;
	union dmub_reg_dst_offset_cfg {
		struct {
			uint32_t dst_x_offset: 13;
			uint32_t reserved: 19;
		} bits;
		uint32_t raw;
	} dst_offset;
};
union dmub_reg_cur0_control_cfg {
	struct {
		uint32_t     cur0_enable: 1;
		uint32_t  expansion_mode: 1;
		uint32_t          reser0: 1;
		uint32_t     cur0_rom_en: 1;
		uint32_t            mode: 3;
		uint32_t        reserved: 25;
	} bits;
	uint32_t raw;
};
struct dmub_cursor_position_cache_dpp {
	union dmub_reg_cur0_control_cfg cur0_ctl;
};
struct dmub_cursor_position_cfg {
	struct  dmub_cursor_position_cache_hubp pHubp;
	struct  dmub_cursor_position_cache_dpp  pDpp;
	uint8_t pipe_idx;
	uint8_t padding[3];
};
struct dmub_cursor_attribute_cache_hubp {
	uint32_t SURFACE_ADDR_HIGH;
	uint32_t SURFACE_ADDR;
	union    dmub_reg_cursor_control_cfg  cur_ctl;
	union    dmub_reg_cursor_size_cfg {
		struct {
			uint32_t width: 16;
			uint32_t height: 16;
		} bits;
		uint32_t raw;
	} size;
	union    dmub_reg_cursor_settings_cfg {
		struct {
			uint32_t     dst_y_offset: 8;
			uint32_t chunk_hdl_adjust: 2;
			uint32_t         reserved: 22;
		} bits;
		uint32_t raw;
	} settings;
};
struct dmub_cursor_attribute_cache_dpp {
	union dmub_reg_cur0_control_cfg cur0_ctl;
};
struct dmub_cursor_attributes_cfg {
	struct  dmub_cursor_attribute_cache_hubp aHubp;
	struct  dmub_cursor_attribute_cache_dpp  aDpp;
};
struct dmub_cmd_update_cursor_payload0 {
	struct dmub_rect cursor_rect;
	union dmub_psr_su_debug_flags debug_flags;
	uint8_t enable;
	uint8_t pipe_idx;
	uint8_t cmd_version;
	uint8_t panel_inst;
	struct dmub_cursor_position_cfg position_cfg;
};
struct dmub_cmd_update_cursor_payload1 {
	struct dmub_cursor_attributes_cfg attribute_cfg;
};
union dmub_cmd_update_cursor_info_data {
	struct dmub_cmd_update_cursor_payload0 payload0;
	struct dmub_cmd_update_cursor_payload1 payload1;
};
struct dmub_rb_cmd_update_cursor_info {
	struct dmub_cmd_header header;
	union dmub_cmd_update_cursor_info_data update_cursor_info_data;
};
struct dmub_cmd_psr_set_vtotal_data {
	uint16_t psr_vtotal_idle;
	uint8_t cmd_version;
	uint8_t panel_inst;
	uint16_t psr_vtotal_su;
	uint8_t pad2[2];
};
struct dmub_rb_cmd_psr_set_vtotal {
	struct dmub_cmd_header header;
	struct dmub_cmd_psr_set_vtotal_data psr_set_vtotal_data;
};
struct dmub_cmd_psr_set_power_opt_data {
	uint8_t cmd_version;
	uint8_t panel_inst;
	uint8_t pad[2];
	uint32_t power_opt;
};
#define REPLAY_RESIDENCY_MODE_SHIFT            (0)
#define REPLAY_RESIDENCY_ENABLE_SHIFT          (1)
#define REPLAY_RESIDENCY_MODE_MASK             (0x1 << REPLAY_RESIDENCY_MODE_SHIFT)
# define REPLAY_RESIDENCY_MODE_PHY             (0x0 << REPLAY_RESIDENCY_MODE_SHIFT)
# define REPLAY_RESIDENCY_MODE_ALPM            (0x1 << REPLAY_RESIDENCY_MODE_SHIFT)
#define REPLAY_RESIDENCY_ENABLE_MASK           (0x1 << REPLAY_RESIDENCY_ENABLE_SHIFT)
# define REPLAY_RESIDENCY_DISABLE              (0x0 << REPLAY_RESIDENCY_ENABLE_SHIFT)
# define REPLAY_RESIDENCY_ENABLE               (0x1 << REPLAY_RESIDENCY_ENABLE_SHIFT)
enum replay_state {
	REPLAY_STATE_0			= 0x0,
	REPLAY_STATE_1			= 0x10,
	REPLAY_STATE_1A			= 0x11,
	REPLAY_STATE_2			= 0x20,
	REPLAY_STATE_3			= 0x30,
	REPLAY_STATE_3INIT		= 0x31,
	REPLAY_STATE_4			= 0x40,
	REPLAY_STATE_4A			= 0x41,
	REPLAY_STATE_4B			= 0x42,
	REPLAY_STATE_4C			= 0x43,
	REPLAY_STATE_4D			= 0x44,
	REPLAY_STATE_4B_LOCKED		= 0x4A,
	REPLAY_STATE_4C_UNLOCKED	= 0x4B,
	REPLAY_STATE_5			= 0x50,
	REPLAY_STATE_5A			= 0x51,
	REPLAY_STATE_5B			= 0x52,
	REPLAY_STATE_5A_LOCKED		= 0x5A,
	REPLAY_STATE_5B_UNLOCKED	= 0x5B,
	REPLAY_STATE_6			= 0x60,
	REPLAY_STATE_6A			= 0x61,
	REPLAY_STATE_6B			= 0x62,
	REPLAY_STATE_INVALID		= 0xFF,
};
enum dmub_cmd_replay_type {
	DMUB_CMD__REPLAY_COPY_SETTINGS		= 0,
	DMUB_CMD__REPLAY_ENABLE			= 1,
	DMUB_CMD__SET_REPLAY_POWER_OPT		= 2,
	DMUB_CMD__REPLAY_SET_COASTING_VTOTAL	= 3,
};
struct dmub_cmd_replay_copy_settings_data {
	union replay_debug_flags debug;
	union replay_hw_flags flags;
	uint8_t dpp_inst;
	uint8_t otg_inst;
	uint8_t digfe_inst;
	uint8_t digbe_inst;
	uint8_t aux_inst;
	uint8_t panel_inst;
	uint8_t pixel_deviation_per_line;
	uint8_t max_deviation_line;
	uint32_t line_time_in_ns;
	uint8_t dpphy_inst;
	uint8_t smu_optimizations_en;
	uint8_t replay_timing_sync_supported;
	uint8_t use_phy_fsm;
};
struct dmub_rb_cmd_replay_copy_settings {
	struct dmub_cmd_header header;
	struct dmub_cmd_replay_copy_settings_data replay_copy_settings_data;
};
enum replay_enable {
	REPLAY_DISABLE				= 0,
	REPLAY_ENABLE				= 1,
};
struct dmub_rb_cmd_replay_enable_data {
	uint8_t enable;
	uint8_t panel_inst;
	uint8_t phy_fsm_state;
	uint8_t phy_rate;
};
struct dmub_rb_cmd_replay_enable {
	struct dmub_cmd_header header;
	struct dmub_rb_cmd_replay_enable_data data;
};
struct dmub_cmd_replay_set_power_opt_data {
	uint8_t panel_inst;
	uint8_t pad[3];
	uint32_t power_opt;
};
struct dmub_rb_cmd_replay_set_power_opt {
	struct dmub_cmd_header header;
	struct dmub_cmd_replay_set_power_opt_data replay_set_power_opt_data;
};
struct dmub_cmd_replay_set_coasting_vtotal_data {
	uint16_t coasting_vtotal;
	uint8_t cmd_version;
	uint8_t panel_inst;
};
struct dmub_rb_cmd_replay_set_coasting_vtotal {
	struct dmub_cmd_header header;
	struct dmub_cmd_replay_set_coasting_vtotal_data replay_set_coasting_vtotal_data;
};
struct dmub_rb_cmd_psr_set_power_opt {
	struct dmub_cmd_header header;
	struct dmub_cmd_psr_set_power_opt_data psr_set_power_opt_data;
};
union dmub_hw_lock_flags {
	struct {
		uint8_t lock_pipe   : 1;
		uint8_t lock_cursor : 1;
		uint8_t lock_dig    : 1;
		uint8_t triple_buffer_lock : 1;
	} bits;
	uint8_t u8All;
};
struct dmub_hw_lock_inst_flags {
	uint8_t otg_inst;
	uint8_t opp_inst;
	uint8_t dig_inst;
	uint8_t pad;
};
enum hw_lock_client {
	HW_LOCK_CLIENT_DRIVER = 0,
	HW_LOCK_CLIENT_PSR_SU		= 1,
	HW_LOCK_CLIENT_REPLAY           = 4,
	HW_LOCK_CLIENT_INVALID = 0xFFFFFFFF,
};
struct dmub_cmd_lock_hw_data {
	enum hw_lock_client client;
	struct dmub_hw_lock_inst_flags inst_flags;
	union dmub_hw_lock_flags hw_locks;
	uint8_t lock;
	uint8_t should_release;
	uint8_t pad;
};
struct dmub_rb_cmd_lock_hw {
	struct dmub_cmd_header header;
	struct dmub_cmd_lock_hw_data lock_hw_data;
};
enum dmub_cmd_abm_type {
	DMUB_CMD__ABM_INIT_CONFIG	= 0,
	DMUB_CMD__ABM_SET_PIPE		= 1,
	DMUB_CMD__ABM_SET_BACKLIGHT	= 2,
	DMUB_CMD__ABM_SET_LEVEL		= 3,
	DMUB_CMD__ABM_SET_AMBIENT_LEVEL	= 4,
	DMUB_CMD__ABM_SET_PWM_FRAC	= 5,
	DMUB_CMD__ABM_PAUSE	= 6,
	DMUB_CMD__ABM_SAVE_RESTORE	= 7,
};
struct abm_config_table {
	uint16_t crgb_thresh[NUM_POWER_FN_SEGS];                  
	uint16_t crgb_offset[NUM_POWER_FN_SEGS];                  
	uint16_t crgb_slope[NUM_POWER_FN_SEGS];                   
	uint16_t backlight_thresholds[NUM_BL_CURVE_SEGS];         
	uint16_t backlight_offsets[NUM_BL_CURVE_SEGS];            
	uint16_t ambient_thresholds_lux[NUM_AMBI_LEVEL];          
	uint16_t min_abm_backlight;                               
	uint8_t min_reduction[NUM_AMBI_LEVEL][NUM_AGGR_LEVEL];    
	uint8_t max_reduction[NUM_AMBI_LEVEL][NUM_AGGR_LEVEL];    
	uint8_t bright_pos_gain[NUM_AMBI_LEVEL][NUM_AGGR_LEVEL];  
	uint8_t dark_pos_gain[NUM_AMBI_LEVEL][NUM_AGGR_LEVEL];    
	uint8_t hybrid_factor[NUM_AGGR_LEVEL];                    
	uint8_t contrast_factor[NUM_AGGR_LEVEL];                  
	uint8_t deviation_gain[NUM_AGGR_LEVEL];                   
	uint8_t min_knee[NUM_AGGR_LEVEL];                         
	uint8_t max_knee[NUM_AGGR_LEVEL];                         
	uint8_t iir_curve[NUM_AMBI_LEVEL];                        
	uint8_t pad3[3];                                          
	uint16_t blRampReduction[NUM_AGGR_LEVEL];                 
	uint16_t blRampStart[NUM_AGGR_LEVEL];                     
};
struct dmub_cmd_abm_set_pipe_data {
	uint8_t otg_inst;
	uint8_t panel_inst;
	uint8_t set_pipe_option;
	uint8_t ramping_boundary;
	uint8_t pwrseq_inst;
	uint8_t pad[3];
};
struct dmub_rb_cmd_abm_set_pipe {
	struct dmub_cmd_header header;
	struct dmub_cmd_abm_set_pipe_data abm_set_pipe_data;
};
struct dmub_cmd_abm_set_backlight_data {
	uint32_t frame_ramp;
	uint32_t backlight_user_level;
	uint8_t version;
	uint8_t panel_mask;
	uint8_t pad[2];
};
struct dmub_rb_cmd_abm_set_backlight {
	struct dmub_cmd_header header;
	struct dmub_cmd_abm_set_backlight_data abm_set_backlight_data;
};
struct dmub_cmd_abm_set_level_data {
	uint32_t level;
	uint8_t version;
	uint8_t panel_mask;
	uint8_t pad[2];
};
struct dmub_rb_cmd_abm_set_level {
	struct dmub_cmd_header header;
	struct dmub_cmd_abm_set_level_data abm_set_level_data;
};
struct dmub_cmd_abm_set_ambient_level_data {
	uint32_t ambient_lux;
	uint8_t version;
	uint8_t panel_mask;
	uint8_t pad[2];
};
struct dmub_rb_cmd_abm_set_ambient_level {
	struct dmub_cmd_header header;
	struct dmub_cmd_abm_set_ambient_level_data abm_set_ambient_level_data;
};
struct dmub_cmd_abm_set_pwm_frac_data {
	uint32_t fractional_pwm;
	uint8_t version;
	uint8_t panel_mask;
	uint8_t pad[2];
};
struct dmub_rb_cmd_abm_set_pwm_frac {
	struct dmub_cmd_header header;
	struct dmub_cmd_abm_set_pwm_frac_data abm_set_pwm_frac_data;
};
struct dmub_cmd_abm_init_config_data {
	union dmub_addr src;
	uint16_t bytes;
	uint8_t version;
	uint8_t panel_mask;
	uint8_t pad[2];
};
struct dmub_rb_cmd_abm_init_config {
	struct dmub_cmd_header header;
	struct dmub_cmd_abm_init_config_data abm_init_config_data;
};
struct dmub_cmd_abm_pause_data {
	uint8_t panel_mask;
	uint8_t otg_inst;
	uint8_t enable;
	uint8_t pad[1];
};
struct dmub_rb_cmd_abm_pause {
	struct dmub_cmd_header header;
	struct dmub_cmd_abm_pause_data abm_pause_data;
};
struct dmub_rb_cmd_abm_save_restore {
	struct dmub_cmd_header header;
	uint8_t otg_inst;
	uint8_t freeze;
	uint8_t debug;
	struct dmub_cmd_abm_init_config_data abm_init_config_data;
};
struct dmub_cmd_query_feature_caps_data {
	struct dmub_feature_caps feature_caps;
};
struct dmub_rb_cmd_query_feature_caps {
	struct dmub_cmd_header header;
	struct dmub_cmd_query_feature_caps_data query_feature_caps_data;
};
struct dmub_cmd_visual_confirm_color_data {
struct dmub_visual_confirm_color visual_confirm_color;
};
struct dmub_rb_cmd_get_visual_confirm_color {
	struct dmub_cmd_header header;
	struct dmub_cmd_visual_confirm_color_data visual_confirm_color_data;
};
struct dmub_optc_state {
	uint32_t v_total_max;
	uint32_t v_total_min;
	uint32_t tg_inst;
};
struct dmub_rb_cmd_drr_update {
		struct dmub_cmd_header header;
		struct dmub_optc_state dmub_optc_state_req;
};
struct dmub_cmd_fw_assisted_mclk_switch_pipe_data {
	uint32_t pix_clk_100hz;
	uint8_t max_ramp_step;
	uint8_t pipes;
	uint8_t min_refresh_in_hz;
	uint8_t pipe_count;
	uint8_t pipe_index[4];
};
struct dmub_cmd_fw_assisted_mclk_switch_config {
	uint8_t fams_enabled;
	uint8_t visual_confirm_enabled;
	uint16_t vactive_stretch_margin_us;  
	struct dmub_cmd_fw_assisted_mclk_switch_pipe_data pipe_data[DMUB_MAX_FPO_STREAMS];
};
struct dmub_rb_cmd_fw_assisted_mclk_switch {
	struct dmub_cmd_header header;
	struct dmub_cmd_fw_assisted_mclk_switch_config config_data;
};
enum dmub_cmd_panel_cntl_type {
	DMUB_CMD__PANEL_CNTL_HW_INIT = 0,
	DMUB_CMD__PANEL_CNTL_QUERY_BACKLIGHT_INFO = 1,
};
struct dmub_cmd_panel_cntl_data {
	uint32_t pwrseq_inst;  
	uint32_t current_backlight;  
	uint32_t bl_pwm_cntl;  
	uint32_t bl_pwm_period_cntl;  
	uint32_t bl_pwm_ref_div1;  
	uint8_t is_backlight_on : 1;  
	uint8_t is_powered_on : 1;  
	uint8_t padding[3];
	uint32_t bl_pwm_ref_div2;  
	uint8_t reserved[4];
};
struct dmub_rb_cmd_panel_cntl {
	struct dmub_cmd_header header;  
	struct dmub_cmd_panel_cntl_data data;  
};
struct dmub_cmd_lvtma_control_data {
	uint8_t uc_pwr_action;  
	uint8_t bypass_panel_control_wait;
	uint8_t reserved_0[2];  
	uint8_t pwrseq_inst;  
	uint8_t reserved_1[3];  
};
struct dmub_rb_cmd_lvtma_control {
	struct dmub_cmd_header header;
	struct dmub_cmd_lvtma_control_data data;
};
struct dmub_rb_cmd_transmitter_query_dp_alt_data {
	uint8_t phy_id;  
	uint8_t is_usb;  
	uint8_t is_dp_alt_disable;  
	uint8_t is_dp4;  
};
struct dmub_rb_cmd_transmitter_query_dp_alt {
	struct dmub_cmd_header header;  
	struct dmub_rb_cmd_transmitter_query_dp_alt_data data;  
};
#define DMUB_EDID_CEA_DATA_CHUNK_BYTES 8
struct dmub_cmd_send_edid_cea {
	uint16_t offset;	 
	uint8_t length;	 
	uint16_t cea_total_length;   
	uint8_t payload[DMUB_EDID_CEA_DATA_CHUNK_BYTES];  
	uint8_t pad[3];  
};
struct dmub_cmd_edid_cea_amd_vsdb {
	uint8_t vsdb_found;		 
	uint8_t freesync_supported;	 
	uint16_t amd_vsdb_version;	 
	uint16_t min_frame_rate;	 
	uint16_t max_frame_rate;	 
};
struct dmub_cmd_edid_cea_ack {
	uint16_t offset;	 
	uint8_t success;	 
	uint8_t pad;		 
};
enum dmub_cmd_edid_cea_reply_type {
	DMUB_CMD__EDID_CEA_AMD_VSDB	= 1,  
	DMUB_CMD__EDID_CEA_ACK		= 2,  
};
struct dmub_rb_cmd_edid_cea {
	struct dmub_cmd_header header;	 
	union dmub_cmd_edid_cea_data {
		struct dmub_cmd_send_edid_cea input;  
		struct dmub_cmd_edid_cea_output {  
			uint8_t type;	 
			union {
				struct dmub_cmd_edid_cea_amd_vsdb amd_vsdb;
				struct dmub_cmd_edid_cea_ack ack;
			};
		} output;	 
	} data;	 
};
struct dmub_cmd_cable_id_input {
	uint8_t phy_inst;   
};
struct dmub_cmd_cable_id_output {
	uint8_t UHBR10_20_CAPABILITY	:2;  
	uint8_t UHBR13_5_CAPABILITY	:1;  
	uint8_t CABLE_TYPE		:3;  
	uint8_t RESERVED		:2;  
};
struct dmub_rb_cmd_get_usbc_cable_id {
	struct dmub_cmd_header header;  
	union dmub_cmd_cable_id_data {
		struct dmub_cmd_cable_id_input input;  
		struct dmub_cmd_cable_id_output output;  
		uint8_t output_raw;  
	} data;
};
enum dmub_cmd_secure_display_type {
	DMUB_CMD__SECURE_DISPLAY_TEST_CMD = 0,		 
	DMUB_CMD__SECURE_DISPLAY_CRC_STOP_UPDATE,
	DMUB_CMD__SECURE_DISPLAY_CRC_WIN_NOTIFY
};
struct dmub_rb_cmd_secure_display {
	struct dmub_cmd_header header;
	struct dmub_cmd_roi_info {
		uint16_t x_start;
		uint16_t x_end;
		uint16_t y_start;
		uint16_t y_end;
		uint8_t otg_id;
		uint8_t phy_id;
	} roi_info;
};
union dmub_rb_cmd {
	struct dmub_rb_cmd_common cmd_common;
	struct dmub_rb_cmd_read_modify_write read_modify_write;
	struct dmub_rb_cmd_reg_field_update_sequence reg_field_update_seq;
	struct dmub_rb_cmd_burst_write burst_write;
	struct dmub_rb_cmd_reg_wait reg_wait;
	struct dmub_rb_cmd_digx_encoder_control digx_encoder_control;
	struct dmub_rb_cmd_set_pixel_clock set_pixel_clock;
	struct dmub_rb_cmd_enable_disp_power_gating enable_disp_power_gating;
	struct dmub_rb_cmd_dpphy_init dpphy_init;
	struct dmub_rb_cmd_dig1_transmitter_control dig1_transmitter_control;
	struct dmub_rb_cmd_domain_control domain_control;
	struct dmub_rb_cmd_psr_set_version psr_set_version;
	struct dmub_rb_cmd_psr_copy_settings psr_copy_settings;
	struct dmub_rb_cmd_psr_enable psr_enable;
	struct dmub_rb_cmd_psr_set_level psr_set_level;
	struct dmub_rb_cmd_psr_force_static psr_force_static;
	struct dmub_rb_cmd_update_dirty_rect update_dirty_rect;
	struct dmub_rb_cmd_update_cursor_info update_cursor_info;
	struct dmub_rb_cmd_lock_hw lock_hw;
	struct dmub_rb_cmd_psr_set_vtotal psr_set_vtotal;
	struct dmub_rb_cmd_psr_set_power_opt psr_set_power_opt;
	struct dmub_rb_cmd_PLAT_54186_wa PLAT_54186_wa;
	struct dmub_rb_cmd_mall mall;
	struct dmub_rb_cmd_cab_for_ss cab;
	struct dmub_rb_cmd_fw_assisted_mclk_switch_v2 fw_assisted_mclk_switch_v2;
	struct dmub_rb_cmd_idle_opt_dcn_restore dcn_restore;
	struct dmub_rb_cmd_clk_mgr_notify_clocks notify_clocks;
	struct dmub_rb_cmd_panel_cntl panel_cntl;
	struct dmub_rb_cmd_abm_set_pipe abm_set_pipe;
	struct dmub_rb_cmd_abm_set_backlight abm_set_backlight;
	struct dmub_rb_cmd_abm_set_level abm_set_level;
	struct dmub_rb_cmd_abm_set_ambient_level abm_set_ambient_level;
	struct dmub_rb_cmd_abm_set_pwm_frac abm_set_pwm_frac;
	struct dmub_rb_cmd_abm_init_config abm_init_config;
	struct dmub_rb_cmd_abm_pause abm_pause;
	struct dmub_rb_cmd_abm_save_restore abm_save_restore;
	struct dmub_rb_cmd_dp_aux_access dp_aux_access;
	struct dmub_rb_cmd_outbox1_enable outbox1_enable;
	struct dmub_rb_cmd_query_feature_caps query_feature_caps;
	struct dmub_rb_cmd_get_visual_confirm_color visual_confirm_color;
	struct dmub_rb_cmd_drr_update drr_update;
	struct dmub_rb_cmd_fw_assisted_mclk_switch fw_assisted_mclk_switch;
	struct dmub_rb_cmd_lvtma_control lvtma_control;
	struct dmub_rb_cmd_transmitter_query_dp_alt query_dp_alt;
	struct dmub_rb_cmd_dig1_dpia_control dig1_dpia_control;
	struct dmub_rb_cmd_set_config_access set_config_access;
	struct dmub_rb_cmd_set_mst_alloc_slots set_mst_alloc_slots;
	struct dmub_rb_cmd_edid_cea edid_cea;
	struct dmub_rb_cmd_get_usbc_cable_id cable_id;
	struct dmub_rb_cmd_query_hpd_state query_hpd;
	struct dmub_rb_cmd_secure_display secure_display;
	struct dmub_rb_cmd_dpia_hpd_int_enable dpia_hpd_int_enable;
	struct dmub_rb_cmd_idle_opt_dcn_notify_idle idle_opt_notify_idle;
	struct dmub_rb_cmd_replay_copy_settings replay_copy_settings;
	struct dmub_rb_cmd_replay_enable replay_enable;
	struct dmub_rb_cmd_replay_set_power_opt replay_set_power_opt;
	struct dmub_rb_cmd_replay_set_coasting_vtotal replay_set_coasting_vtotal;
};
union dmub_rb_out_cmd {
	struct dmub_rb_cmd_common cmd_common;
	struct dmub_rb_cmd_dp_aux_reply dp_aux_reply;
	struct dmub_rb_cmd_dp_hpd_notify dp_hpd_notify;
	struct dmub_rb_cmd_dp_set_config_reply set_config_reply;
	struct dmub_rb_cmd_dpia_notification dpia_notification;
};
#pragma pack(pop)
#if defined(__cplusplus)
extern "C" {
#endif
struct dmub_rb_init_params {
	void *ctx;  
	void *base_address;  
	uint32_t capacity;  
	uint32_t read_ptr;  
	uint32_t write_ptr;  
};
struct dmub_rb {
	void *base_address;  
	uint32_t rptr;  
	uint32_t wrpt;  
	uint32_t capacity;  
	void *ctx;  
	void *dmub;  
};
static inline bool dmub_rb_empty(struct dmub_rb *rb)
{
	return (rb->wrpt == rb->rptr);
}
static inline bool dmub_rb_full(struct dmub_rb *rb)
{
	uint32_t data_count;
	if (rb->wrpt >= rb->rptr)
		data_count = rb->wrpt - rb->rptr;
	else
		data_count = rb->capacity - (rb->rptr - rb->wrpt);
	return (data_count == (rb->capacity - DMUB_RB_CMD_SIZE));
}
static inline bool dmub_rb_push_front(struct dmub_rb *rb,
				      const union dmub_rb_cmd *cmd)
{
	uint64_t volatile *dst = (uint64_t volatile *)((uint8_t *)(rb->base_address) + rb->wrpt);
	const uint64_t *src = (const uint64_t *)cmd;
	uint8_t i;
	if (dmub_rb_full(rb))
		return false;
	for (i = 0; i < DMUB_RB_CMD_SIZE / sizeof(uint64_t); i++)
		*dst++ = *src++;
	rb->wrpt += DMUB_RB_CMD_SIZE;
	if (rb->wrpt >= rb->capacity)
		rb->wrpt %= rb->capacity;
	return true;
}
static inline bool dmub_rb_out_push_front(struct dmub_rb *rb,
				      const union dmub_rb_out_cmd *cmd)
{
	uint8_t *dst = (uint8_t *)(rb->base_address) + rb->wrpt;
	const uint8_t *src = (const uint8_t *)cmd;
	if (dmub_rb_full(rb))
		return false;
	dmub_memcpy(dst, src, DMUB_RB_CMD_SIZE);
	rb->wrpt += DMUB_RB_CMD_SIZE;
	if (rb->wrpt >= rb->capacity)
		rb->wrpt %= rb->capacity;
	return true;
}
static inline bool dmub_rb_front(struct dmub_rb *rb,
				 union dmub_rb_cmd  **cmd)
{
	uint8_t *rb_cmd = (uint8_t *)(rb->base_address) + rb->rptr;
	if (dmub_rb_empty(rb))
		return false;
	*cmd = (union dmub_rb_cmd *)rb_cmd;
	return true;
}
static inline void dmub_rb_get_rptr_with_offset(struct dmub_rb *rb,
				  uint32_t num_cmds,
				  uint32_t *next_rptr)
{
	*next_rptr = rb->rptr + DMUB_RB_CMD_SIZE * num_cmds;
	if (*next_rptr >= rb->capacity)
		*next_rptr %= rb->capacity;
}
static inline bool dmub_rb_peek_offset(struct dmub_rb *rb,
				 union dmub_rb_cmd  **cmd,
				 uint32_t rptr)
{
	uint8_t *rb_cmd = (uint8_t *)(rb->base_address) + rptr;
	if (dmub_rb_empty(rb))
		return false;
	*cmd = (union dmub_rb_cmd *)rb_cmd;
	return true;
}
static inline bool dmub_rb_out_front(struct dmub_rb *rb,
				 union dmub_rb_out_cmd *cmd)
{
	const uint64_t volatile *src = (const uint64_t volatile *)((uint8_t *)(rb->base_address) + rb->rptr);
	uint64_t *dst = (uint64_t *)cmd;
	uint8_t i;
	if (dmub_rb_empty(rb))
		return false;
	for (i = 0; i < DMUB_RB_CMD_SIZE / sizeof(uint64_t); i++)
		*dst++ = *src++;
	return true;
}
static inline bool dmub_rb_pop_front(struct dmub_rb *rb)
{
	if (dmub_rb_empty(rb))
		return false;
	rb->rptr += DMUB_RB_CMD_SIZE;
	if (rb->rptr >= rb->capacity)
		rb->rptr %= rb->capacity;
	return true;
}
static inline void dmub_rb_flush_pending(const struct dmub_rb *rb)
{
	uint32_t rptr = rb->rptr;
	uint32_t wptr = rb->wrpt;
	while (rptr != wptr) {
		uint64_t *data = (uint64_t *)((uint8_t *)(rb->base_address) + rptr);
		uint8_t i;
		for (i = 0; i < DMUB_RB_CMD_SIZE / sizeof(uint64_t); i++)
			(void)READ_ONCE(*data++);
		rptr += DMUB_RB_CMD_SIZE;
		if (rptr >= rb->capacity)
			rptr %= rb->capacity;
	}
}
static inline void dmub_rb_init(struct dmub_rb *rb,
				struct dmub_rb_init_params *init_params)
{
	rb->base_address = init_params->base_address;
	rb->capacity = init_params->capacity;
	rb->rptr = init_params->read_ptr;
	rb->wrpt = init_params->write_ptr;
}
static inline void dmub_rb_get_return_data(struct dmub_rb *rb,
					   union dmub_rb_cmd *cmd)
{
	uint8_t *rd_ptr = (rb->rptr == 0) ?
		(uint8_t *)rb->base_address + rb->capacity - DMUB_RB_CMD_SIZE :
		(uint8_t *)rb->base_address + rb->rptr - DMUB_RB_CMD_SIZE;
	dmub_memcpy(cmd, rd_ptr, DMUB_RB_CMD_SIZE);
}
#if defined(__cplusplus)
}
#endif
#endif  
