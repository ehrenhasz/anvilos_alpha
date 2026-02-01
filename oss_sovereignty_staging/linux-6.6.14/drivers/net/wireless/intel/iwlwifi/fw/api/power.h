 
 
#ifndef __iwl_fw_api_power_h__
#define __iwl_fw_api_power_h__

 

 
enum iwl_ltr_config_flags {
	LTR_CFG_FLAG_FEATURE_ENABLE = BIT(0),
	LTR_CFG_FLAG_HW_DIS_ON_SHADOW_REG_ACCESS = BIT(1),
	LTR_CFG_FLAG_HW_EN_SHRT_WR_THROUGH = BIT(2),
	LTR_CFG_FLAG_HW_DIS_ON_D0_2_D3 = BIT(3),
	LTR_CFG_FLAG_SW_SET_SHORT = BIT(4),
	LTR_CFG_FLAG_SW_SET_LONG = BIT(5),
	LTR_CFG_FLAG_DENIE_C10_ON_PD = BIT(6),
	LTR_CFG_FLAG_UPDATE_VALUES = BIT(7),
};

 
struct iwl_ltr_config_cmd_v1 {
	__le32 flags;
	__le32 static_long;
	__le32 static_short;
} __packed;  

#define LTR_VALID_STATES_NUM 4

 
struct iwl_ltr_config_cmd {
	__le32 flags;
	__le32 static_long;
	__le32 static_short;
	__le32 ltr_cfg_values[LTR_VALID_STATES_NUM];
	__le32 ltr_short_idle_timeout;
} __packed;  

 
#define POWER_LPRX_RSSI_THRESHOLD	75
#define POWER_LPRX_RSSI_THRESHOLD_MAX	94
#define POWER_LPRX_RSSI_THRESHOLD_MIN	30

 
enum iwl_power_flags {
	POWER_FLAGS_POWER_SAVE_ENA_MSK		= BIT(0),
	POWER_FLAGS_POWER_MANAGEMENT_ENA_MSK	= BIT(1),
	POWER_FLAGS_SKIP_OVER_DTIM_MSK		= BIT(2),
	POWER_FLAGS_SNOOZE_ENA_MSK		= BIT(5),
	POWER_FLAGS_BT_SCO_ENA			= BIT(8),
	POWER_FLAGS_ADVANCE_PM_ENA_MSK		= BIT(9),
	POWER_FLAGS_LPRX_ENA_MSK		= BIT(11),
	POWER_FLAGS_UAPSD_MISBEHAVING_ENA_MSK	= BIT(12),
};

#define IWL_POWER_VEC_SIZE 5

 
struct iwl_powertable_cmd {
	 
	__le16 flags;
	u8 keep_alive_seconds;
	u8 debug_flags;
	__le32 rx_data_timeout;
	__le32 tx_data_timeout;
	__le32 sleep_interval[IWL_POWER_VEC_SIZE];
	__le32 skip_dtim_periods;
	__le32 lprx_rssi_threshold;
} __packed;

 
enum iwl_device_power_flags {
	DEVICE_POWER_FLAGS_POWER_SAVE_ENA_MSK		= BIT(0),
	DEVICE_POWER_FLAGS_ALLOW_MEM_RETENTION_MSK	= BIT(1),
	DEVICE_POWER_FLAGS_32K_CLK_VALID_MSK		= BIT(12),
};

 
struct iwl_device_power_cmd {
	 
	__le16 flags;
	__le16 reserved;
} __packed;

 
struct iwl_mac_power_cmd {
	 
	__le32 id_and_color;

	 
	__le16 flags;
	__le16 keep_alive_seconds;
	__le32 rx_data_timeout;
	__le32 tx_data_timeout;
	__le32 rx_data_timeout_uapsd;
	__le32 tx_data_timeout_uapsd;
	u8 lprx_rssi_threshold;
	u8 skip_dtim_periods;
	__le16 snooze_interval;
	__le16 snooze_window;
	u8 snooze_step;
	u8 qndp_tid;
	u8 uapsd_ac_flags;
	u8 uapsd_max_sp;
	u8 heavy_tx_thld_packets;
	u8 heavy_rx_thld_packets;
	u8 heavy_tx_thld_percentage;
	u8 heavy_rx_thld_percentage;
	u8 limited_ps_threshold;
	u8 reserved;
} __packed;

 
struct iwl_uapsd_misbehaving_ap_notif {
	__le32 sta_id;
	u8 mac_id;
	u8 reserved[3];
} __packed;

 
struct iwl_reduce_tx_power_cmd {
	u8 flags;
	u8 mac_context_id;
	__le16 pwr_restriction;
} __packed;  

enum iwl_dev_tx_power_cmd_mode {
	IWL_TX_POWER_MODE_SET_MAC = 0,
	IWL_TX_POWER_MODE_SET_DEVICE = 1,
	IWL_TX_POWER_MODE_SET_CHAINS = 2,
	IWL_TX_POWER_MODE_SET_ACK = 3,
	IWL_TX_POWER_MODE_SET_SAR_TIMER = 4,
	IWL_TX_POWER_MODE_SET_SAR_TIMER_DEFAULT_TABLE = 5,
};  ;

#define IWL_NUM_CHAIN_TABLES	1
#define IWL_NUM_CHAIN_TABLES_V2	2
#define IWL_NUM_CHAIN_LIMITS	2
#define IWL_NUM_SUB_BANDS_V1	5
#define IWL_NUM_SUB_BANDS_V2	11

 
struct iwl_dev_tx_power_common {
	__le32 set_mode;
	__le32 mac_context_id;
	__le16 pwr_restriction;
	__le16 dev_24;
	__le16 dev_52_low;
	__le16 dev_52_high;
};

 
struct iwl_dev_tx_power_cmd_v3 {
	__le16 per_chain[IWL_NUM_CHAIN_TABLES][IWL_NUM_CHAIN_LIMITS][IWL_NUM_SUB_BANDS_V1];
} __packed;  

#define IWL_DEV_MAX_TX_POWER 0x7FFF

 
struct iwl_dev_tx_power_cmd_v4 {
	__le16 per_chain[IWL_NUM_CHAIN_TABLES][IWL_NUM_CHAIN_LIMITS][IWL_NUM_SUB_BANDS_V1];
	u8 enable_ack_reduction;
	u8 reserved[3];
} __packed;  

 
struct iwl_dev_tx_power_cmd_v5 {
	__le16 per_chain[IWL_NUM_CHAIN_TABLES][IWL_NUM_CHAIN_LIMITS][IWL_NUM_SUB_BANDS_V1];
	u8 enable_ack_reduction;
	u8 per_chain_restriction_changed;
	u8 reserved[2];
	__le32 timer_period;
} __packed;  

 
struct iwl_dev_tx_power_cmd_v6 {
	__le16 per_chain[IWL_NUM_CHAIN_TABLES_V2][IWL_NUM_CHAIN_LIMITS][IWL_NUM_SUB_BANDS_V2];
	u8 enable_ack_reduction;
	u8 per_chain_restriction_changed;
	u8 reserved[2];
	__le32 timer_period;
} __packed;  

 
struct iwl_dev_tx_power_cmd_v7 {
	__le16 per_chain[IWL_NUM_CHAIN_TABLES_V2][IWL_NUM_CHAIN_LIMITS][IWL_NUM_SUB_BANDS_V2];
	u8 enable_ack_reduction;
	u8 per_chain_restriction_changed;
	u8 reserved[2];
	__le32 timer_period;
	__le32 flags;
} __packed;  
 
struct iwl_dev_tx_power_cmd {
	struct iwl_dev_tx_power_common common;
	union {
		struct iwl_dev_tx_power_cmd_v3 v3;
		struct iwl_dev_tx_power_cmd_v4 v4;
		struct iwl_dev_tx_power_cmd_v5 v5;
		struct iwl_dev_tx_power_cmd_v6 v6;
		struct iwl_dev_tx_power_cmd_v7 v7;
	};
};

#define IWL_NUM_GEO_PROFILES		3
#define IWL_NUM_GEO_PROFILES_V3		8
#define IWL_NUM_BANDS_PER_CHAIN_V1	2
#define IWL_NUM_BANDS_PER_CHAIN_V2	3

 
enum iwl_geo_per_chain_offset_operation {
	IWL_PER_CHAIN_OFFSET_SET_TABLES,
	IWL_PER_CHAIN_OFFSET_GET_CURRENT_TABLE,
};   

 
struct iwl_per_chain_offset {
	__le16 max_tx_power;
	u8 chain_a;
	u8 chain_b;
} __packed;  

 
struct iwl_geo_tx_power_profiles_cmd_v1 {
	__le32 ops;
	struct iwl_per_chain_offset table[IWL_NUM_GEO_PROFILES][IWL_NUM_BANDS_PER_CHAIN_V1];
} __packed;  

 
struct iwl_geo_tx_power_profiles_cmd_v2 {
	__le32 ops;
	struct iwl_per_chain_offset table[IWL_NUM_GEO_PROFILES][IWL_NUM_BANDS_PER_CHAIN_V1];
	__le32 table_revision;
} __packed;  

 
struct iwl_geo_tx_power_profiles_cmd_v3 {
	__le32 ops;
	struct iwl_per_chain_offset table[IWL_NUM_GEO_PROFILES][IWL_NUM_BANDS_PER_CHAIN_V2];
	__le32 table_revision;
} __packed;  

 
struct iwl_geo_tx_power_profiles_cmd_v4 {
	__le32 ops;
	struct iwl_per_chain_offset table[IWL_NUM_GEO_PROFILES_V3][IWL_NUM_BANDS_PER_CHAIN_V1];
	__le32 table_revision;
} __packed;  

 
struct iwl_geo_tx_power_profiles_cmd_v5 {
	__le32 ops;
	struct iwl_per_chain_offset table[IWL_NUM_GEO_PROFILES_V3][IWL_NUM_BANDS_PER_CHAIN_V2];
	__le32 table_revision;
} __packed;  

union iwl_geo_tx_power_profiles_cmd {
	struct iwl_geo_tx_power_profiles_cmd_v1 v1;
	struct iwl_geo_tx_power_profiles_cmd_v2 v2;
	struct iwl_geo_tx_power_profiles_cmd_v3 v3;
	struct iwl_geo_tx_power_profiles_cmd_v4 v4;
	struct iwl_geo_tx_power_profiles_cmd_v5 v5;
};

 
struct iwl_geo_tx_power_profiles_resp {
	__le32 profile_idx;
} __packed;  

 
union iwl_ppag_table_cmd {
	struct {
		__le32 flags;
		s8 gain[IWL_NUM_CHAIN_LIMITS][IWL_NUM_SUB_BANDS_V1];
		s8 reserved[2];
	} v1;
	struct {
		__le32 flags;
		s8 gain[IWL_NUM_CHAIN_LIMITS][IWL_NUM_SUB_BANDS_V2];
		s8 reserved[2];
	} v2;
} __packed;

#define MCC_TO_SAR_OFFSET_TABLE_ROW_SIZE	26
#define MCC_TO_SAR_OFFSET_TABLE_COL_SIZE	13

 
struct iwl_sar_offset_mapping_cmd {
	u8 offset_map[MCC_TO_SAR_OFFSET_TABLE_ROW_SIZE]
		[MCC_TO_SAR_OFFSET_TABLE_COL_SIZE];
	__le16 reserved;
} __packed;  

 
struct iwl_beacon_filter_cmd {
	__le32 bf_energy_delta;
	__le32 bf_roaming_energy_delta;
	__le32 bf_roaming_state;
	__le32 bf_temp_threshold;
	__le32 bf_temp_fast_filter;
	__le32 bf_temp_slow_filter;
	__le32 bf_enable_beacon_filter;
	__le32 bf_debug_flag;
	__le32 bf_escape_timer;
	__le32 ba_escape_timer;
	__le32 ba_enable_beacon_abort;
	__le32 bf_threshold_absolute_low[2];
	__le32 bf_threshold_absolute_high[2];
} __packed;  

 
#define IWL_BF_ENERGY_DELTA_DEFAULT 5
#define IWL_BF_ENERGY_DELTA_D0I3 20
#define IWL_BF_ENERGY_DELTA_MAX 255
#define IWL_BF_ENERGY_DELTA_MIN 0

#define IWL_BF_ROAMING_ENERGY_DELTA_DEFAULT 1
#define IWL_BF_ROAMING_ENERGY_DELTA_D0I3 20
#define IWL_BF_ROAMING_ENERGY_DELTA_MAX 255
#define IWL_BF_ROAMING_ENERGY_DELTA_MIN 0

#define IWL_BF_ROAMING_STATE_DEFAULT 72
#define IWL_BF_ROAMING_STATE_D0I3 72
#define IWL_BF_ROAMING_STATE_MAX 255
#define IWL_BF_ROAMING_STATE_MIN 0

#define IWL_BF_TEMP_THRESHOLD_DEFAULT 112
#define IWL_BF_TEMP_THRESHOLD_D0I3 112
#define IWL_BF_TEMP_THRESHOLD_MAX 255
#define IWL_BF_TEMP_THRESHOLD_MIN 0

#define IWL_BF_TEMP_FAST_FILTER_DEFAULT 1
#define IWL_BF_TEMP_FAST_FILTER_D0I3 1
#define IWL_BF_TEMP_FAST_FILTER_MAX 255
#define IWL_BF_TEMP_FAST_FILTER_MIN 0

#define IWL_BF_TEMP_SLOW_FILTER_DEFAULT 5
#define IWL_BF_TEMP_SLOW_FILTER_D0I3 20
#define IWL_BF_TEMP_SLOW_FILTER_MAX 255
#define IWL_BF_TEMP_SLOW_FILTER_MIN 0

#define IWL_BF_ENABLE_BEACON_FILTER_DEFAULT 1

#define IWL_BF_DEBUG_FLAG_DEFAULT 0
#define IWL_BF_DEBUG_FLAG_D0I3 0

#define IWL_BF_ESCAPE_TIMER_DEFAULT 0
#define IWL_BF_ESCAPE_TIMER_D0I3 0
#define IWL_BF_ESCAPE_TIMER_MAX 1024
#define IWL_BF_ESCAPE_TIMER_MIN 0

#define IWL_BA_ESCAPE_TIMER_DEFAULT 6
#define IWL_BA_ESCAPE_TIMER_D0I3 6
#define IWL_BA_ESCAPE_TIMER_D3 9
#define IWL_BA_ESCAPE_TIMER_MAX 1024
#define IWL_BA_ESCAPE_TIMER_MIN 0

#define IWL_BA_ENABLE_BEACON_ABORT_DEFAULT 1

#define IWL_BF_CMD_CONFIG(mode)					     \
	.bf_energy_delta = cpu_to_le32(IWL_BF_ENERGY_DELTA ## mode),	      \
	.bf_roaming_energy_delta =					      \
		cpu_to_le32(IWL_BF_ROAMING_ENERGY_DELTA ## mode),	      \
	.bf_roaming_state = cpu_to_le32(IWL_BF_ROAMING_STATE ## mode),	      \
	.bf_temp_threshold = cpu_to_le32(IWL_BF_TEMP_THRESHOLD ## mode),      \
	.bf_temp_fast_filter = cpu_to_le32(IWL_BF_TEMP_FAST_FILTER ## mode),  \
	.bf_temp_slow_filter = cpu_to_le32(IWL_BF_TEMP_SLOW_FILTER ## mode),  \
	.bf_debug_flag = cpu_to_le32(IWL_BF_DEBUG_FLAG ## mode),	      \
	.bf_escape_timer = cpu_to_le32(IWL_BF_ESCAPE_TIMER ## mode),	      \
	.ba_escape_timer = cpu_to_le32(IWL_BA_ESCAPE_TIMER ## mode)

#define IWL_BF_CMD_CONFIG_DEFAULTS IWL_BF_CMD_CONFIG(_DEFAULT)
#define IWL_BF_CMD_CONFIG_D0I3 IWL_BF_CMD_CONFIG(_D0I3)
#endif  
