#ifndef __iwl_fw_api_sta_h__
#define __iwl_fw_api_sta_h__
enum iwl_sta_flags {
	STA_FLG_REDUCED_TX_PWR_CTRL	= BIT(3),
	STA_FLG_REDUCED_TX_PWR_DATA	= BIT(6),
	STA_FLG_DISABLE_TX		= BIT(4),
	STA_FLG_PS			= BIT(8),
	STA_FLG_DRAIN_FLOW		= BIT(12),
	STA_FLG_PAN			= BIT(13),
	STA_FLG_CLASS_AUTH		= BIT(14),
	STA_FLG_CLASS_ASSOC		= BIT(15),
	STA_FLG_RTS_MIMO_PROT		= BIT(17),
	STA_FLG_MAX_AGG_SIZE_SHIFT	= 19,
	STA_FLG_MAX_AGG_SIZE_8K		= (0 << STA_FLG_MAX_AGG_SIZE_SHIFT),
	STA_FLG_MAX_AGG_SIZE_16K	= (1 << STA_FLG_MAX_AGG_SIZE_SHIFT),
	STA_FLG_MAX_AGG_SIZE_32K	= (2 << STA_FLG_MAX_AGG_SIZE_SHIFT),
	STA_FLG_MAX_AGG_SIZE_64K	= (3 << STA_FLG_MAX_AGG_SIZE_SHIFT),
	STA_FLG_MAX_AGG_SIZE_128K	= (4 << STA_FLG_MAX_AGG_SIZE_SHIFT),
	STA_FLG_MAX_AGG_SIZE_256K	= (5 << STA_FLG_MAX_AGG_SIZE_SHIFT),
	STA_FLG_MAX_AGG_SIZE_512K	= (6 << STA_FLG_MAX_AGG_SIZE_SHIFT),
	STA_FLG_MAX_AGG_SIZE_1024K	= (7 << STA_FLG_MAX_AGG_SIZE_SHIFT),
	STA_FLG_MAX_AGG_SIZE_2M		= (8 << STA_FLG_MAX_AGG_SIZE_SHIFT),
	STA_FLG_MAX_AGG_SIZE_4M		= (9 << STA_FLG_MAX_AGG_SIZE_SHIFT),
	STA_FLG_MAX_AGG_SIZE_MSK	= (0xf << STA_FLG_MAX_AGG_SIZE_SHIFT),
	STA_FLG_AGG_MPDU_DENS_SHIFT	= 23,
	STA_FLG_AGG_MPDU_DENS_2US	= (4 << STA_FLG_AGG_MPDU_DENS_SHIFT),
	STA_FLG_AGG_MPDU_DENS_4US	= (5 << STA_FLG_AGG_MPDU_DENS_SHIFT),
	STA_FLG_AGG_MPDU_DENS_8US	= (6 << STA_FLG_AGG_MPDU_DENS_SHIFT),
	STA_FLG_AGG_MPDU_DENS_16US	= (7 << STA_FLG_AGG_MPDU_DENS_SHIFT),
	STA_FLG_AGG_MPDU_DENS_MSK	= (7 << STA_FLG_AGG_MPDU_DENS_SHIFT),
	STA_FLG_FAT_EN_20MHZ		= (0 << 26),
	STA_FLG_FAT_EN_40MHZ		= (1 << 26),
	STA_FLG_FAT_EN_80MHZ		= (2 << 26),
	STA_FLG_FAT_EN_160MHZ		= (3 << 26),
	STA_FLG_FAT_EN_MSK		= (3 << 26),
	STA_FLG_MIMO_EN_SISO		= (0 << 28),
	STA_FLG_MIMO_EN_MIMO2		= (1 << 28),
	STA_FLG_MIMO_EN_MIMO3		= (2 << 28),
	STA_FLG_MIMO_EN_MSK		= (3 << 28),
};
enum iwl_sta_key_flag {
	STA_KEY_FLG_NO_ENC		= (0 << 0),
	STA_KEY_FLG_WEP			= (1 << 0),
	STA_KEY_FLG_CCM			= (2 << 0),
	STA_KEY_FLG_TKIP		= (3 << 0),
	STA_KEY_FLG_EXT			= (4 << 0),
	STA_KEY_FLG_GCMP		= (5 << 0),
	STA_KEY_FLG_CMAC		= (6 << 0),
	STA_KEY_FLG_ENC_UNKNOWN		= (7 << 0),
	STA_KEY_FLG_EN_MSK		= (7 << 0),
	STA_KEY_FLG_WEP_KEY_MAP		= BIT(3),
	STA_KEY_FLG_KEYID_POS		 = 8,
	STA_KEY_FLG_KEYID_MSK		= (3 << STA_KEY_FLG_KEYID_POS),
	STA_KEY_NOT_VALID		= BIT(11),
	STA_KEY_FLG_WEP_13BYTES		= BIT(12),
	STA_KEY_FLG_KEY_32BYTES		= BIT(12),
	STA_KEY_MULTICAST		= BIT(14),
	STA_KEY_MFP			= BIT(15),
};
enum iwl_sta_modify_flag {
	STA_MODIFY_QUEUE_REMOVAL		= BIT(0),
	STA_MODIFY_TID_DISABLE_TX		= BIT(1),
	STA_MODIFY_UAPSD_ACS			= BIT(2),
	STA_MODIFY_ADD_BA_TID			= BIT(3),
	STA_MODIFY_REMOVE_BA_TID		= BIT(4),
	STA_MODIFY_SLEEPING_STA_TX_COUNT	= BIT(5),
	STA_MODIFY_PROT_TH			= BIT(6),
	STA_MODIFY_QUEUES			= BIT(7),
};
enum iwl_sta_mode {
	STA_MODE_ADD	= 0,
	STA_MODE_MODIFY	= 1,
};
enum iwl_sta_sleep_flag {
	STA_SLEEP_STATE_AWAKE		= 0,
	STA_SLEEP_STATE_PS_POLL		= BIT(0),
	STA_SLEEP_STATE_UAPSD		= BIT(1),
	STA_SLEEP_STATE_MOREDATA	= BIT(2),
};
#define STA_KEY_MAX_NUM (16)
#define STA_KEY_IDX_INVALID (0xff)
#define STA_KEY_MAX_DATA_KEY_NUM (4)
#define IWL_MAX_GLOBAL_KEYS (4)
#define STA_KEY_LEN_WEP40 (5)
#define STA_KEY_LEN_WEP104 (13)
#define IWL_ADD_STA_STATUS_MASK		0xFF
#define IWL_ADD_STA_BAID_VALID_MASK	0x8000
#define IWL_ADD_STA_BAID_MASK		0x7F00
#define IWL_ADD_STA_BAID_SHIFT		8
struct iwl_mvm_add_sta_cmd_v7 {
	u8 add_modify;
	u8 awake_acs;
	__le16 tid_disable_tx;
	__le32 mac_id_n_color;
	u8 addr[ETH_ALEN];	 
	__le16 reserved2;
	u8 sta_id;
	u8 modify_mask;
	__le16 reserved3;
	__le32 station_flags;
	__le32 station_flags_msk;
	u8 add_immediate_ba_tid;
	u8 remove_immediate_ba_tid;
	__le16 add_immediate_ba_ssn;
	__le16 sleep_tx_count;
	__le16 sleep_state_flags;
	__le16 assoc_id;
	__le16 beamform_flags;
	__le32 tfd_queue_msk;
} __packed;  
enum iwl_sta_type {
	IWL_STA_LINK,
	IWL_STA_GENERAL_PURPOSE,
	IWL_STA_MULTICAST,
	IWL_STA_TDLS_LINK,
	IWL_STA_AUX_ACTIVITY,
};
struct iwl_mvm_add_sta_cmd {
	u8 add_modify;
	u8 awake_acs;
	__le16 tid_disable_tx;
	__le32 mac_id_n_color;   
	u8 addr[ETH_ALEN];	 
	__le16 reserved2;
	u8 sta_id;
	u8 modify_mask;
	__le16 reserved3;
	__le32 station_flags;
	__le32 station_flags_msk;
	u8 add_immediate_ba_tid;
	u8 remove_immediate_ba_tid;
	__le16 add_immediate_ba_ssn;
	__le16 sleep_tx_count;
	u8 sleep_state_flags;
	u8 station_type;
	__le16 assoc_id;
	__le16 beamform_flags;
	__le32 tfd_queue_msk;
	__le16 rx_ba_window;
	u8 sp_length;
	u8 uapsd_acs;
} __packed;  
struct iwl_mvm_add_sta_key_common {
	u8 sta_id;
	u8 key_offset;
	__le16 key_flags;
	u8 key[32];
	u8 rx_secur_seq_cnt[16];
} __packed;
struct iwl_mvm_add_sta_key_cmd_v1 {
	struct iwl_mvm_add_sta_key_common common;
	u8 tkip_rx_tsc_byte2;
	u8 reserved;
	__le16 tkip_rx_ttak[5];
} __packed;  
struct iwl_mvm_add_sta_key_cmd {
	struct iwl_mvm_add_sta_key_common common;
	__le64 rx_mic_key;
	__le64 tx_mic_key;
	__le64 transmit_seq_cnt;
} __packed;  
enum iwl_mvm_add_sta_rsp_status {
	ADD_STA_SUCCESS			= 0x1,
	ADD_STA_STATIONS_OVERLOAD	= 0x2,
	ADD_STA_IMMEDIATE_BA_FAILURE	= 0x4,
	ADD_STA_MODIFY_NON_EXISTING_STA	= 0x8,
};
struct iwl_mvm_rm_sta_cmd {
	u8 sta_id;
	u8 reserved[3];
} __packed;  
struct iwl_mvm_mgmt_mcast_key_cmd_v1 {
	__le32 ctrl_flags;
	u8 igtk[16];
	u8 k1[16];
	u8 k2[16];
	__le32 key_id;
	__le32 sta_id;
	__le64 receive_seq_cnt;
} __packed;  
struct iwl_mvm_mgmt_mcast_key_cmd {
	__le32 ctrl_flags;
	u8 igtk[32];
	__le32 key_id;
	__le32 sta_id;
	__le64 receive_seq_cnt;
} __packed;  
struct iwl_mvm_wep_key {
	u8 key_index;
	u8 key_offset;
	__le16 reserved1;
	u8 key_size;
	u8 reserved2[3];
	u8 key[16];
} __packed;
struct iwl_mvm_wep_key_cmd {
	__le32 mac_id_n_color;
	u8 num_keys;
	u8 decryption_type;
	u8 flags;
	u8 reserved;
	struct iwl_mvm_wep_key wep_key[];
} __packed;  
struct iwl_mvm_eosp_notification {
	__le32 remain_frame_count;
	__le32 sta_id;
} __packed;  
#endif  
