 
 
#ifndef __iwl_fw_api_datapath_h__
#define __iwl_fw_api_datapath_h__

 
enum iwl_data_path_subcmd_ids {
	 
	DQA_ENABLE_CMD = 0x0,

	 
	UPDATE_MU_GROUPS_CMD = 0x1,

	 
	TRIGGER_RX_QUEUES_NOTIF_CMD = 0x2,

	 
	WNM_PLATFORM_PTM_REQUEST_CMD = 0x3,

	 
	WNM_80211V_TIMING_MEASUREMENT_CONFIG_CMD = 0x4,

	 
	STA_HE_CTXT_CMD = 0x7,

	 
	RLC_CONFIG_CMD = 0x8,

	 
	RFH_QUEUE_CONFIG_CMD = 0xD,

	 
	TLC_MNG_CONFIG_CMD = 0xF,

	 
	HE_AIR_SNIFFER_CONFIG_CMD = 0x13,

	 
	CHEST_COLLECTOR_FILTER_CONFIG_CMD = 0x14,

	 
	RX_BAID_ALLOCATION_CONFIG_CMD = 0x16,

	 
	SCD_QUEUE_CONFIG_CMD = 0x17,

	 
	SEC_KEY_CMD = 0x18,

	 
	MONITOR_NOTIF = 0xF4,

	 
	RX_NO_DATA_NOTIF = 0xF5,

	 
	THERMAL_DUAL_CHAIN_REQUEST = 0xF6,

	 
	TLC_MNG_UPDATE_NOTIF = 0xF7,

	 
	STA_PM_NOTIF = 0xFD,

	 
	MU_GROUP_MGMT_NOTIF = 0xFE,

	 
	RX_QUEUES_NOTIFICATION = 0xFF,
};

 
struct iwl_mu_group_mgmt_cmd {
	__le32 reserved;
	__le32 membership_status[2];
	__le32 user_position[4];
} __packed;  

 
struct iwl_mu_group_mgmt_notif {
	__le32 membership_status[2];
	__le32 user_position[4];
} __packed;  

enum iwl_channel_estimation_flags {
	IWL_CHANNEL_ESTIMATION_ENABLE	= BIT(0),
	IWL_CHANNEL_ESTIMATION_TIMER	= BIT(1),
	IWL_CHANNEL_ESTIMATION_COUNTER	= BIT(2),
};

enum iwl_time_sync_protocol_type {
	IWL_TIME_SYNC_PROTOCOL_TM	= BIT(0),
	IWL_TIME_SYNC_PROTOCOL_FTM	= BIT(1),
};  

 
struct iwl_time_sync_cfg_cmd {
	__le32 protocols;
	u8 peer_addr[ETH_ALEN];
	u8 reserved[2];
} __packed;  

 
enum iwl_synced_time_operation {
	IWL_SYNCED_TIME_OPERATION_READ_ARTB = 1,
	IWL_SYNCED_TIME_OPERATION_READ_GP2,
	IWL_SYNCED_TIME_OPERATION_READ_BOTH,
};

 
struct iwl_synced_time_cmd {
	__le32 operation;
} __packed;  

 
struct iwl_synced_time_rsp {
	__le32 operation;
	__le32 platform_timestamp_hi;
	__le32 platform_timestamp_lo;
	__le32 gp2_timestamp_hi;
	__le32 gp2_timestamp_lo;
} __packed;  

 
#define PTP_CTX_MAX_DATA_SIZE   128

 
struct iwl_time_msmt_ptp_ctx {
	 
	union {
		struct {
			u8 element_id;
			u8 length;
			__le16 reserved;
			u8 data[PTP_CTX_MAX_DATA_SIZE];
		} ftm;  
		struct {
			u8 element_id;
			u8 length;
			u8 data[PTP_CTX_MAX_DATA_SIZE];
		} tm;  
	};
} __packed  ;

 
struct iwl_time_msmt_notify {
	u8 peer_addr[ETH_ALEN];
	u8 reserved[2];
	__le32 dialog_token;
	__le32 followup_dialog_token;
	__le32 t1_hi;
	__le32 t1_lo;
	__le32 t1_max_err;
	__le32 t4_hi;
	__le32 t4_lo;
	__le32 t4_max_err;
	__le32 t2_hi;
	__le32 t2_lo;
	__le32 t2_max_err;
	__le32 t3_hi;
	__le32 t3_lo;
	__le32 t3_max_err;
	struct iwl_time_msmt_ptp_ctx ptp;
} __packed;  

 
struct iwl_time_msmt_cfm_notify {
	u8 peer_addr[ETH_ALEN];
	u8 reserved[2];
	__le32 dialog_token;
	__le32 t1_hi;
	__le32 t1_lo;
	__le32 t1_max_err;
	__le32 t4_hi;
	__le32 t4_lo;
	__le32 t4_max_err;
} __packed;  

 
struct iwl_channel_estimation_cfg {
	 
	__le32 flags;
	 
	__le32 timer;
	 
	__le32 count;
	 
	__le32 rate_n_flags_mask;
	 
	__le32 rate_n_flags_val;
	 
	__le32 reserved;
	 
	__le64 frame_types;
} __packed;  

enum iwl_datapath_monitor_notif_type {
	IWL_DP_MON_NOTIF_TYPE_EXT_CCA,
};

struct iwl_datapath_monitor_notif {
	__le32 type;
	u8 mac_id;
	u8 reserved[3];
} __packed;  

 
enum iwl_thermal_dual_chain_req_events {
	THERMAL_DUAL_CHAIN_REQ_ENABLE,
	THERMAL_DUAL_CHAIN_REQ_DISABLE,
};  

 
struct iwl_thermal_dual_chain_request {
	__le32 event;
} __packed;  

enum iwl_rlc_chain_info {
	IWL_RLC_CHAIN_INFO_DRIVER_FORCE		= BIT(0),
	IWL_RLC_CHAIN_INFO_VALID		= 0x000e,
	IWL_RLC_CHAIN_INFO_FORCE		= 0x0070,
	IWL_RLC_CHAIN_INFO_FORCE_MIMO		= 0x0380,
	IWL_RLC_CHAIN_INFO_COUNT		= 0x0c00,
	IWL_RLC_CHAIN_INFO_MIMO_COUNT		= 0x3000,
};

 
struct iwl_rlc_properties {
	__le32 rx_chain_info;
	__le32 reserved;
} __packed;  

enum iwl_sad_mode {
	IWL_SAD_MODE_ENABLED		= BIT(0),
	IWL_SAD_MODE_DEFAULT_ANT_MSK	= 0x6,
	IWL_SAD_MODE_DEFAULT_ANT_FW	= 0x0,
	IWL_SAD_MODE_DEFAULT_ANT_A	= 0x2,
	IWL_SAD_MODE_DEFAULT_ANT_B	= 0x4,
};

 
struct iwl_sad_properties {
	__le32 chain_a_sad_mode;
	__le32 chain_b_sad_mode;
	__le32 mac_id;
	__le32 reserved;
} __packed;

 
struct iwl_rlc_config_cmd {
	__le32 phy_id;
	struct iwl_rlc_properties rlc;
	struct iwl_sad_properties sad;
	u8 flags;
	u8 reserved[3];
} __packed;  

#define IWL_MAX_BAID_OLD	16  
#define IWL_MAX_BAID		32  

 
enum iwl_rx_baid_action {
	IWL_RX_BAID_ACTION_ADD,
	IWL_RX_BAID_ACTION_MODIFY,
	IWL_RX_BAID_ACTION_REMOVE,
};  

 
struct iwl_rx_baid_cfg_cmd_alloc {
	__le32 sta_id_mask;
	u8 tid;
	u8 reserved[3];
	__le16 ssn;
	__le16 win_size;
} __packed;  

 
struct iwl_rx_baid_cfg_cmd_modify {
	__le32 old_sta_id_mask;
	__le32 new_sta_id_mask;
	__le32 tid;
} __packed;  

 
struct iwl_rx_baid_cfg_cmd_remove_v1 {
	__le32 baid;
} __packed;  

 
struct iwl_rx_baid_cfg_cmd_remove {
	__le32 sta_id_mask;
	__le32 tid;
} __packed;  

 
struct iwl_rx_baid_cfg_cmd {
	__le32 action;
	union {
		struct iwl_rx_baid_cfg_cmd_alloc alloc;
		struct iwl_rx_baid_cfg_cmd_modify modify;
		struct iwl_rx_baid_cfg_cmd_remove_v1 remove_v1;
		struct iwl_rx_baid_cfg_cmd_remove remove;
	};  
} __packed;  

 
struct iwl_rx_baid_cfg_resp {
	__le32 baid;
};  

 
enum iwl_scd_queue_cfg_operation {
	IWL_SCD_QUEUE_ADD = 0,
	IWL_SCD_QUEUE_REMOVE = 1,
	IWL_SCD_QUEUE_MODIFY = 2,
};

 
struct iwl_scd_queue_cfg_cmd {
	__le32 operation;
	union {
		struct {
			__le32 sta_mask;
			u8 tid;
			u8 reserved[3];
			__le32 flags;
			__le32 cb_size;
			__le64 bc_dram_addr;
			__le64 tfdq_dram_addr;
		} __packed add;  
		struct {
			__le32 sta_mask;
			__le32 tid;
		} __packed remove;  
		struct {
			__le32 old_sta_mask;
			__le32 tid;
			__le32 new_sta_mask;
		} __packed modify;  
	} __packed u;  
} __packed;  

 
enum iwl_sec_key_flags {
	IWL_SEC_KEY_FLAG_CIPHER_MASK	= 0x07,
	IWL_SEC_KEY_FLAG_CIPHER_WEP	= 0x01,
	IWL_SEC_KEY_FLAG_CIPHER_CCMP	= 0x02,
	IWL_SEC_KEY_FLAG_CIPHER_TKIP	= 0x03,
	IWL_SEC_KEY_FLAG_CIPHER_GCMP	= 0x05,
	IWL_SEC_KEY_FLAG_NO_TX		= 0x08,
	IWL_SEC_KEY_FLAG_KEY_SIZE	= 0x10,
	IWL_SEC_KEY_FLAG_MFP		= 0x20,
	IWL_SEC_KEY_FLAG_MCAST_KEY	= 0x40,
	IWL_SEC_KEY_FLAG_SPP_AMSDU	= 0x80,
};

#define IWL_SEC_WEP_KEY_OFFSET	3

 
struct iwl_sec_key_cmd {
	__le32 action;
	union {
		struct {
			__le32 sta_mask;
			__le32 key_id;
			__le32 key_flags;
			u8 key[32];
			u8 tkip_mic_rx_key[8];
			u8 tkip_mic_tx_key[8];
			__le64 rx_seq;
			__le64 tx_seq;
		} __packed add;  
		struct {
			__le32 old_sta_mask;
			__le32 new_sta_mask;
			__le32 key_id;
			__le32 key_flags;
		} __packed modify;  
		struct {
			__le32 sta_mask;
			__le32 key_id;
			__le32 key_flags;
		} __packed remove;  
	} __packed u;  
} __packed;  

#endif  
