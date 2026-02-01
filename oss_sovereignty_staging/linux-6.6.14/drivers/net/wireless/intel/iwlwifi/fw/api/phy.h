 
 
#ifndef __iwl_fw_api_phy_h__
#define __iwl_fw_api_phy_h__

 
enum iwl_phy_ops_subcmd_ids {
	 
	CMD_DTS_MEASUREMENT_TRIGGER_WIDE = 0x0,

	 
	CTDP_CONFIG_CMD = 0x03,

	 
	TEMP_REPORTING_THRESHOLDS_CMD = 0x04,

	 
	PER_CHAIN_LIMIT_OFFSET_CMD = 0x05,

	 
	PER_PLATFORM_ANT_GAIN_CMD = 0x07,

	 
	CT_KILL_NOTIFICATION = 0xFE,

	 
	DTS_MEASUREMENT_NOTIF_WIDE = 0xFF,
};

 

enum iwl_dts_measurement_flags {
	DTS_TRIGGER_CMD_FLAGS_TEMP	= BIT(0),
	DTS_TRIGGER_CMD_FLAGS_VOLT	= BIT(1),
};

 
struct iwl_dts_measurement_cmd {
	__le32 flags;
} __packed;  

 
enum iwl_dts_control_measurement_mode {
	DTS_AUTOMATIC			= 0,
	DTS_REQUEST_READ		= 1,
	DTS_OVER_WRITE			= 2,
	DTS_DIRECT_WITHOUT_MEASURE	= 3,
};

 
enum iwl_dts_used {
	DTS_USE_TOP		= 0,
	DTS_USE_CHAIN_A		= 1,
	DTS_USE_CHAIN_B		= 2,
	DTS_USE_CHAIN_C		= 3,
	XTAL_TEMPERATURE	= 4,
};

 
enum iwl_dts_bit_mode {
	DTS_BIT6_MODE	= 0,
	DTS_BIT8_MODE	= 1,
};

 
struct iwl_ext_dts_measurement_cmd {
	__le32 control_mode;
	__le32 temperature;
	__le32 sensor;
	__le32 avg_factor;
	__le32 bit_mode;
	__le32 step_duration;
} __packed;  

 
struct iwl_dts_measurement_notif_v1 {
	__le32 temp;
	__le32 voltage;
} __packed;  

 
struct iwl_dts_measurement_notif_v2 {
	__le32 temp;
	__le32 voltage;
	__le32 threshold_idx;
} __packed;  

 
struct iwl_dts_measurement_resp {
	__le32 temp;
} __packed;  

 
struct ct_kill_notif {
	__le16 temperature;
	u8 dts;
	u8 scheme;
} __packed;  

 
enum iwl_mvm_ctdp_cmd_operation {
	CTDP_CMD_OPERATION_START	= 0x1,
	CTDP_CMD_OPERATION_STOP		= 0x2,
	CTDP_CMD_OPERATION_REPORT	= 0x4,
}; 

 
struct iwl_mvm_ctdp_cmd {
	__le32 operation;
	__le32 budget;
	__le32 window_size;
} __packed;

#define IWL_MAX_DTS_TRIPS	8

 
struct temp_report_ths_cmd {
	__le32 num_temps;
	__le16 thresholds[IWL_MAX_DTS_TRIPS];
} __packed;  

#endif  
