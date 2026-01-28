#ifndef _INTEL_BIOS_PRIVATE
#error "intel_vbt_defs.h is private to intel_bios.c"
#endif
#ifndef _INTEL_VBT_DEFS_H_
#define _INTEL_VBT_DEFS_H_
#include "intel_bios.h"
struct vbt_header {
	u8 signature[20];
	u16 version;
	u16 header_size;
	u16 vbt_size;
	u8 vbt_checksum;
	u8 reserved0;
	u32 bdb_offset;
	u32 aim_offset[4];
} __packed;
struct bdb_header {
	u8 signature[16];
	u16 version;
	u16 header_size;
	u16 bdb_size;
} __packed;
enum bdb_block_id {
	BDB_GENERAL_FEATURES		= 1,
	BDB_GENERAL_DEFINITIONS		= 2,
	BDB_OLD_TOGGLE_LIST		= 3,
	BDB_MODE_SUPPORT_LIST		= 4,
	BDB_GENERIC_MODE_TABLE		= 5,
	BDB_EXT_MMIO_REGS		= 6,
	BDB_SWF_IO			= 7,
	BDB_SWF_MMIO			= 8,
	BDB_PSR				= 9,
	BDB_MODE_REMOVAL_TABLE		= 10,
	BDB_CHILD_DEVICE_TABLE		= 11,
	BDB_DRIVER_FEATURES		= 12,
	BDB_DRIVER_PERSISTENCE		= 13,
	BDB_EXT_TABLE_PTRS		= 14,
	BDB_DOT_CLOCK_OVERRIDE		= 15,
	BDB_DISPLAY_SELECT		= 16,
	BDB_DRIVER_ROTATION		= 18,
	BDB_DISPLAY_REMOVE		= 19,
	BDB_OEM_CUSTOM			= 20,
	BDB_EFP_LIST			= 21,  
	BDB_SDVO_LVDS_OPTIONS		= 22,
	BDB_SDVO_PANEL_DTDS		= 23,
	BDB_SDVO_LVDS_PNP_IDS		= 24,
	BDB_SDVO_LVDS_POWER_SEQ		= 25,
	BDB_TV_OPTIONS			= 26,
	BDB_EDP				= 27,
	BDB_LVDS_OPTIONS		= 40,
	BDB_LVDS_LFP_DATA_PTRS		= 41,
	BDB_LVDS_LFP_DATA		= 42,
	BDB_LVDS_BACKLIGHT		= 43,
	BDB_LFP_POWER			= 44,
	BDB_MIPI_CONFIG			= 52,
	BDB_MIPI_SEQUENCE		= 53,
	BDB_COMPRESSION_PARAMETERS	= 56,
	BDB_GENERIC_DTD			= 58,
	BDB_SKIP			= 254,  
};
struct bdb_general_features {
	u8 panel_fitting:2;
	u8 flexaim:1;
	u8 msg_enable:1;
	u8 clear_screen:3;
	u8 color_flip:1;
	u8 download_ext_vbt:1;
	u8 enable_ssc:1;
	u8 ssc_freq:1;
	u8 enable_lfp_on_override:1;
	u8 disable_ssc_ddt:1;
	u8 underscan_vga_timings:1;
	u8 display_clock_mode:1;
	u8 vbios_hotplug_support:1;
	u8 disable_smooth_vision:1;
	u8 single_dvi:1;
	u8 rotate_180:1;					 
	u8 fdi_rx_polarity_inverted:1;
	u8 vbios_extended_mode:1;				 
	u8 copy_ilfp_dtd_to_sdvo_lvds_dtd:1;			 
	u8 panel_best_fit_timing:1;				 
	u8 ignore_strap_state:1;				 
	u8 legacy_monitor_detect;
	u8 int_crt_support:1;
	u8 int_tv_support:1;
	u8 int_efp_support:1;
	u8 dp_ssc_enable:1;	 
	u8 dp_ssc_freq:1;	 
	u8 dp_ssc_dongle_supported:1;
	u8 rsvd11:2;  
	u8 tc_hpd_retry_timeout:7;				 
	u8 rsvd12:1;
	u8 afc_startup_config:2;				 
	u8 rsvd13:6;
} __packed;
#define GPIO_PIN_DVI_LVDS	0x03  
#define GPIO_PIN_ADD_I2C	0x05  
#define GPIO_PIN_ADD_DDC	0x04  
#define GPIO_PIN_ADD_DDC_I2C	0x06  
#define DEVICE_HANDLE_CRT	0x0001
#define DEVICE_HANDLE_EFP1	0x0004
#define DEVICE_HANDLE_EFP2	0x0040
#define DEVICE_HANDLE_EFP3	0x0020
#define DEVICE_HANDLE_EFP4	0x0010  
#define DEVICE_HANDLE_EFP5	0x0002  
#define DEVICE_HANDLE_EFP6	0x0001  
#define DEVICE_HANDLE_EFP7	0x0100  
#define DEVICE_HANDLE_EFP8	0x0200  
#define DEVICE_HANDLE_LFP1	0x0008
#define DEVICE_HANDLE_LFP2	0x0080
#define DEVICE_TYPE_NONE	0x00
#define DEVICE_TYPE_CRT		0x01
#define DEVICE_TYPE_TV		0x09
#define DEVICE_TYPE_EFP		0x12
#define DEVICE_TYPE_LFP		0x22
#define DEVICE_TYPE_CRT_DPMS		0x6001
#define DEVICE_TYPE_CRT_DPMS_HOTPLUG	0x4001
#define DEVICE_TYPE_TV_COMPOSITE	0x0209
#define DEVICE_TYPE_TV_MACROVISION	0x0289
#define DEVICE_TYPE_TV_RF_COMPOSITE	0x020c
#define DEVICE_TYPE_TV_SVIDEO_COMPOSITE	0x0609
#define DEVICE_TYPE_TV_SCART		0x0209
#define DEVICE_TYPE_TV_CODEC_HOTPLUG_PWR 0x6009
#define DEVICE_TYPE_EFP_HOTPLUG_PWR	0x6012
#define DEVICE_TYPE_EFP_DVI_HOTPLUG_PWR	0x6052
#define DEVICE_TYPE_EFP_DVI_I		0x6053
#define DEVICE_TYPE_EFP_DVI_D_DUAL	0x6152
#define DEVICE_TYPE_EFP_DVI_D_HDCP	0x60d2
#define DEVICE_TYPE_OPENLDI_HOTPLUG_PWR	0x6062
#define DEVICE_TYPE_OPENLDI_DUALPIX	0x6162
#define DEVICE_TYPE_LFP_PANELLINK	0x5012
#define DEVICE_TYPE_LFP_CMOS_PWR	0x5042
#define DEVICE_TYPE_LFP_LVDS_PWR	0x5062
#define DEVICE_TYPE_LFP_LVDS_DUAL	0x5162
#define DEVICE_TYPE_LFP_LVDS_DUAL_HDCP	0x51e2
#define DEVICE_TYPE_INT_LFP		0x1022
#define DEVICE_TYPE_INT_TV		0x1009
#define DEVICE_TYPE_HDMI		0x60D2
#define DEVICE_TYPE_DP			0x68C6
#define DEVICE_TYPE_DP_DUAL_MODE	0x60D6
#define DEVICE_TYPE_eDP			0x78C6
#define DEVICE_TYPE_CLASS_EXTENSION	(1 << 15)
#define DEVICE_TYPE_POWER_MANAGEMENT	(1 << 14)
#define DEVICE_TYPE_HOTPLUG_SIGNALING	(1 << 13)
#define DEVICE_TYPE_INTERNAL_CONNECTOR	(1 << 12)
#define DEVICE_TYPE_NOT_HDMI_OUTPUT	(1 << 11)
#define DEVICE_TYPE_MIPI_OUTPUT		(1 << 10)
#define DEVICE_TYPE_COMPOSITE_OUTPUT	(1 << 9)
#define DEVICE_TYPE_DUAL_CHANNEL	(1 << 8)
#define DEVICE_TYPE_HIGH_SPEED_LINK	(1 << 6)
#define DEVICE_TYPE_LVDS_SIGNALING	(1 << 5)
#define DEVICE_TYPE_TMDS_DVI_SIGNALING	(1 << 4)
#define DEVICE_TYPE_VIDEO_SIGNALING	(1 << 3)
#define DEVICE_TYPE_DISPLAYPORT_OUTPUT	(1 << 2)
#define DEVICE_TYPE_DIGITAL_OUTPUT	(1 << 1)
#define DEVICE_TYPE_ANALOG_OUTPUT	(1 << 0)
#define DEVICE_CFG_NONE		0x00
#define DEVICE_CFG_12BIT_DVOB	0x01
#define DEVICE_CFG_12BIT_DVOC	0x02
#define DEVICE_CFG_24BIT_DVOBC	0x09
#define DEVICE_CFG_24BIT_DVOCB	0x0a
#define DEVICE_CFG_DUAL_DVOB	0x11
#define DEVICE_CFG_DUAL_DVOC	0x12
#define DEVICE_CFG_DUAL_DVOBC	0x13
#define DEVICE_CFG_DUAL_LINK_DVOBC	0x19
#define DEVICE_CFG_DUAL_LINK_DVOCB	0x1a
#define DEVICE_WIRE_NONE	0x00
#define DEVICE_WIRE_DVOB	0x01
#define DEVICE_WIRE_DVOC	0x02
#define DEVICE_WIRE_DVOBC	0x03
#define DEVICE_WIRE_DVOBB	0x05
#define DEVICE_WIRE_DVOCC	0x06
#define DEVICE_WIRE_DVOB_MASTER 0x0d
#define DEVICE_WIRE_DVOC_MASTER 0x0e
#define DEVICE_PORT_DVOA	0x00  
#define DEVICE_PORT_DVOB	0x01
#define DEVICE_PORT_DVOC	0x02
#define DVO_PORT_HDMIA		0
#define DVO_PORT_HDMIB		1
#define DVO_PORT_HDMIC		2
#define DVO_PORT_HDMID		3
#define DVO_PORT_LVDS		4
#define DVO_PORT_TV		5
#define DVO_PORT_CRT		6
#define DVO_PORT_DPB		7
#define DVO_PORT_DPC		8
#define DVO_PORT_DPD		9
#define DVO_PORT_DPA		10
#define DVO_PORT_DPE		11				 
#define DVO_PORT_HDMIE		12				 
#define DVO_PORT_DPF		13				 
#define DVO_PORT_HDMIF		14				 
#define DVO_PORT_DPG		15				 
#define DVO_PORT_HDMIG		16				 
#define DVO_PORT_DPH		17				 
#define DVO_PORT_HDMIH		18				 
#define DVO_PORT_DPI		19				 
#define DVO_PORT_HDMII		20				 
#define DVO_PORT_MIPIA		21				 
#define DVO_PORT_MIPIB		22				 
#define DVO_PORT_MIPIC		23				 
#define DVO_PORT_MIPID		24				 
#define HDMI_MAX_DATA_RATE_PLATFORM	0			 
#define HDMI_MAX_DATA_RATE_297		1			 
#define HDMI_MAX_DATA_RATE_165		2			 
#define HDMI_MAX_DATA_RATE_594		3			 
#define HDMI_MAX_DATA_RATE_340		4			 
#define HDMI_MAX_DATA_RATE_300		5			 
#define LEGACY_CHILD_DEVICE_CONFIG_SIZE		33
enum vbt_gmbus_ddi {
	DDC_BUS_DDI_B = 0x1,
	DDC_BUS_DDI_C,
	DDC_BUS_DDI_D,
	DDC_BUS_DDI_F,
	ICL_DDC_BUS_DDI_A = 0x1,
	ICL_DDC_BUS_DDI_B,
	TGL_DDC_BUS_DDI_C,
	RKL_DDC_BUS_DDI_D = 0x3,
	RKL_DDC_BUS_DDI_E,
	ICL_DDC_BUS_PORT_1 = 0x4,
	ICL_DDC_BUS_PORT_2,
	ICL_DDC_BUS_PORT_3,
	ICL_DDC_BUS_PORT_4,
	TGL_DDC_BUS_PORT_5,
	TGL_DDC_BUS_PORT_6,
	ADLS_DDC_BUS_PORT_TC1 = 0x2,
	ADLS_DDC_BUS_PORT_TC2,
	ADLS_DDC_BUS_PORT_TC3,
	ADLS_DDC_BUS_PORT_TC4,
	ADLP_DDC_BUS_PORT_TC1 = 0x3,
	ADLP_DDC_BUS_PORT_TC2,
	ADLP_DDC_BUS_PORT_TC3,
	ADLP_DDC_BUS_PORT_TC4
};
#define DP_AUX_A 0x40
#define DP_AUX_B 0x10
#define DP_AUX_C 0x20
#define DP_AUX_D 0x30
#define DP_AUX_E 0x50
#define DP_AUX_F 0x60
#define DP_AUX_G 0x70
#define DP_AUX_H 0x80
#define DP_AUX_I 0x90
#define BDB_216_VBT_DP_MAX_LINK_RATE_HBR3	0
#define BDB_216_VBT_DP_MAX_LINK_RATE_HBR2	1
#define BDB_216_VBT_DP_MAX_LINK_RATE_HBR	2
#define BDB_216_VBT_DP_MAX_LINK_RATE_LBR	3
#define BDB_230_VBT_DP_MAX_LINK_RATE_DEF	0
#define BDB_230_VBT_DP_MAX_LINK_RATE_LBR	1
#define BDB_230_VBT_DP_MAX_LINK_RATE_HBR	2
#define BDB_230_VBT_DP_MAX_LINK_RATE_HBR2	3
#define BDB_230_VBT_DP_MAX_LINK_RATE_HBR3	4
#define BDB_230_VBT_DP_MAX_LINK_RATE_UHBR10	5
#define BDB_230_VBT_DP_MAX_LINK_RATE_UHBR13P5	6
#define BDB_230_VBT_DP_MAX_LINK_RATE_UHBR20	7
struct child_device_config {
	u16 handle;
	u16 device_type;  
	union {
		u8  device_id[10];  
		struct {
			u8 i2c_speed;
			u8 dp_onboard_redriver_preemph:3;	 
			u8 dp_onboard_redriver_vswing:3;	 
			u8 dp_onboard_redriver_present:1;	 
			u8 reserved0:1;
			u8 dp_ondock_redriver_preemph:3;	 
			u8 dp_ondock_redriver_vswing:3;		 
			u8 dp_ondock_redriver_present:1;	 
			u8 reserved1:1;
			u8 hdmi_level_shifter_value:5;		 
			u8 hdmi_max_data_rate:3;		 
			u16 dtd_buf_ptr;			 
			u8 edidless_efp:1;			 
			u8 compression_enable:1;		 
			u8 compression_method_cps:1;		 
			u8 ganged_edp:1;			 
			u8 lttpr_non_transparent:1;		 
			u8 disable_compression_for_ext_disp:1;	 
			u8 reserved2:2;
			u8 compression_structure_index:4;	 
			u8 reserved3:4;
			u8 hdmi_max_frl_rate:4;			 
			u8 hdmi_max_frl_rate_valid:1;		 
			u8 reserved4:3;				 
			u8 reserved5;
		} __packed;
	} __packed;
	u16 addin_offset;
	u8 dvo_port;  
	u8 i2c_pin;
	u8 slave_addr;
	u8 ddc_pin;
	u16 edid_ptr;
	u8 dvo_cfg;  
	union {
		struct {
			u8 dvo2_port;
			u8 i2c2_pin;
			u8 slave2_addr;
			u8 ddc2_pin;
		} __packed;
		struct {
			u8 efp_routed:1;			 
			u8 lane_reversal:1;			 
			u8 lspcon:1;				 
			u8 iboost:1;				 
			u8 hpd_invert:1;			 
			u8 use_vbt_vswing:1;			 
			u8 dp_max_lane_count:2;			 
			u8 hdmi_support:1;			 
			u8 dp_support:1;			 
			u8 tmds_support:1;			 
			u8 support_reserved:5;
			u8 aux_channel;
			u8 dongle_detect;
		} __packed;
	} __packed;
	u8 pipe_cap:2;
	u8 sdvo_stall:1;					 
	u8 hpd_status:2;
	u8 integrated_encoder:1;
	u8 capabilities_reserved:2;
	u8 dvo_wiring;  
	union {
		u8 dvo2_wiring;
		u8 mipi_bridge_type;				 
	} __packed;
	u16 extended_type;
	u8 dvo_function;
	u8 dp_usb_type_c:1;					 
	u8 tbt:1;						 
	u8 flags2_reserved:2;					 
	u8 dp_port_trace_length:4;				 
	u8 dp_gpio_index;					 
	u16 dp_gpio_pin_num;					 
	u8 dp_iboost_level:4;					 
	u8 hdmi_iboost_level:4;					 
	u8 dp_max_link_rate:3;					 
	u8 dp_max_link_rate_reserved:5;				 
} __packed;
struct bdb_general_definitions {
	u8 crt_ddc_gmbus_pin;
	u8 dpms_non_acpi:1;
	u8 skip_boot_crt_detect:1;
	u8 dpms_aim:1;
	u8 rsvd1:5;  
	u8 boot_display[2];
	u8 child_dev_size;
	u8 devices[];
} __packed;
struct psr_table {
	u8 full_link:1;						 
	u8 require_aux_to_wakeup:1;				 
	u8 feature_bits_rsvd:6;
	u8 idle_frames:4;					 
	u8 lines_to_wait:3;					 
	u8 wait_times_rsvd:1;
	u16 tp1_wakeup_time;					 
	u16 tp2_tp3_wakeup_time;				 
} __packed;
struct bdb_psr {
	struct psr_table psr_table[16];
	u32 psr2_tp2_tp3_wakeup_time;				 
} __packed;
#define BDB_DRIVER_FEATURE_NO_LVDS		0
#define BDB_DRIVER_FEATURE_INT_LVDS		1
#define BDB_DRIVER_FEATURE_SDVO_LVDS		2
#define BDB_DRIVER_FEATURE_INT_SDVO_LVDS	3
struct bdb_driver_features {
	u8 boot_dev_algorithm:1;
	u8 allow_display_switch_dvd:1;
	u8 allow_display_switch_dos:1;
	u8 hotplug_dvo:1;
	u8 dual_view_zoom:1;
	u8 int15h_hook:1;
	u8 sprite_in_clone:1;
	u8 primary_lfp_id:1;
	u16 boot_mode_x;
	u16 boot_mode_y;
	u8 boot_mode_bpp;
	u8 boot_mode_refresh;
	u16 enable_lfp_primary:1;
	u16 selective_mode_pruning:1;
	u16 dual_frequency:1;
	u16 render_clock_freq:1;  
	u16 nt_clone_support:1;
	u16 power_scheme_ui:1;  
	u16 sprite_display_assign:1;  
	u16 cui_aspect_scaling:1;
	u16 preserve_aspect_ratio:1;
	u16 sdvo_device_power_down:1;
	u16 crt_hotplug:1;
	u16 lvds_config:2;
	u16 tv_hotplug:1;
	u16 hdmi_config:2;
	u8 static_display:1;					 
	u8 embedded_platform:1;					 
	u8 display_subsystem_enable:1;				 
	u8 reserved0:5;
	u16 legacy_crt_max_x;
	u16 legacy_crt_max_y;
	u8 legacy_crt_max_refresh;
	u8 hdmi_termination:1;
	u8 cea861d_hdmi_support:1;
	u8 self_refresh_enable:1;
	u8 reserved1:5;
	u8 custom_vbt_version;					 
	u16 rmpm_enabled:1;					 
	u16 s2ddt_enabled:1;					 
	u16 dpst_enabled:1;					 
	u16 bltclt_enabled:1;					 
	u16 adb_enabled:1;					 
	u16 drrs_enabled:1;					 
	u16 grs_enabled:1;					 
	u16 gpmt_enabled:1;					 
	u16 tbt_enabled:1;					 
	u16 psr_enabled:1;					 
	u16 ips_enabled:1;					 
	u16 dpfs_enabled:1;					 
	u16 dmrrs_enabled:1;					 
	u16 adt_enabled:1;					 
	u16 hpd_wake:1;						 
	u16 pc_feature_valid:1;
} __packed;
struct bdb_sdvo_lvds_options {
	u8 panel_backlight;
	u8 h40_set_panel_type;
	u8 panel_type;
	u8 ssc_clk_freq;
	u16 als_low_trip;
	u16 als_high_trip;
	u8 sclalarcoeff_tab_row_num;
	u8 sclalarcoeff_tab_row_size;
	u8 coefficient[8];
	u8 panel_misc_bits_1;
	u8 panel_misc_bits_2;
	u8 panel_misc_bits_3;
	u8 panel_misc_bits_4;
} __packed;
struct lvds_dvo_timing {
	u16 clock;		 
	u8 hactive_lo;
	u8 hblank_lo;
	u8 hblank_hi:4;
	u8 hactive_hi:4;
	u8 vactive_lo;
	u8 vblank_lo;
	u8 vblank_hi:4;
	u8 vactive_hi:4;
	u8 hsync_off_lo;
	u8 hsync_pulse_width_lo;
	u8 vsync_pulse_width_lo:4;
	u8 vsync_off_lo:4;
	u8 vsync_pulse_width_hi:2;
	u8 vsync_off_hi:2;
	u8 hsync_pulse_width_hi:2;
	u8 hsync_off_hi:2;
	u8 himage_lo;
	u8 vimage_lo;
	u8 vimage_hi:4;
	u8 himage_hi:4;
	u8 h_border;
	u8 v_border;
	u8 rsvd1:3;
	u8 digital:2;
	u8 vsync_positive:1;
	u8 hsync_positive:1;
	u8 non_interlaced:1;
} __packed;
struct bdb_sdvo_panel_dtds {
	struct lvds_dvo_timing dtds[4];
} __packed;
#define EDP_18BPP	0
#define EDP_24BPP	1
#define EDP_30BPP	2
#define EDP_RATE_1_62	0
#define EDP_RATE_2_7	1
#define EDP_RATE_5_4	2
#define EDP_LANE_1	0
#define EDP_LANE_2	1
#define EDP_LANE_4	3
#define EDP_PREEMPHASIS_NONE	0
#define EDP_PREEMPHASIS_3_5dB	1
#define EDP_PREEMPHASIS_6dB	2
#define EDP_PREEMPHASIS_9_5dB	3
#define EDP_VSWING_0_4V		0
#define EDP_VSWING_0_6V		1
#define EDP_VSWING_0_8V		2
#define EDP_VSWING_1_2V		3
struct edp_fast_link_params {
	u8 rate:4;						 
	u8 lanes:4;
	u8 preemphasis:4;
	u8 vswing:4;
} __packed;
struct edp_pwm_delays {
	u16 pwm_on_to_backlight_enable;
	u16 backlight_disable_to_pwm_off;
} __packed;
struct edp_full_link_params {
	u8 preemphasis:4;
	u8 vswing:4;
} __packed;
struct edp_apical_params {
	u32 panel_oui;
	u32 dpcd_base_address;
	u32 dpcd_idridix_control_0;
	u32 dpcd_option_select;
	u32 dpcd_backlight;
	u32 ambient_light;
	u32 backlight_scale;
} __packed;
struct bdb_edp {
	struct edp_power_seq power_seqs[16];
	u32 color_depth;
	struct edp_fast_link_params fast_link_params[16];
	u32 sdrrs_msa_timing_delay;
	u16 edp_s3d_feature;					 
	u16 edp_t3_optimization;				 
	u64 edp_vswing_preemph;					 
	u16 fast_link_training;					 
	u16 dpcd_600h_write_required;				 
	struct edp_pwm_delays pwm_delays[16];			 
	u16 full_link_params_provided;				 
	struct edp_full_link_params full_link_params[16];	 
	u16 apical_enable;					 
	struct edp_apical_params apical_params[16];		 
	u16 edp_fast_link_training_rate[16];			 
	u16 edp_max_port_link_rate[16];				 
} __packed;
struct bdb_lvds_options {
	u8 panel_type;
	u8 panel_type2;						 
	u8 pfit_mode:2;
	u8 pfit_text_mode_enhanced:1;
	u8 pfit_gfx_mode_enhanced:1;
	u8 pfit_ratio_auto:1;
	u8 pixel_dither:1;
	u8 lvds_edid:1;						 
	u8 rsvd2:1;
	u8 rsvd4;
	u32 lvds_panel_channel_bits;
	u16 ssc_bits;
	u16 ssc_freq;
	u16 ssc_ddt;
	u16 panel_color_depth;
	u32 dps_panel_type_bits;
	u32 blt_control_type_bits;				 
	u16 lcdvcc_s0_enable;					 
	u32 rotation;						 
	u32 position;						 
} __packed;
struct lvds_lfp_data_ptr_table {
	u16 offset;  
	u8 table_size;
} __packed;
struct lvds_lfp_data_ptr {
	struct lvds_lfp_data_ptr_table fp_timing;
	struct lvds_lfp_data_ptr_table dvo_timing;
	struct lvds_lfp_data_ptr_table panel_pnp_id;
} __packed;
struct bdb_lvds_lfp_data_ptrs {
	u8 lvds_entries;
	struct lvds_lfp_data_ptr ptr[16];
	struct lvds_lfp_data_ptr_table panel_name;		 
} __packed;
struct lvds_fp_timing {
	u16 x_res;
	u16 y_res;
	u32 lvds_reg;
	u32 lvds_reg_val;
	u32 pp_on_reg;
	u32 pp_on_reg_val;
	u32 pp_off_reg;
	u32 pp_off_reg_val;
	u32 pp_cycle_reg;
	u32 pp_cycle_reg_val;
	u32 pfit_reg;
	u32 pfit_reg_val;
	u16 terminator;
} __packed;
struct lvds_pnp_id {
	u16 mfg_name;
	u16 product_code;
	u32 serial;
	u8 mfg_week;
	u8 mfg_year;
} __packed;
struct lvds_lfp_data_entry {
	struct lvds_fp_timing fp_timing;
	struct lvds_dvo_timing dvo_timing;
	struct lvds_pnp_id pnp_id;
} __packed;
struct bdb_lvds_lfp_data {
	struct lvds_lfp_data_entry data[16];
} __packed;
struct lvds_lfp_panel_name {
	u8 name[13];
} __packed;
struct lvds_lfp_black_border {
	u8 top;		 
	u8 bottom;	 
	u8 left;	 
	u8 right;	 
} __packed;
struct bdb_lvds_lfp_data_tail {
	struct lvds_lfp_panel_name panel_name[16];		 
	u16 scaling_enable;					 
	u8 seamless_drrs_min_refresh_rate[16];			 
	u8 pixel_overlap_count[16];				 
	struct lvds_lfp_black_border black_border[16];		 
	u16 dual_lfp_port_sync_enable;				 
	u16 gpu_dithering_for_banding_artifacts;		 
} __packed;
#define BDB_BACKLIGHT_TYPE_NONE	0
#define BDB_BACKLIGHT_TYPE_PWM	2
struct lfp_backlight_data_entry {
	u8 type:2;
	u8 active_low_pwm:1;
	u8 obsolete1:5;
	u16 pwm_freq_hz;
	u8 min_brightness;					 
	u8 obsolete2;
	u8 obsolete3;
} __packed;
struct lfp_backlight_control_method {
	u8 type:4;
	u8 controller:4;
} __packed;
struct lfp_brightness_level {
	u16 level;
	u16 reserved;
} __packed;
#define EXP_BDB_LFP_BL_DATA_SIZE_REV_191 \
	offsetof(struct bdb_lfp_backlight_data, brightness_level)
#define EXP_BDB_LFP_BL_DATA_SIZE_REV_234 \
	offsetof(struct bdb_lfp_backlight_data, brightness_precision_bits)
struct bdb_lfp_backlight_data {
	u8 entry_size;
	struct lfp_backlight_data_entry data[16];
	u8 level[16];							 
	struct lfp_backlight_control_method backlight_control[16];
	struct lfp_brightness_level brightness_level[16];		 
	struct lfp_brightness_level brightness_min_level[16];		 
	u8 brightness_precision_bits[16];				 
	u16 hdr_dpcd_refresh_timeout[16];				 
} __packed;
struct lfp_power_features {
	u8 reserved1:1;
	u8 power_conservation_pref:3;
	u8 reserved2:1;
	u8 lace_enabled_status:1;					 
	u8 lace_support:1;						 
	u8 als_enable:1;
} __packed;
struct als_data_entry {
	u16 backlight_adjust;
	u16 lux;
} __packed;
struct aggressiveness_profile_entry {
	u8 dpst_aggressiveness : 4;
	u8 lace_aggressiveness : 4;
} __packed;
struct aggressiveness_profile2_entry {
	u8 opst_aggressiveness : 4;
	u8 elp_aggressiveness : 4;
} __packed;
struct bdb_lfp_power {
	struct lfp_power_features features;				 
	struct als_data_entry als[5];
	u8 lace_aggressiveness_profile:3;				 
	u8 reserved1:5;
	u16 dpst;							 
	u16 psr;							 
	u16 drrs;							 
	u16 lace_support;						 
	u16 adt;							 
	u16 dmrrs;							 
	u16 adb;							 
	u16 lace_enabled_status;					 
	struct aggressiveness_profile_entry aggressiveness[16];		 
	u16 hobl;							 
	u16 vrr_feature_enabled;					 
	u16 elp;							 
	u16 opst;							 
	struct aggressiveness_profile2_entry aggressiveness2[16];	 
} __packed;
#define MAX_MIPI_CONFIGURATIONS	6
struct bdb_mipi_config {
	struct mipi_config config[MAX_MIPI_CONFIGURATIONS];		 
	struct mipi_pps_data pps[MAX_MIPI_CONFIGURATIONS];		 
	struct edp_pwm_delays pwm_delays[MAX_MIPI_CONFIGURATIONS];	 
	u8 pmic_i2c_bus_number[MAX_MIPI_CONFIGURATIONS];		 
} __packed;
struct bdb_mipi_sequence {
	u8 version;
	u8 data[];  
} __packed;
#define VBT_RC_BUFFER_BLOCK_SIZE_1KB	0
#define VBT_RC_BUFFER_BLOCK_SIZE_4KB	1
#define VBT_RC_BUFFER_BLOCK_SIZE_16KB	2
#define VBT_RC_BUFFER_BLOCK_SIZE_64KB	3
#define VBT_DSC_LINE_BUFFER_DEPTH(vbt_value)	((vbt_value) + 8)  
#define VBT_DSC_MAX_BPP(vbt_value)		(6 + (vbt_value) * 2)
struct dsc_compression_parameters_entry {
	u8 version_major:4;
	u8 version_minor:4;
	u8 rc_buffer_block_size:2;
	u8 reserved1:6;
	u8 rc_buffer_size;
	u32 slices_per_line;
	u8 line_buffer_depth:4;
	u8 reserved2:4;
	u8 block_prediction_enable:1;
	u8 reserved3:7;
	u8 max_bpp;  
	u8 reserved4:1;
	u8 support_8bpc:1;
	u8 support_10bpc:1;
	u8 support_12bpc:1;
	u8 reserved5:4;
	u16 slice_height;
} __packed;
struct bdb_compression_parameters {
	u16 entry_size;
	struct dsc_compression_parameters_entry data[16];
} __packed;
struct generic_dtd_entry {
	u32 pixel_clock;
	u16 hactive;
	u16 hblank;
	u16 hfront_porch;
	u16 hsync;
	u16 vactive;
	u16 vblank;
	u16 vfront_porch;
	u16 vsync;
	u16 width_mm;
	u16 height_mm;
	u8 rsvd_flags:6;
	u8 vsync_positive_polarity:1;
	u8 hsync_positive_polarity:1;
	u8 rsvd[3];
} __packed;
struct bdb_generic_dtd {
	u16 gdtd_size;
	struct generic_dtd_entry dtd[];	 
} __packed;
#endif  
