#ifndef __iwl_fw_api_scan_h__
#define __iwl_fw_api_scan_h__
enum iwl_scan_subcmd_ids {
	OFFLOAD_MATCH_INFO_NOTIF = 0xFC,
};
#define PROBE_OPTION_MAX		20
#define SCAN_SHORT_SSID_MAX_SIZE        8
#define SCAN_BSSID_MAX_SIZE             16
struct iwl_ssid_ie {
	u8 id;
	u8 len;
	u8 ssid[IEEE80211_MAX_SSID_LEN];
} __packed;  
#define IWL_SCAN_MAX_BLACKLIST_LEN	64
#define IWL_SCAN_SHORT_BLACKLIST_LEN	16
#define IWL_SCAN_MAX_PROFILES		11
#define IWL_SCAN_MAX_PROFILES_V2	8
#define SCAN_OFFLOAD_PROBE_REQ_SIZE	512
#define SCAN_NUM_BAND_PROBE_DATA_V_1	2
#define SCAN_NUM_BAND_PROBE_DATA_V_2	3
#define IWL_SCHED_SCAN_WATCHDOG cpu_to_le16(15000)
#define IWL_GOOD_CRC_TH_DEFAULT cpu_to_le16(1)
#define CAN_ABORT_STATUS 1
#define IWL_FULL_SCAN_MULTIPLIER 5
#define IWL_FAST_SCHED_SCAN_ITERATIONS 3
#define IWL_MAX_SCHED_SCAN_PLANS 2
enum scan_framework_client {
	SCAN_CLIENT_SCHED_SCAN		= BIT(0),
	SCAN_CLIENT_NETDETECT		= BIT(1),
	SCAN_CLIENT_ASSET_TRACKING	= BIT(2),
};
struct iwl_scan_offload_blocklist {
	u8 ssid[ETH_ALEN];
	u8 reported_rssi;
	u8 client_bitmap;
} __packed;
enum iwl_scan_offload_network_type {
	IWL_NETWORK_TYPE_BSS	= 1,
	IWL_NETWORK_TYPE_IBSS	= 2,
	IWL_NETWORK_TYPE_ANY	= 3,
};
enum iwl_scan_offload_band_selection {
	IWL_SCAN_OFFLOAD_SELECT_2_4	= 0x4,
	IWL_SCAN_OFFLOAD_SELECT_5_2	= 0x8,
	IWL_SCAN_OFFLOAD_SELECT_ANY	= 0xc,
};
enum iwl_scan_offload_auth_alg {
	IWL_AUTH_ALGO_UNSUPPORTED  = 0x00,
	IWL_AUTH_ALGO_NONE         = 0x01,
	IWL_AUTH_ALGO_PSK          = 0x02,
	IWL_AUTH_ALGO_8021X        = 0x04,
	IWL_AUTH_ALGO_SAE          = 0x08,
	IWL_AUTH_ALGO_8021X_SHA384 = 0x10,
	IWL_AUTH_ALGO_OWE          = 0x20,
};
struct iwl_scan_offload_profile {
	u8 ssid_index;
	u8 unicast_cipher;
	u8 auth_alg;
	u8 network_type;
	u8 band_selection;
	u8 client_bitmap;
	u8 reserved[2];
} __packed;
struct iwl_scan_offload_profile_cfg_data {
	u8 blocklist_len;
	u8 num_profiles;
	u8 match_notify;
	u8 pass_match;
	u8 active_clients;
	u8 any_beacon_notify;
	u8 reserved[2];
} __packed;
struct iwl_scan_offload_profile_cfg_v1 {
	struct iwl_scan_offload_profile profiles[IWL_SCAN_MAX_PROFILES];
	struct iwl_scan_offload_profile_cfg_data data;
} __packed;  
struct iwl_scan_offload_profile_cfg {
	struct iwl_scan_offload_profile profiles[IWL_SCAN_MAX_PROFILES_V2];
	struct iwl_scan_offload_profile_cfg_data data;
} __packed;  
struct iwl_scan_schedule_lmac {
	__le16 delay;
	u8 iterations;
	u8 full_scan_mul;
} __packed;  
enum iwl_scan_offload_complete_status {
	IWL_SCAN_OFFLOAD_COMPLETED	= 1,
	IWL_SCAN_OFFLOAD_ABORTED	= 2,
};
enum iwl_scan_ebs_status {
	IWL_SCAN_EBS_SUCCESS,
	IWL_SCAN_EBS_FAILED,
	IWL_SCAN_EBS_CHAN_NOT_FOUND,
	IWL_SCAN_EBS_INACTIVE,
};
struct iwl_scan_req_tx_cmd {
	__le32 tx_flags;
	__le32 rate_n_flags;
	u8 sta_id;
	u8 reserved[3];
} __packed;
enum iwl_scan_channel_flags_lmac {
	IWL_UNIFIED_SCAN_CHANNEL_FULL		= BIT(27),
	IWL_UNIFIED_SCAN_CHANNEL_PARTIAL	= BIT(28),
};
struct iwl_scan_channel_cfg_lmac {
	__le32 flags;
	__le16 channel_num;
	__le16 iter_count;
	__le32 iter_interval;
} __packed;
struct iwl_scan_probe_segment {
	__le16 offset;
	__le16 len;
} __packed;
struct iwl_scan_probe_req_v1 {
	struct iwl_scan_probe_segment mac_header;
	struct iwl_scan_probe_segment band_data[SCAN_NUM_BAND_PROBE_DATA_V_1];
	struct iwl_scan_probe_segment common_data;
	u8 buf[SCAN_OFFLOAD_PROBE_REQ_SIZE];
} __packed;
struct iwl_scan_probe_req {
	struct iwl_scan_probe_segment mac_header;
	struct iwl_scan_probe_segment band_data[SCAN_NUM_BAND_PROBE_DATA_V_2];
	struct iwl_scan_probe_segment common_data;
	u8 buf[SCAN_OFFLOAD_PROBE_REQ_SIZE];
} __packed;
enum iwl_scan_channel_flags {
	IWL_SCAN_CHANNEL_FLAG_EBS		= BIT(0),
	IWL_SCAN_CHANNEL_FLAG_EBS_ACCURATE	= BIT(1),
	IWL_SCAN_CHANNEL_FLAG_CACHE_ADD		= BIT(2),
	IWL_SCAN_CHANNEL_FLAG_EBS_FRAG		= BIT(3),
	IWL_SCAN_CHANNEL_FLAG_FORCE_EBS         = BIT(4),
	IWL_SCAN_CHANNEL_FLAG_ENABLE_CHAN_ORDER = BIT(5),
	IWL_SCAN_CHANNEL_FLAG_6G_PSC_NO_FILTER  = BIT(6),
};
struct iwl_scan_channel_opt {
	__le16 flags;
	__le16 non_ebs_ratio;
} __packed;
enum iwl_mvm_lmac_scan_flags {
	IWL_MVM_LMAC_SCAN_FLAG_PASS_ALL		= BIT(0),
	IWL_MVM_LMAC_SCAN_FLAG_PASSIVE		= BIT(1),
	IWL_MVM_LMAC_SCAN_FLAG_PRE_CONNECTION	= BIT(2),
	IWL_MVM_LMAC_SCAN_FLAG_ITER_COMPLETE	= BIT(3),
	IWL_MVM_LMAC_SCAN_FLAG_MULTIPLE_SSIDS	= BIT(4),
	IWL_MVM_LMAC_SCAN_FLAG_FRAGMENTED	= BIT(5),
	IWL_MVM_LMAC_SCAN_FLAGS_RRM_ENABLED	= BIT(6),
	IWL_MVM_LMAC_SCAN_FLAG_EXTENDED_DWELL	= BIT(7),
	IWL_MVM_LMAC_SCAN_FLAG_MATCH		= BIT(9),
};
enum iwl_scan_priority {
	IWL_SCAN_PRIORITY_LOW,
	IWL_SCAN_PRIORITY_MEDIUM,
	IWL_SCAN_PRIORITY_HIGH,
};
enum iwl_scan_priority_ext {
	IWL_SCAN_PRIORITY_EXT_0_LOWEST,
	IWL_SCAN_PRIORITY_EXT_1,
	IWL_SCAN_PRIORITY_EXT_2,
	IWL_SCAN_PRIORITY_EXT_3,
	IWL_SCAN_PRIORITY_EXT_4,
	IWL_SCAN_PRIORITY_EXT_5,
	IWL_SCAN_PRIORITY_EXT_6,
	IWL_SCAN_PRIORITY_EXT_7_HIGHEST,
};
struct iwl_scan_req_lmac {
	__le32 reserved1;
	u8 n_channels;
	u8 active_dwell;
	u8 passive_dwell;
	u8 fragmented_dwell;
	u8 extended_dwell;
	u8 reserved2;
	__le16 rx_chain_select;
	__le32 scan_flags;
	__le32 max_out_time;
	__le32 suspend_time;
	__le32 flags;
	__le32 filter_flags;
	struct iwl_scan_req_tx_cmd tx_cmd[2];
	struct iwl_ssid_ie direct_scan[PROBE_OPTION_MAX];
	__le32 scan_prio;
	__le32 iter_num;
	__le32 delay;
	struct iwl_scan_schedule_lmac schedule[IWL_MAX_SCHED_SCAN_PLANS];
	struct iwl_scan_channel_opt channel_opt[2];
	u8 data[];
} __packed;
struct iwl_scan_results_notif {
	u8 channel;
	u8 band;
	u8 probe_status;
	u8 num_probe_not_sent;
	__le32 duration;
} __packed;
struct iwl_lmac_scan_complete_notif {
	u8 scanned_channels;
	u8 status;
	u8 bt_status;
	u8 last_channel;
	__le32 tsf_low;
	__le32 tsf_high;
	struct iwl_scan_results_notif results[];
} __packed;
struct iwl_periodic_scan_complete {
	u8 last_schedule_line;
	u8 last_schedule_iteration;
	u8 status;
	u8 ebs_status;
	__le32 time_after_last_iter;
	__le32 reserved;
} __packed;
#define IWL_MVM_MAX_UMAC_SCANS 4
#define IWL_MVM_MAX_LMAC_SCANS 1
enum scan_config_flags {
	SCAN_CONFIG_FLAG_ACTIVATE			= BIT(0),
	SCAN_CONFIG_FLAG_DEACTIVATE			= BIT(1),
	SCAN_CONFIG_FLAG_FORBID_CHUB_REQS		= BIT(2),
	SCAN_CONFIG_FLAG_ALLOW_CHUB_REQS		= BIT(3),
	SCAN_CONFIG_FLAG_SET_TX_CHAINS			= BIT(8),
	SCAN_CONFIG_FLAG_SET_RX_CHAINS			= BIT(9),
	SCAN_CONFIG_FLAG_SET_AUX_STA_ID			= BIT(10),
	SCAN_CONFIG_FLAG_SET_ALL_TIMES			= BIT(11),
	SCAN_CONFIG_FLAG_SET_EFFECTIVE_TIMES		= BIT(12),
	SCAN_CONFIG_FLAG_SET_CHANNEL_FLAGS		= BIT(13),
	SCAN_CONFIG_FLAG_SET_LEGACY_RATES		= BIT(14),
	SCAN_CONFIG_FLAG_SET_MAC_ADDR			= BIT(15),
	SCAN_CONFIG_FLAG_SET_FRAGMENTED			= BIT(16),
	SCAN_CONFIG_FLAG_CLEAR_FRAGMENTED		= BIT(17),
	SCAN_CONFIG_FLAG_SET_CAM_MODE			= BIT(18),
	SCAN_CONFIG_FLAG_CLEAR_CAM_MODE			= BIT(19),
	SCAN_CONFIG_FLAG_SET_PROMISC_MODE		= BIT(20),
	SCAN_CONFIG_FLAG_CLEAR_PROMISC_MODE		= BIT(21),
	SCAN_CONFIG_FLAG_SET_LMAC2_FRAGMENTED		= BIT(22),
	SCAN_CONFIG_FLAG_CLEAR_LMAC2_FRAGMENTED		= BIT(23),
#define SCAN_CONFIG_N_CHANNELS(n) ((n) << 26)
};
enum scan_config_rates {
	SCAN_CONFIG_RATE_6M	= BIT(0),
	SCAN_CONFIG_RATE_9M	= BIT(1),
	SCAN_CONFIG_RATE_12M	= BIT(2),
	SCAN_CONFIG_RATE_18M	= BIT(3),
	SCAN_CONFIG_RATE_24M	= BIT(4),
	SCAN_CONFIG_RATE_36M	= BIT(5),
	SCAN_CONFIG_RATE_48M	= BIT(6),
	SCAN_CONFIG_RATE_54M	= BIT(7),
	SCAN_CONFIG_RATE_1M	= BIT(8),
	SCAN_CONFIG_RATE_2M	= BIT(9),
	SCAN_CONFIG_RATE_5M	= BIT(10),
	SCAN_CONFIG_RATE_11M	= BIT(11),
#define SCAN_CONFIG_SUPPORTED_RATE(rate)	((rate) << 16)
};
enum iwl_channel_flags {
	IWL_CHANNEL_FLAG_EBS				= BIT(0),
	IWL_CHANNEL_FLAG_ACCURATE_EBS			= BIT(1),
	IWL_CHANNEL_FLAG_EBS_ADD			= BIT(2),
	IWL_CHANNEL_FLAG_PRE_SCAN_PASSIVE2ACTIVE	= BIT(3),
};
enum iwl_uhb_chan_cfg_flags {
	IWL_UHB_CHAN_CFG_FLAG_UNSOLICITED_PROBE_RES = BIT(24),
	IWL_UHB_CHAN_CFG_FLAG_PSC_CHAN_NO_LISTEN    = BIT(25),
	IWL_UHB_CHAN_CFG_FLAG_FORCE_PASSIVE         = BIT(26),
};
struct iwl_scan_dwell {
	u8 active;
	u8 passive;
	u8 fragmented;
	u8 extended;
} __packed;
struct iwl_scan_config_v1 {
	__le32 flags;
	__le32 tx_chains;
	__le32 rx_chains;
	__le32 legacy_rates;
	__le32 out_of_channel_time;
	__le32 suspend_time;
	struct iwl_scan_dwell dwell;
	u8 mac_addr[ETH_ALEN];
	u8 bcast_sta_id;
	u8 channel_flags;
	u8 channel_array[];
} __packed;  
#define SCAN_TWO_LMACS 2
#define SCAN_LB_LMAC_IDX 0
#define SCAN_HB_LMAC_IDX 1
struct iwl_scan_config_v2 {
	__le32 flags;
	__le32 tx_chains;
	__le32 rx_chains;
	__le32 legacy_rates;
	__le32 out_of_channel_time[SCAN_TWO_LMACS];
	__le32 suspend_time[SCAN_TWO_LMACS];
	struct iwl_scan_dwell dwell;
	u8 mac_addr[ETH_ALEN];
	u8 bcast_sta_id;
	u8 channel_flags;
	u8 channel_array[];
} __packed;  
struct iwl_scan_config {
	u8 enable_cam_mode;
	u8 enable_promiscouos_mode;
	u8 bcast_sta_id;
	u8 reserved;
	__le32 tx_chains;
	__le32 rx_chains;
} __packed;  
enum iwl_umac_scan_flags {
	IWL_UMAC_SCAN_FLAG_PREEMPTIVE		= BIT(0),
	IWL_UMAC_SCAN_FLAG_START_NOTIF		= BIT(1),
};
enum iwl_umac_scan_uid_offsets {
	IWL_UMAC_SCAN_UID_TYPE_OFFSET		= 0,
	IWL_UMAC_SCAN_UID_SEQ_OFFSET		= 8,
};
enum iwl_umac_scan_general_flags {
	IWL_UMAC_SCAN_GEN_FLAGS_PERIODIC		= BIT(0),
	IWL_UMAC_SCAN_GEN_FLAGS_OVER_BT			= BIT(1),
	IWL_UMAC_SCAN_GEN_FLAGS_PASS_ALL		= BIT(2),
	IWL_UMAC_SCAN_GEN_FLAGS_PASSIVE			= BIT(3),
	IWL_UMAC_SCAN_GEN_FLAGS_PRE_CONNECT		= BIT(4),
	IWL_UMAC_SCAN_GEN_FLAGS_ITER_COMPLETE		= BIT(5),
	IWL_UMAC_SCAN_GEN_FLAGS_MULTIPLE_SSID		= BIT(6),
	IWL_UMAC_SCAN_GEN_FLAGS_FRAGMENTED		= BIT(7),
	IWL_UMAC_SCAN_GEN_FLAGS_RRM_ENABLED		= BIT(8),
	IWL_UMAC_SCAN_GEN_FLAGS_MATCH			= BIT(9),
	IWL_UMAC_SCAN_GEN_FLAGS_EXTENDED_DWELL		= BIT(10),
	IWL_UMAC_SCAN_GEN_FLAGS_PROB_REQ_DEFER_SUPP	= BIT(10),
	IWL_UMAC_SCAN_GEN_FLAGS_LMAC2_FRAGMENTED	= BIT(11),
	IWL_UMAC_SCAN_GEN_FLAGS_ADAPTIVE_DWELL		= BIT(13),
	IWL_UMAC_SCAN_GEN_FLAGS_MAX_CHNL_TIME		= BIT(14),
	IWL_UMAC_SCAN_GEN_FLAGS_PROB_REQ_HIGH_TX_RATE	= BIT(15),
};
enum iwl_umac_scan_general_flags2 {
	IWL_UMAC_SCAN_GEN_FLAGS2_NOTIF_PER_CHNL		= BIT(0),
	IWL_UMAC_SCAN_GEN_FLAGS2_ALLOW_CHNL_REORDER	= BIT(1),
};
enum iwl_umac_scan_general_flags_v2 {
	IWL_UMAC_SCAN_GEN_FLAGS_V2_PERIODIC             = BIT(0),
	IWL_UMAC_SCAN_GEN_FLAGS_V2_PASS_ALL             = BIT(1),
	IWL_UMAC_SCAN_GEN_FLAGS_V2_NTFY_ITER_COMPLETE   = BIT(2),
	IWL_UMAC_SCAN_GEN_FLAGS_V2_FRAGMENTED_LMAC1     = BIT(3),
	IWL_UMAC_SCAN_GEN_FLAGS_V2_FRAGMENTED_LMAC2     = BIT(4),
	IWL_UMAC_SCAN_GEN_FLAGS_V2_MATCH                = BIT(5),
	IWL_UMAC_SCAN_GEN_FLAGS_V2_USE_ALL_RX_CHAINS    = BIT(6),
	IWL_UMAC_SCAN_GEN_FLAGS_V2_ADAPTIVE_DWELL       = BIT(7),
	IWL_UMAC_SCAN_GEN_FLAGS_V2_PREEMPTIVE           = BIT(8),
	IWL_UMAC_SCAN_GEN_FLAGS_V2_NTF_START            = BIT(9),
	IWL_UMAC_SCAN_GEN_FLAGS_V2_MULTI_SSID           = BIT(10),
	IWL_UMAC_SCAN_GEN_FLAGS_V2_FORCE_PASSIVE        = BIT(11),
	IWL_UMAC_SCAN_GEN_FLAGS_V2_TRIGGER_UHB_SCAN     = BIT(12),
	IWL_UMAC_SCAN_GEN_FLAGS_V2_6GHZ_PASSIVE_SCAN    = BIT(13),
	IWL_UMAC_SCAN_GEN_FLAGS_V2_6GHZ_PASSIVE_SCAN_FILTER_IN = BIT(14),
	IWL_UMAC_SCAN_GEN_FLAGS_V2_OCE                  = BIT(15),
};
enum iwl_umac_scan_general_params_flags2 {
	IWL_UMAC_SCAN_GEN_PARAMS_FLAGS2_RESPECT_P2P_GO_LB = BIT(0),
	IWL_UMAC_SCAN_GEN_PARAMS_FLAGS2_RESPECT_P2P_GO_HB = BIT(1),
	IWL_UMAC_SCAN_GEN_PARAMS_FLAGS2_DONT_TOGGLE_ANT   = BIT(2),
};
struct  iwl_scan_channel_cfg_umac {
#define IWL_CHAN_CFG_FLAGS_BAND_POS 30
	__le32 flags;
	union {
		struct {
			u8 channel_num;
			u8 iter_count;
			__le16 iter_interval;
		} v1;   
		struct {
			u8 channel_num;
			u8 band;
			u8 iter_count;
			u8 iter_interval;
		 } v2;  
		struct {
			u8 channel_num;
			u8 psd_20;
			u8 iter_count;
			u8 iter_interval;
		} v5;   
	};
} __packed;
struct iwl_scan_umac_schedule {
	__le16 interval;
	u8 iter_count;
	u8 reserved;
} __packed;  
struct iwl_scan_req_umac_tail_v1 {
	struct iwl_scan_umac_schedule schedule[IWL_MAX_SCHED_SCAN_PLANS];
	__le16 delay;
	__le16 reserved;
	struct iwl_scan_probe_req_v1 preq;
	struct iwl_ssid_ie direct_scan[PROBE_OPTION_MAX];
} __packed;
struct iwl_scan_req_umac_tail_v2 {
	struct iwl_scan_umac_schedule schedule[IWL_MAX_SCHED_SCAN_PLANS];
	__le16 delay;
	__le16 reserved;
	struct iwl_scan_probe_req preq;
	struct iwl_ssid_ie direct_scan[PROBE_OPTION_MAX];
} __packed;
struct iwl_scan_umac_chan_param {
	u8 flags;
	u8 count;
	__le16 reserved;
} __packed;  
struct iwl_scan_req_umac {
	__le32 flags;
	__le32 uid;
	__le32 ooc_priority;
	__le16 general_flags;
	u8 reserved;
	u8 scan_start_mac_id;
	union {
		struct {
			u8 extended_dwell;
			u8 active_dwell;
			u8 passive_dwell;
			u8 fragmented_dwell;
			__le32 max_out_time;
			__le32 suspend_time;
			__le32 scan_priority;
			struct iwl_scan_umac_chan_param channel;
			u8 data[];
		} v1;  
		struct {
			u8 extended_dwell;
			u8 active_dwell;
			u8 passive_dwell;
			u8 fragmented_dwell;
			__le32 max_out_time[SCAN_TWO_LMACS];
			__le32 suspend_time[SCAN_TWO_LMACS];
			__le32 scan_priority;
			struct iwl_scan_umac_chan_param channel;
			u8 data[];
		} v6;  
		struct {
			u8 active_dwell;
			u8 passive_dwell;
			u8 fragmented_dwell;
			u8 adwell_default_n_aps;
			u8 adwell_default_n_aps_social;
			u8 reserved3;
			__le16 adwell_max_budget;
			__le32 max_out_time[SCAN_TWO_LMACS];
			__le32 suspend_time[SCAN_TWO_LMACS];
			__le32 scan_priority;
			struct iwl_scan_umac_chan_param channel;
			u8 data[];
		} v7;  
		struct {
			u8 active_dwell[SCAN_TWO_LMACS];
			u8 reserved2;
			u8 adwell_default_n_aps;
			u8 adwell_default_n_aps_social;
			u8 general_flags2;
			__le16 adwell_max_budget;
			__le32 max_out_time[SCAN_TWO_LMACS];
			__le32 suspend_time[SCAN_TWO_LMACS];
			__le32 scan_priority;
			u8 passive_dwell[SCAN_TWO_LMACS];
			u8 num_of_fragments[SCAN_TWO_LMACS];
			struct iwl_scan_umac_chan_param channel;
			u8 data[];
		} v8;  
		struct {
			u8 active_dwell[SCAN_TWO_LMACS];
			u8 adwell_default_hb_n_aps;
			u8 adwell_default_lb_n_aps;
			u8 adwell_default_n_aps_social;
			u8 general_flags2;
			__le16 adwell_max_budget;
			__le32 max_out_time[SCAN_TWO_LMACS];
			__le32 suspend_time[SCAN_TWO_LMACS];
			__le32 scan_priority;
			u8 passive_dwell[SCAN_TWO_LMACS];
			u8 num_of_fragments[SCAN_TWO_LMACS];
			struct iwl_scan_umac_chan_param channel;
			u8 data[];
		} v9;  
	};
} __packed;
#define IWL_SCAN_REQ_UMAC_SIZE_V8 sizeof(struct iwl_scan_req_umac)
#define IWL_SCAN_REQ_UMAC_SIZE_V7 48
#define IWL_SCAN_REQ_UMAC_SIZE_V6 44
#define IWL_SCAN_REQ_UMAC_SIZE_V1 36
struct iwl_scan_probe_params_v3 {
	struct iwl_scan_probe_req preq;
	u8 ssid_num;
	u8 short_ssid_num;
	u8 bssid_num;
	u8 reserved;
	struct iwl_ssid_ie direct_scan[PROBE_OPTION_MAX];
	__le32 short_ssid[SCAN_SHORT_SSID_MAX_SIZE];
	u8 bssid_array[SCAN_BSSID_MAX_SIZE][ETH_ALEN];
} __packed;  
struct iwl_scan_probe_params_v4 {
	struct iwl_scan_probe_req preq;
	u8 short_ssid_num;
	u8 bssid_num;
	__le16 reserved;
	struct iwl_ssid_ie direct_scan[PROBE_OPTION_MAX];
	__le32 short_ssid[SCAN_SHORT_SSID_MAX_SIZE];
	u8 bssid_array[SCAN_BSSID_MAX_SIZE][ETH_ALEN];
} __packed;  
#define SCAN_MAX_NUM_CHANS_V3 67
struct iwl_scan_channel_params_v4 {
	u8 flags;
	u8 count;
	u8 num_of_aps_override;
	u8 reserved;
	struct iwl_scan_channel_cfg_umac channel_config[SCAN_MAX_NUM_CHANS_V3];
	u8 adwell_ch_override_bitmap[16];
} __packed;  
struct iwl_scan_channel_params_v7 {
	u8 flags;
	u8 count;
	u8 n_aps_override[2];
	struct iwl_scan_channel_cfg_umac channel_config[SCAN_MAX_NUM_CHANS_V3];
} __packed;  
struct iwl_scan_general_params_v11 {
	__le16 flags;
	u8 reserved;
	u8 scan_start_mac_or_link_id;
	u8 active_dwell[SCAN_TWO_LMACS];
	u8 adwell_default_2g;
	u8 adwell_default_5g;
	u8 adwell_default_social_chn;
	u8 flags2;
	__le16 adwell_max_budget;
	__le32 max_out_of_time[SCAN_TWO_LMACS];
	__le32 suspend_time[SCAN_TWO_LMACS];
	__le32 scan_priority;
	u8 passive_dwell[SCAN_TWO_LMACS];
	u8 num_of_fragments[SCAN_TWO_LMACS];
} __packed;  
struct iwl_scan_periodic_parms_v1 {
	struct iwl_scan_umac_schedule schedule[IWL_MAX_SCHED_SCAN_PLANS];
	__le16 delay;
	__le16 reserved;
} __packed;  
struct iwl_scan_req_params_v12 {
	struct iwl_scan_general_params_v11 general_params;
	struct iwl_scan_channel_params_v4 channel_params;
	struct iwl_scan_periodic_parms_v1 periodic_params;
	struct iwl_scan_probe_params_v3 probe_params;
} __packed;  
struct iwl_scan_req_params_v17 {
	struct iwl_scan_general_params_v11 general_params;
	struct iwl_scan_channel_params_v7 channel_params;
	struct iwl_scan_periodic_parms_v1 periodic_params;
	struct iwl_scan_probe_params_v4 probe_params;
} __packed;  
struct iwl_scan_req_umac_v12 {
	__le32 uid;
	__le32 ooc_priority;
	struct iwl_scan_req_params_v12 scan_params;
} __packed;  
struct iwl_scan_req_umac_v17 {
	__le32 uid;
	__le32 ooc_priority;
	struct iwl_scan_req_params_v17 scan_params;
} __packed;  
struct iwl_umac_scan_abort {
	__le32 uid;
	__le32 flags;
} __packed;  
struct iwl_umac_scan_complete {
	__le32 uid;
	u8 last_schedule;
	u8 last_iter;
	u8 status;
	u8 ebs_status;
	__le32 time_from_last_iter;
	__le32 reserved;
} __packed;  
#define SCAN_OFFLOAD_MATCHING_CHANNELS_LEN_V1 5
#define SCAN_OFFLOAD_MATCHING_CHANNELS_LEN    7
struct iwl_scan_offload_profile_match_v1 {
	u8 bssid[ETH_ALEN];
	__le16 reserved;
	u8 channel;
	u8 energy;
	u8 matching_feature;
	u8 matching_channels[SCAN_OFFLOAD_MATCHING_CHANNELS_LEN_V1];
} __packed;  
struct iwl_scan_offload_profiles_query_v1 {
	__le32 matched_profiles;
	__le32 last_scan_age;
	__le32 n_scans_done;
	__le32 gp2_d0u;
	__le32 gp2_invoked;
	u8 resume_while_scanning;
	u8 self_recovery;
	__le16 reserved;
	struct iwl_scan_offload_profile_match_v1 matches[];
} __packed;  
struct iwl_scan_offload_profile_match {
	u8 bssid[ETH_ALEN];
	__le16 reserved;
	u8 channel;
	u8 energy;
	u8 matching_feature;
	u8 matching_channels[SCAN_OFFLOAD_MATCHING_CHANNELS_LEN];
} __packed;  
struct iwl_scan_offload_match_info {
	__le32 matched_profiles;
	__le32 last_scan_age;
	__le32 n_scans_done;
	__le32 gp2_d0u;
	__le32 gp2_invoked;
	u8 resume_while_scanning;
	u8 self_recovery;
	__le16 reserved;
	struct iwl_scan_offload_profile_match matches[];
} __packed;  
struct iwl_umac_scan_iter_complete_notif {
	__le32 uid;
	u8 scanned_channels;
	u8 status;
	u8 bt_status;
	u8 last_channel;
	__le64 start_tsf;
	struct iwl_scan_results_notif results[];
} __packed;  
#endif  
