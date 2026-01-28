#ifndef AMD_ACPI_H
#define AMD_ACPI_H
#define ACPI_AC_CLASS           "ac_adapter"
struct atif_verify_interface {
	u16 size;		 
	u16 version;		 
	u32 notification_mask;	 
	u32 function_bits;	 
} __packed;
struct atif_system_params {
	u16 size;		 
	u32 valid_mask;		 
	u32 flags;		 
	u8 command_code;	 
} __packed;
struct atif_sbios_requests {
	u16 size;		 
	u32 pending;		 
	u8 panel_exp_mode;	 
	u8 thermal_gfx;		 
	u8 thermal_state;	 
	u8 forced_power_gfx;	 
	u8 forced_power_state;	 
	u8 system_power_src;	 
	u8 backlight_level;	 
} __packed;
struct atif_qbtc_arguments {
	u16 size;		 
	u8 requested_display;	 
} __packed;
#define ATIF_QBTC_MAX_DATA_POINTS 99
struct atif_qbtc_data_point {
	u8 luminance;		 
	u8 ipnut_signal;	 
} __packed;
struct atif_qbtc_output {
	u16 size;		 
	u16 flags;		 
	u8 error_code;		 
	u8 ac_level;		 
	u8 dc_level;		 
	u8 min_input_signal;	 
	u8 max_input_signal;	 
	u8 number_of_points;	 
	struct atif_qbtc_data_point data_points[ATIF_QBTC_MAX_DATA_POINTS];
} __packed;
#define ATIF_NOTIFY_MASK	0x3
#define ATIF_NOTIFY_NONE	0
#define ATIF_NOTIFY_81		1
#define ATIF_NOTIFY_N		2
struct atcs_verify_interface {
	u16 size;		 
	u16 version;		 
	u32 function_bits;	 
} __packed;
#define ATCS_VALID_FLAGS_MASK	0x3
struct atcs_pref_req_input {
	u16 size;		 
	u16 client_id;		 
	u16 valid_flags_mask;	 
	u16 flags;		 
	u8 req_type;		 
	u8 perf_req;		 
} __packed;
struct atcs_pref_req_output {
	u16 size;		 
	u8 ret_val;		 
} __packed;
struct atcs_pwr_shift_input {
	u16 size;		 
	u16 dgpu_id;		 
	u8 dev_acpi_state;	 
	u8 drv_state;	 
} __packed;
#define ATIF_FUNCTION_VERIFY_INTERFACE                             0x0
#       define ATIF_THERMAL_STATE_CHANGE_REQUEST_SUPPORTED         (1 << 2)
#       define ATIF_FORCED_POWER_STATE_CHANGE_REQUEST_SUPPORTED    (1 << 3)
#       define ATIF_SYSTEM_POWER_SOURCE_CHANGE_REQUEST_SUPPORTED   (1 << 4)
#       define ATIF_PANEL_BRIGHTNESS_CHANGE_REQUEST_SUPPORTED      (1 << 7)
#       define ATIF_DGPU_DISPLAY_EVENT_SUPPORTED                   (1 << 8)
#       define ATIF_GPU_PACKAGE_POWER_LIMIT_REQUEST_SUPPORTED      (1 << 12)
#       define ATIF_GET_SYSTEM_PARAMETERS_SUPPORTED               (1 << 0)
#       define ATIF_GET_SYSTEM_BIOS_REQUESTS_SUPPORTED            (1 << 1)
#       define ATIF_TEMPERATURE_CHANGE_NOTIFICATION_SUPPORTED     (1 << 12)
#       define ATIF_QUERY_BACKLIGHT_TRANSFER_CHARACTERISTICS_SUPPORTED (1 << 15)
#       define ATIF_READY_TO_UNDOCK_NOTIFICATION_SUPPORTED        (1 << 16)
#       define ATIF_GET_EXTERNAL_GPU_INFORMATION_SUPPORTED        (1 << 20)
#define ATIF_FUNCTION_GET_SYSTEM_PARAMETERS                        0x1
#define ATIF_FUNCTION_GET_SYSTEM_BIOS_REQUESTS                     0x2
#       define ATIF_THERMAL_STATE_CHANGE_REQUEST                   (1 << 2)
#       define ATIF_FORCED_POWER_STATE_CHANGE_REQUEST              (1 << 3)
#       define ATIF_SYSTEM_POWER_SOURCE_CHANGE_REQUEST             (1 << 4)
#       define ATIF_PANEL_BRIGHTNESS_CHANGE_REQUEST                (1 << 7)
#       define ATIF_DGPU_DISPLAY_EVENT                             (1 << 8)
#       define ATIF_GPU_PACKAGE_POWER_LIMIT_REQUEST                (1 << 12)
#       define ATIF_TARGET_GFX_SINGLE                              0
#       define ATIF_TARGET_GFX_PX_IGPU                             1
#       define ATIF_TARGET_GFX_PX_DGPU                             2
#       define ATIF_POWER_SOURCE_AC                                1
#       define ATIF_POWER_SOURCE_DC                                2
#       define ATIF_POWER_SOURCE_RESTRICTED_AC_1                   3
#       define ATIF_POWER_SOURCE_RESTRICTED_AC_2                   4
#define ATIF_FUNCTION_TEMPERATURE_CHANGE_NOTIFICATION              0xD
#define ATIF_FUNCTION_QUERY_BRIGHTNESS_TRANSFER_CHARACTERISTICS    0x10
#       define ATIF_QBTC_REQUEST_LCD1                              0
#       define ATIF_QBTC_REQUEST_CRT1                              1
#       define ATIF_QBTC_REQUEST_DFP1                              3
#       define ATIF_QBTC_REQUEST_CRT2                              4
#       define ATIF_QBTC_REQUEST_LCD2                              5
#       define ATIF_QBTC_REQUEST_DFP2                              7
#       define ATIF_QBTC_REQUEST_DFP3                              9
#       define ATIF_QBTC_REQUEST_DFP4                              10
#       define ATIF_QBTC_REQUEST_DFP5                              11
#       define ATIF_QBTC_REQUEST_DFP6                              12
#       define ATIF_QBTC_ERROR_CODE_SUCCESS                        0
#       define ATIF_QBTC_ERROR_CODE_FAILURE                        1
#       define ATIF_QBTC_ERROR_CODE_DEVICE_NOT_SUPPORTED           2
#define ATIF_FUNCTION_READY_TO_UNDOCK_NOTIFICATION                 0x11
#define ATIF_FUNCTION_GET_EXTERNAL_GPU_INFORMATION                 0x15
#       define ATIF_EXTERNAL_GRAPHICS_PORT                         (1 << 0)
#define ATPX_FUNCTION_VERIFY_INTERFACE                             0x0
#       define ATPX_GET_PX_PARAMETERS_SUPPORTED                    (1 << 0)
#       define ATPX_POWER_CONTROL_SUPPORTED                        (1 << 1)
#       define ATPX_DISPLAY_MUX_CONTROL_SUPPORTED                  (1 << 2)
#       define ATPX_I2C_MUX_CONTROL_SUPPORTED                      (1 << 3)
#       define ATPX_GRAPHICS_DEVICE_SWITCH_START_NOTIFICATION_SUPPORTED (1 << 4)
#       define ATPX_GRAPHICS_DEVICE_SWITCH_END_NOTIFICATION_SUPPORTED   (1 << 5)
#       define ATPX_GET_DISPLAY_CONNECTORS_MAPPING_SUPPORTED       (1 << 7)
#       define ATPX_GET_DISPLAY_DETECTION_PORTS_SUPPORTED          (1 << 8)
#define ATPX_FUNCTION_GET_PX_PARAMETERS                            0x1
#       define ATPX_LVDS_I2C_AVAILABLE_TO_BOTH_GPUS                (1 << 0)
#       define ATPX_CRT1_I2C_AVAILABLE_TO_BOTH_GPUS                (1 << 1)
#       define ATPX_DVI1_I2C_AVAILABLE_TO_BOTH_GPUS                (1 << 2)
#       define ATPX_CRT1_RGB_SIGNAL_MUXED                          (1 << 3)
#       define ATPX_TV_SIGNAL_MUXED                                (1 << 4)
#       define ATPX_DFP_SIGNAL_MUXED                               (1 << 5)
#       define ATPX_SEPARATE_MUX_FOR_I2C                           (1 << 6)
#       define ATPX_DYNAMIC_PX_SUPPORTED                           (1 << 7)
#       define ATPX_ACF_NOT_SUPPORTED                              (1 << 8)
#       define ATPX_FIXED_NOT_SUPPORTED                            (1 << 9)
#       define ATPX_DYNAMIC_DGPU_POWER_OFF_SUPPORTED               (1 << 10)
#       define ATPX_DGPU_REQ_POWER_FOR_DISPLAYS                    (1 << 11)
#       define ATPX_DGPU_CAN_DRIVE_DISPLAYS                        (1 << 12)
#       define ATPX_MS_HYBRID_GFX_SUPPORTED                        (1 << 14)
#define ATPX_FUNCTION_POWER_CONTROL                                0x2
#define ATPX_FUNCTION_DISPLAY_MUX_CONTROL                          0x3
#       define ATPX_INTEGRATED_GPU                                 0
#       define ATPX_DISCRETE_GPU                                   1
#define ATPX_FUNCTION_I2C_MUX_CONTROL                              0x4
#define ATPX_FUNCTION_GRAPHICS_DEVICE_SWITCH_START_NOTIFICATION    0x5
#define ATPX_FUNCTION_GRAPHICS_DEVICE_SWITCH_END_NOTIFICATION      0x6
#define ATPX_FUNCTION_GET_DISPLAY_CONNECTORS_MAPPING               0x8
#       define ATPX_DISPLAY_OUTPUT_SUPPORTED_BY_ADAPTER_ID_DEVICE  (1 << 0)
#       define ATPX_DISPLAY_HPD_SUPPORTED_BY_ADAPTER_ID_DEVICE     (1 << 1)
#       define ATPX_DISPLAY_I2C_SUPPORTED_BY_ADAPTER_ID_DEVICE     (1 << 2)
#define ATPX_FUNCTION_GET_DISPLAY_DETECTION_PORTS                  0x9
#       define ATPX_HPD_NONE                                       0
#       define ATPX_HPD1                                           1
#       define ATPX_HPD2                                           2
#       define ATPX_HPD3                                           3
#       define ATPX_HPD4                                           4
#       define ATPX_HPD5                                           5
#       define ATPX_HPD6                                           6
#       define ATPX_DDC_NONE                                       0
#       define ATPX_DDC1                                           1
#       define ATPX_DDC2                                           2
#       define ATPX_DDC3                                           3
#       define ATPX_DDC4                                           4
#       define ATPX_DDC5                                           5
#       define ATPX_DDC6                                           6
#       define ATPX_DDC7                                           7
#       define ATPX_DDC8                                           8
#define ATCS_FUNCTION_VERIFY_INTERFACE                             0x0
#       define ATCS_GET_EXTERNAL_STATE_SUPPORTED                   (1 << 0)
#       define ATCS_PCIE_PERFORMANCE_REQUEST_SUPPORTED             (1 << 1)
#       define ATCS_PCIE_DEVICE_READY_NOTIFICATION_SUPPORTED       (1 << 2)
#       define ATCS_SET_PCIE_BUS_WIDTH_SUPPORTED                   (1 << 3)
#       define ATCS_SET_POWER_SHIFT_CONTROL_SUPPORTED		   (1 << 7)
#define ATCS_FUNCTION_GET_EXTERNAL_STATE                           0x1
#       define ATCS_DOCKED                                         (1 << 0)
#define ATCS_FUNCTION_PCIE_PERFORMANCE_REQUEST                     0x2
#       define ATCS_ADVERTISE_CAPS                                 (1 << 0)
#       define ATCS_WAIT_FOR_COMPLETION                            (1 << 1)
#       define ATCS_PCIE_LINK_SPEED                                1
#       define ATCS_REMOVE                                         0
#       define ATCS_FORCE_LOW_POWER                                1
#       define ATCS_PERF_LEVEL_1                                   2  
#       define ATCS_PERF_LEVEL_2                                   3  
#       define ATCS_PERF_LEVEL_3                                   4  
#       define ATCS_REQUEST_REFUSED                                1
#       define ATCS_REQUEST_COMPLETE                               2
#       define ATCS_REQUEST_IN_PROGRESS                            3
#define ATCS_FUNCTION_PCIE_DEVICE_READY_NOTIFICATION               0x3
#define ATCS_FUNCTION_SET_PCIE_BUS_WIDTH                           0x4
#define ATCS_FUNCTION_POWER_SHIFT_CONTROL                          0x8
#endif
