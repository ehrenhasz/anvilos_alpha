#ifndef __iwl_fw_api_location_h__
#define __iwl_fw_api_location_h__
enum iwl_location_subcmd_ids {
	TOF_RANGE_REQ_CMD = 0x0,
	TOF_CONFIG_CMD = 0x1,
	TOF_RANGE_ABORT_CMD = 0x2,
	TOF_RANGE_REQ_EXT_CMD = 0x3,
	TOF_RESPONDER_CONFIG_CMD = 0x4,
	TOF_RESPONDER_DYN_CONFIG_CMD = 0x5,
	CSI_HEADER_NOTIFICATION = 0xFA,
	CSI_CHUNKS_NOTIFICATION = 0xFB,
	TOF_LC_NOTIF = 0xFC,
	TOF_RESPONDER_STATS = 0xFD,
	TOF_MCSI_DEBUG_NOTIF = 0xFE,
	TOF_RANGE_RESPONSE_NOTIF = 0xFF,
};
struct iwl_tof_config_cmd {
	u8 tof_disabled;
	u8 one_sided_disabled;
	u8 is_debug_mode;
	u8 is_buf_required;
} __packed;
enum iwl_tof_bandwidth {
	IWL_TOF_BW_20_LEGACY,
	IWL_TOF_BW_20_HT,
	IWL_TOF_BW_40,
	IWL_TOF_BW_80,
	IWL_TOF_BW_160,
	IWL_TOF_BW_NUM,
};  
enum iwl_tof_algo_type {
	IWL_TOF_ALGO_TYPE_MAX_LIKE	= 0,
	IWL_TOF_ALGO_TYPE_LINEAR_REG	= 1,
	IWL_TOF_ALGO_TYPE_FFT		= 2,
	IWL_TOF_ALGO_TYPE_INVALID,
};  
enum iwl_tof_mcsi_enable {
	IWL_TOF_MCSI_DISABLED = 0,
	IWL_TOF_MCSI_ENABLED = 1,
};  
enum iwl_tof_responder_cmd_valid_field {
	IWL_TOF_RESPONDER_CMD_VALID_CHAN_INFO = BIT(0),
	IWL_TOF_RESPONDER_CMD_VALID_TOA_OFFSET = BIT(1),
	IWL_TOF_RESPONDER_CMD_VALID_COMMON_CALIB = BIT(2),
	IWL_TOF_RESPONDER_CMD_VALID_SPECIFIC_CALIB = BIT(3),
	IWL_TOF_RESPONDER_CMD_VALID_BSSID = BIT(4),
	IWL_TOF_RESPONDER_CMD_VALID_TX_ANT = BIT(5),
	IWL_TOF_RESPONDER_CMD_VALID_ALGO_TYPE = BIT(6),
	IWL_TOF_RESPONDER_CMD_VALID_NON_ASAP_SUPPORT = BIT(7),
	IWL_TOF_RESPONDER_CMD_VALID_STATISTICS_REPORT_SUPPORT = BIT(8),
	IWL_TOF_RESPONDER_CMD_VALID_MCSI_NOTIF_SUPPORT = BIT(9),
	IWL_TOF_RESPONDER_CMD_VALID_FAST_ALGO_SUPPORT = BIT(10),
	IWL_TOF_RESPONDER_CMD_VALID_RETRY_ON_ALGO_FAIL = BIT(11),
	IWL_TOF_RESPONDER_CMD_VALID_STA_ID = BIT(12),
	IWL_TOF_RESPONDER_CMD_VALID_NDP_SUPPORT = BIT(22),
	IWL_TOF_RESPONDER_CMD_VALID_NDP_PARAMS = BIT(23),
	IWL_TOF_RESPONDER_CMD_VALID_LMR_FEEDBACK = BIT(24),
	IWL_TOF_RESPONDER_CMD_VALID_SESSION_ID = BIT(25),
	IWL_TOF_RESPONDER_CMD_VALID_BSS_COLOR = BIT(26),
	IWL_TOF_RESPONDER_CMD_VALID_MIN_MAX_TIME_BETWEEN_MSR = BIT(27),
};
enum iwl_tof_responder_cfg_flags {
	IWL_TOF_RESPONDER_FLAGS_NON_ASAP_SUPPORT = BIT(0),
	IWL_TOF_RESPONDER_FLAGS_REPORT_STATISTICS = BIT(1),
	IWL_TOF_RESPONDER_FLAGS_REPORT_MCSI = BIT(2),
	IWL_TOF_RESPONDER_FLAGS_ALGO_TYPE = BIT(3) | BIT(4) | BIT(5),
	IWL_TOF_RESPONDER_FLAGS_TOA_OFFSET_MODE = BIT(6),
	IWL_TOF_RESPONDER_FLAGS_COMMON_CALIB_MODE = BIT(7),
	IWL_TOF_RESPONDER_FLAGS_SPECIFIC_CALIB_MODE = BIT(8),
	IWL_TOF_RESPONDER_FLAGS_FAST_ALGO_SUPPORT = BIT(9),
	IWL_TOF_RESPONDER_FLAGS_RETRY_ON_ALGO_FAIL = BIT(10),
	IWL_TOF_RESPONDER_FLAGS_FTM_TX_ANT = RATE_MCS_ANT_AB_MSK,
	IWL_TOF_RESPONDER_FLAGS_NDP_SUPPORT = BIT(24),
	IWL_TOF_RESPONDER_FLAGS_LMR_FEEDBACK = BIT(25),
	IWL_TOF_RESPONDER_FLAGS_SESSION_ID = BIT(27),
};
struct iwl_tof_responder_config_cmd_v6 {
	__le32 cmd_valid_fields;
	__le32 responder_cfg_flags;
	u8 bandwidth;
	u8 rate;
	u8 channel_num;
	u8 ctrl_ch_position;
	u8 sta_id;
	u8 reserved1;
	__le16 toa_offset;
	__le16 common_calib;
	__le16 specific_calib;
	u8 bssid[ETH_ALEN];
	__le16 reserved2;
} __packed;  
struct iwl_tof_responder_config_cmd_v7 {
	__le32 cmd_valid_fields;
	__le32 responder_cfg_flags;
	u8 format_bw;
	u8 rate;
	u8 channel_num;
	u8 ctrl_ch_position;
	u8 sta_id;
	u8 reserved1;
	__le16 toa_offset;
	__le16 common_calib;
	__le16 specific_calib;
	u8 bssid[ETH_ALEN];
	__le16 reserved2;
} __packed;  
#define IWL_RESPONDER_STS_POS	3
#define IWL_RESPONDER_TOTAL_LTF_POS	6
struct iwl_tof_responder_config_cmd_v8 {
	__le32 cmd_valid_fields;
	__le32 responder_cfg_flags;
	u8 format_bw;
	u8 rate;
	u8 channel_num;
	u8 ctrl_ch_position;
	u8 sta_id;
	u8 reserved1;
	__le16 toa_offset;
	__le16 common_calib;
	__le16 specific_calib;
	u8 bssid[ETH_ALEN];
	u8 r2i_ndp_params;
	u8 i2r_ndp_params;
} __packed;  
struct iwl_tof_responder_config_cmd_v9 {
	__le32 cmd_valid_fields;
	__le32 responder_cfg_flags;
	u8 format_bw;
	u8 bss_color;
	u8 channel_num;
	u8 ctrl_ch_position;
	u8 sta_id;
	u8 reserved1;
	__le16 toa_offset;
	__le16 common_calib;
	__le16 specific_calib;
	u8 bssid[ETH_ALEN];
	u8 r2i_ndp_params;
	u8 i2r_ndp_params;
	__le16 min_time_between_msr;
	__le16 max_time_between_msr;
} __packed;  
#define IWL_LCI_CIVIC_IE_MAX_SIZE	400
struct iwl_tof_responder_dyn_config_cmd_v2 {
	__le32 lci_len;
	__le32 civic_len;
	u8 lci_civic[];
} __packed;  
#define IWL_LCI_MAX_SIZE	160
#define IWL_CIVIC_MAX_SIZE	160
#define HLTK_11AZ_LEN	32
enum iwl_responder_dyn_cfg_valid_flags {
	IWL_RESPONDER_DYN_CFG_VALID_LCI = BIT(0),
	IWL_RESPONDER_DYN_CFG_VALID_CIVIC = BIT(1),
	IWL_RESPONDER_DYN_CFG_VALID_PASN_STA = BIT(2),
};
struct iwl_tof_responder_dyn_config_cmd {
	u8 cipher;
	u8 valid_flags;
	u8 lci_len;
	u8 civic_len;
	u8 lci_buf[IWL_LCI_MAX_SIZE];
	u8 civic_buf[IWL_LCI_MAX_SIZE];
	u8 hltk_buf[HLTK_11AZ_LEN];
	u8 addr[ETH_ALEN];
	u8 reserved[2];
} __packed;  
struct iwl_tof_range_req_ext_cmd {
	__le16 tsf_timer_offset_msec;
	__le16 reserved;
	u8 min_delta_ftm;
	u8 ftm_format_and_bw20M;
	u8 ftm_format_and_bw40M;
	u8 ftm_format_and_bw80M;
} __packed;
enum iwl_tof_location_query {
	IWL_TOF_LOC_LCI = 0x01,
	IWL_TOF_LOC_CIVIC = 0x02,
};
struct iwl_tof_range_req_ap_entry_v2 {
	u8 channel_num;
	u8 bandwidth;
	u8 tsf_delta_direction;
	u8 ctrl_ch_position;
	u8 bssid[ETH_ALEN];
	u8 measure_type;
	u8 num_of_bursts;
	__le16 burst_period;
	u8 samples_per_burst;
	u8 retries_per_sample;
	__le32 tsf_delta;
	u8 location_req;
	u8 asap_mode;
	u8 enable_dyn_ack;
	s8 rssi;
	u8 algo_type;
	u8 notify_mcsi;
	__le16 reserved;
} __packed;  
enum iwl_initiator_ap_flags {
	IWL_INITIATOR_AP_FLAGS_ASAP = BIT(1),
	IWL_INITIATOR_AP_FLAGS_LCI_REQUEST = BIT(2),
	IWL_INITIATOR_AP_FLAGS_CIVIC_REQUEST = BIT(3),
	IWL_INITIATOR_AP_FLAGS_DYN_ACK = BIT(4),
	IWL_INITIATOR_AP_FLAGS_ALGO_LR = BIT(5),
	IWL_INITIATOR_AP_FLAGS_ALGO_FFT = BIT(6),
	IWL_INITIATOR_AP_FLAGS_MCSI_REPORT = BIT(8),
	IWL_INITIATOR_AP_FLAGS_NON_TB = BIT(9),
	IWL_INITIATOR_AP_FLAGS_TB = BIT(10),
	IWL_INITIATOR_AP_FLAGS_SECURED = BIT(11),
	IWL_INITIATOR_AP_FLAGS_LMR_FEEDBACK = BIT(12),
	IWL_INITIATOR_AP_FLAGS_USE_CALIB = BIT(13),
	IWL_INITIATOR_AP_FLAGS_PMF = BIT(14),
	IWL_INITIATOR_AP_FLAGS_TERMINATE_ON_LMR_FEEDBACK = BIT(15),
};
struct iwl_tof_range_req_ap_entry_v3 {
	__le32 initiator_ap_flags;
	u8 channel_num;
	u8 bandwidth;
	u8 ctrl_ch_position;
	u8 ftmr_max_retries;
	u8 bssid[ETH_ALEN];
	__le16 burst_period;
	u8 samples_per_burst;
	u8 num_of_bursts;
	__le16 reserved;
	__le32 tsf_delta;
} __packed;  
enum iwl_location_frame_format {
	IWL_LOCATION_FRAME_FORMAT_LEGACY,
	IWL_LOCATION_FRAME_FORMAT_HT,
	IWL_LOCATION_FRAME_FORMAT_VHT,
	IWL_LOCATION_FRAME_FORMAT_HE,
};
enum iwl_location_bw {
	IWL_LOCATION_BW_20MHZ,
	IWL_LOCATION_BW_40MHZ,
	IWL_LOCATION_BW_80MHZ,
	IWL_LOCATION_BW_160MHZ,
};
#define TK_11AZ_LEN	32
#define LOCATION_BW_POS	4
struct iwl_tof_range_req_ap_entry_v4 {
	__le32 initiator_ap_flags;
	u8 channel_num;
	u8 format_bw;
	u8 ctrl_ch_position;
	u8 ftmr_max_retries;
	u8 bssid[ETH_ALEN];
	__le16 burst_period;
	u8 samples_per_burst;
	u8 num_of_bursts;
	__le16 reserved;
	u8 hltk[HLTK_11AZ_LEN];
	u8 tk[TK_11AZ_LEN];
} __packed;  
enum iwl_location_cipher {
	IWL_LOCATION_CIPHER_CCMP_128,
	IWL_LOCATION_CIPHER_GCMP_128,
	IWL_LOCATION_CIPHER_GCMP_256,
	IWL_LOCATION_CIPHER_INVALID,
	IWL_LOCATION_CIPHER_MAX,
};
struct iwl_tof_range_req_ap_entry_v6 {
	__le32 initiator_ap_flags;
	u8 channel_num;
	u8 format_bw;
	u8 ctrl_ch_position;
	u8 ftmr_max_retries;
	u8 bssid[ETH_ALEN];
	__le16 burst_period;
	u8 samples_per_burst;
	u8 num_of_bursts;
	u8 sta_id;
	u8 cipher;
	u8 hltk[HLTK_11AZ_LEN];
	u8 tk[TK_11AZ_LEN];
	__le16 calib[IWL_TOF_BW_NUM];
	__le16 beacon_interval;
} __packed;  
struct iwl_tof_range_req_ap_entry_v7 {
	__le32 initiator_ap_flags;
	u8 channel_num;
	u8 format_bw;
	u8 ctrl_ch_position;
	u8 ftmr_max_retries;
	u8 bssid[ETH_ALEN];
	__le16 burst_period;
	u8 samples_per_burst;
	u8 num_of_bursts;
	u8 sta_id;
	u8 cipher;
	u8 hltk[HLTK_11AZ_LEN];
	u8 tk[TK_11AZ_LEN];
	__le16 calib[IWL_TOF_BW_NUM];
	__le16 beacon_interval;
	u8 rx_pn[IEEE80211_CCMP_PN_LEN];
	u8 tx_pn[IEEE80211_CCMP_PN_LEN];
} __packed;  
#define IWL_LOCATION_MAX_STS_POS	3
struct iwl_tof_range_req_ap_entry_v8 {
	__le32 initiator_ap_flags;
	u8 channel_num;
	u8 format_bw;
	u8 ctrl_ch_position;
	u8 ftmr_max_retries;
	u8 bssid[ETH_ALEN];
	__le16 burst_period;
	u8 samples_per_burst;
	u8 num_of_bursts;
	u8 sta_id;
	u8 cipher;
	u8 hltk[HLTK_11AZ_LEN];
	u8 tk[TK_11AZ_LEN];
	__le16 calib[IWL_TOF_BW_NUM];
	__le16 beacon_interval;
	u8 rx_pn[IEEE80211_CCMP_PN_LEN];
	u8 tx_pn[IEEE80211_CCMP_PN_LEN];
	u8 r2i_ndp_params;
	u8 i2r_ndp_params;
	u8 r2i_max_total_ltf;
	u8 i2r_max_total_ltf;
} __packed;  
struct iwl_tof_range_req_ap_entry_v9 {
	__le32 initiator_ap_flags;
	u8 channel_num;
	u8 format_bw;
	u8 ctrl_ch_position;
	u8 ftmr_max_retries;
	u8 bssid[ETH_ALEN];
	__le16 burst_period;
	u8 samples_per_burst;
	u8 num_of_bursts;
	u8 sta_id;
	u8 cipher;
	u8 hltk[HLTK_11AZ_LEN];
	u8 tk[TK_11AZ_LEN];
	__le16 calib[IWL_TOF_BW_NUM];
	u16 beacon_interval;
	u8 rx_pn[IEEE80211_CCMP_PN_LEN];
	u8 tx_pn[IEEE80211_CCMP_PN_LEN];
	u8 r2i_ndp_params;
	u8 i2r_ndp_params;
	u8 r2i_max_total_ltf;
	u8 i2r_max_total_ltf;
	u8 bss_color;
	u8 band;
	__le16 min_time_between_msr;
} __packed;  
enum iwl_tof_response_mode {
	IWL_MVM_TOF_RESPONSE_ASAP,
	IWL_MVM_TOF_RESPONSE_TIMEOUT,
	IWL_MVM_TOF_RESPONSE_COMPLETE,
};
enum iwl_tof_initiator_flags {
	IWL_TOF_INITIATOR_FLAGS_FAST_ALGO_DISABLED = BIT(0),
	IWL_TOF_INITIATOR_FLAGS_RX_CHAIN_SEL_A = BIT(1),
	IWL_TOF_INITIATOR_FLAGS_RX_CHAIN_SEL_B = BIT(2),
	IWL_TOF_INITIATOR_FLAGS_RX_CHAIN_SEL_C = BIT(3),
	IWL_TOF_INITIATOR_FLAGS_TX_CHAIN_SEL_A = BIT(4),
	IWL_TOF_INITIATOR_FLAGS_TX_CHAIN_SEL_B = BIT(5),
	IWL_TOF_INITIATOR_FLAGS_TX_CHAIN_SEL_C = BIT(6),
	IWL_TOF_INITIATOR_FLAGS_MACADDR_RANDOM = BIT(7),
	IWL_TOF_INITIATOR_FLAGS_SPECIFIC_CALIB = BIT(15),
	IWL_TOF_INITIATOR_FLAGS_COMMON_CALIB   = BIT(16),
	IWL_TOF_INITIATOR_FLAGS_NON_ASAP_SUPPORT = BIT(20),
};  
#define IWL_MVM_TOF_MAX_APS 5
#define IWL_MVM_TOF_MAX_TWO_SIDED_APS 5
struct iwl_tof_range_req_cmd_v5 {
	__le32 initiator_flags;
	u8 request_id;
	u8 initiator;
	u8 one_sided_los_disable;
	u8 req_timeout;
	u8 report_policy;
	u8 reserved0;
	u8 num_of_ap;
	u8 macaddr_random;
	u8 range_req_bssid[ETH_ALEN];
	u8 macaddr_template[ETH_ALEN];
	u8 macaddr_mask[ETH_ALEN];
	u8 ftm_rx_chains;
	u8 ftm_tx_chains;
	__le16 common_calib;
	__le16 specific_calib;
	struct iwl_tof_range_req_ap_entry_v2 ap[IWL_MVM_TOF_MAX_APS];
} __packed;
struct iwl_tof_range_req_cmd_v7 {
	__le32 initiator_flags;
	u8 request_id;
	u8 num_of_ap;
	u8 range_req_bssid[ETH_ALEN];
	u8 macaddr_mask[ETH_ALEN];
	u8 macaddr_template[ETH_ALEN];
	__le32 req_timeout_ms;
	__le32 tsf_mac_id;
	__le16 common_calib;
	__le16 specific_calib;
	struct iwl_tof_range_req_ap_entry_v3 ap[IWL_MVM_TOF_MAX_APS];
} __packed;  
struct iwl_tof_range_req_cmd_v8 {
	__le32 initiator_flags;
	u8 request_id;
	u8 num_of_ap;
	u8 range_req_bssid[ETH_ALEN];
	u8 macaddr_mask[ETH_ALEN];
	u8 macaddr_template[ETH_ALEN];
	__le32 req_timeout_ms;
	__le32 tsf_mac_id;
	__le16 common_calib;
	__le16 specific_calib;
	struct iwl_tof_range_req_ap_entry_v4 ap[IWL_MVM_TOF_MAX_APS];
} __packed;  
struct iwl_tof_range_req_cmd_v9 {
	__le32 initiator_flags;
	u8 request_id;
	u8 num_of_ap;
	u8 range_req_bssid[ETH_ALEN];
	u8 macaddr_mask[ETH_ALEN];
	u8 macaddr_template[ETH_ALEN];
	__le32 req_timeout_ms;
	__le32 tsf_mac_id;
	struct iwl_tof_range_req_ap_entry_v6 ap[IWL_MVM_TOF_MAX_APS];
} __packed;  
struct iwl_tof_range_req_cmd_v11 {
	__le32 initiator_flags;
	u8 request_id;
	u8 num_of_ap;
	u8 range_req_bssid[ETH_ALEN];
	u8 macaddr_mask[ETH_ALEN];
	u8 macaddr_template[ETH_ALEN];
	__le32 req_timeout_ms;
	__le32 tsf_mac_id;
	struct iwl_tof_range_req_ap_entry_v7 ap[IWL_MVM_TOF_MAX_APS];
} __packed;  
struct iwl_tof_range_req_cmd_v12 {
	__le32 initiator_flags;
	u8 request_id;
	u8 num_of_ap;
	u8 range_req_bssid[ETH_ALEN];
	u8 macaddr_mask[ETH_ALEN];
	u8 macaddr_template[ETH_ALEN];
	__le32 req_timeout_ms;
	__le32 tsf_mac_id;
	struct iwl_tof_range_req_ap_entry_v8 ap[IWL_MVM_TOF_MAX_APS];
} __packed;  
struct iwl_tof_range_req_cmd_v13 {
	__le32 initiator_flags;
	u8 request_id;
	u8 num_of_ap;
	u8 range_req_bssid[ETH_ALEN];
	u8 macaddr_mask[ETH_ALEN];
	u8 macaddr_template[ETH_ALEN];
	__le32 req_timeout_ms;
	__le32 tsf_mac_id;
	struct iwl_tof_range_req_ap_entry_v9 ap[IWL_MVM_TOF_MAX_APS];
} __packed;  
enum iwl_tof_range_request_status {
	IWL_TOF_RANGE_REQUEST_STATUS_SUCCESS,
	IWL_TOF_RANGE_REQUEST_STATUS_BUSY,
};
enum iwl_tof_entry_status {
	IWL_TOF_ENTRY_SUCCESS = 0,
	IWL_TOF_ENTRY_GENERAL_FAILURE = 1,
	IWL_TOF_ENTRY_NO_RESPONSE = 2,
	IWL_TOF_ENTRY_REQUEST_REJECTED = 3,
	IWL_TOF_ENTRY_NOT_SCHEDULED = 4,
	IWL_TOF_ENTRY_TIMING_MEASURE_TIMEOUT = 5,
	IWL_TOF_ENTRY_TARGET_DIFF_CH_CANNOT_CHANGE = 6,
	IWL_TOF_ENTRY_RANGE_NOT_SUPPORTED = 7,
	IWL_TOF_ENTRY_REQUEST_ABORT_UNKNOWN_REASON = 8,
	IWL_TOF_ENTRY_LOCATION_INVALID_T1_T4_TIME_STAMP = 9,
	IWL_TOF_ENTRY_11MC_PROTOCOL_FAILURE = 10,
	IWL_TOF_ENTRY_REQUEST_CANNOT_SCHED = 11,
	IWL_TOF_ENTRY_RESPONDER_CANNOT_COLABORATE = 12,
	IWL_TOF_ENTRY_BAD_REQUEST_ARGS = 13,
	IWL_TOF_ENTRY_WIFI_NOT_ENABLED = 14,
	IWL_TOF_ENTRY_RESPONDER_OVERRIDE_PARAMS = 15,
};  
struct iwl_tof_range_rsp_ap_entry_ntfy_v3 {
	u8 bssid[ETH_ALEN];
	u8 measure_status;
	u8 measure_bw;
	__le32 rtt;
	__le32 rtt_variance;
	__le32 rtt_spread;
	s8 rssi;
	u8 rssi_spread;
	u8 reserved;
	u8 refusal_period;
	__le32 range;
	__le32 range_variance;
	__le32 timestamp;
	__le32 t2t3_initiator;
	__le32 t1t4_responder;
	__le16 common_calib;
	__le16 specific_calib;
	__le32 papd_calib_output;
} __packed;  
struct iwl_tof_range_rsp_ap_entry_ntfy_v4 {
	u8 bssid[ETH_ALEN];
	u8 measure_status;
	u8 measure_bw;
	__le32 rtt;
	__le32 rtt_variance;
	__le32 rtt_spread;
	s8 rssi;
	u8 rssi_spread;
	u8 last_burst;
	u8 refusal_period;
	__le32 timestamp;
	__le32 start_tsf;
	__le32 rx_rate_n_flags;
	__le32 tx_rate_n_flags;
	__le32 t2t3_initiator;
	__le32 t1t4_responder;
	__le16 common_calib;
	__le16 specific_calib;
	__le32 papd_calib_output;
} __packed;  
struct iwl_tof_range_rsp_ap_entry_ntfy_v5 {
	u8 bssid[ETH_ALEN];
	u8 measure_status;
	u8 measure_bw;
	__le32 rtt;
	__le32 rtt_variance;
	__le32 rtt_spread;
	s8 rssi;
	u8 rssi_spread;
	u8 last_burst;
	u8 refusal_period;
	__le32 timestamp;
	__le32 start_tsf;
	__le32 rx_rate_n_flags;
	__le32 tx_rate_n_flags;
	__le32 t2t3_initiator;
	__le32 t1t4_responder;
	__le16 common_calib;
	__le16 specific_calib;
	__le32 papd_calib_output;
	u8 rttConfidence;
	u8 reserved[3];
} __packed;  
struct iwl_tof_range_rsp_ap_entry_ntfy_v6 {
	u8 bssid[ETH_ALEN];
	u8 measure_status;
	u8 measure_bw;
	__le32 rtt;
	__le32 rtt_variance;
	__le32 rtt_spread;
	s8 rssi;
	u8 rssi_spread;
	u8 last_burst;
	u8 refusal_period;
	__le32 timestamp;
	__le32 start_tsf;
	__le32 rx_rate_n_flags;
	__le32 tx_rate_n_flags;
	__le32 t2t3_initiator;
	__le32 t1t4_responder;
	__le16 common_calib;
	__le16 specific_calib;
	__le32 papd_calib_output;
	u8 rttConfidence;
	u8 reserved[3];
	u8 rx_pn[IEEE80211_CCMP_PN_LEN];
	u8 tx_pn[IEEE80211_CCMP_PN_LEN];
} __packed;  
enum iwl_tof_response_status {
	IWL_TOF_RESPONSE_SUCCESS = 0,
	IWL_TOF_RESPONSE_TIMEOUT = 1,
	IWL_TOF_RESPONSE_ABORTED = 4,
	IWL_TOF_RESPONSE_FAILED  = 5,
};  
struct iwl_tof_range_rsp_ntfy_v5 {
	u8 request_id;
	u8 request_status;
	u8 last_in_batch;
	u8 num_of_aps;
	struct iwl_tof_range_rsp_ap_entry_ntfy_v3 ap[IWL_MVM_TOF_MAX_APS];
} __packed;  
struct iwl_tof_range_rsp_ntfy_v6 {
	u8 request_id;
	u8 num_of_aps;
	u8 last_report;
	u8 reserved;
	struct iwl_tof_range_rsp_ap_entry_ntfy_v4 ap[IWL_MVM_TOF_MAX_APS];
} __packed;  
struct iwl_tof_range_rsp_ntfy_v7 {
	u8 request_id;
	u8 num_of_aps;
	u8 last_report;
	u8 reserved;
	struct iwl_tof_range_rsp_ap_entry_ntfy_v5 ap[IWL_MVM_TOF_MAX_APS];
} __packed;  
struct iwl_tof_range_rsp_ntfy_v8 {
	u8 request_id;
	u8 num_of_aps;
	u8 last_report;
	u8 reserved;
	struct iwl_tof_range_rsp_ap_entry_ntfy_v6 ap[IWL_MVM_TOF_MAX_APS];
} __packed;  
#define IWL_MVM_TOF_MCSI_BUF_SIZE  (245)
struct iwl_tof_mcsi_notif {
	u8 token;
	u8 role;
	__le16 reserved;
	u8 initiator_bssid[ETH_ALEN];
	u8 responder_bssid[ETH_ALEN];
	u8 mcsi_buffer[IWL_MVM_TOF_MCSI_BUF_SIZE * 4];
} __packed;
struct iwl_tof_range_abort_cmd {
	u8 request_id;
	u8 reserved[3];
} __packed;
enum ftm_responder_stats_flags {
	FTM_RESP_STAT_NON_ASAP_STARTED = BIT(0),
	FTM_RESP_STAT_NON_ASAP_IN_WIN = BIT(1),
	FTM_RESP_STAT_NON_ASAP_OUT_WIN = BIT(2),
	FTM_RESP_STAT_TRIGGER_DUP = BIT(3),
	FTM_RESP_STAT_DUP = BIT(4),
	FTM_RESP_STAT_DUP_IN_WIN = BIT(5),
	FTM_RESP_STAT_DUP_OUT_WIN = BIT(6),
	FTM_RESP_STAT_SCHED_SUCCESS = BIT(7),
	FTM_RESP_STAT_ASAP_REQ = BIT(8),
	FTM_RESP_STAT_NON_ASAP_REQ = BIT(9),
	FTM_RESP_STAT_ASAP_RESP = BIT(10),
	FTM_RESP_STAT_NON_ASAP_RESP = BIT(11),
	FTM_RESP_STAT_FAIL_INITIATOR_INACTIVE = BIT(12),
	FTM_RESP_STAT_FAIL_INITIATOR_OUT_WIN = BIT(13),
	FTM_RESP_STAT_FAIL_INITIATOR_RETRY_LIM = BIT(14),
	FTM_RESP_STAT_FAIL_NEXT_SERVED = BIT(15),
	FTM_RESP_STAT_FAIL_TRIGGER_ERR = BIT(16),
	FTM_RESP_STAT_FAIL_GC = BIT(17),
	FTM_RESP_STAT_SUCCESS = BIT(18),
	FTM_RESP_STAT_INTEL_IE = BIT(19),
	FTM_RESP_STAT_INITIATOR_ACTIVE = BIT(20),
	FTM_RESP_STAT_MEASUREMENTS_AVAILABLE = BIT(21),
	FTM_RESP_STAT_TRIGGER_UNKNOWN = BIT(22),
	FTM_RESP_STAT_PROCESS_FAIL = BIT(23),
	FTM_RESP_STAT_ACK = BIT(24),
	FTM_RESP_STAT_NACK = BIT(25),
	FTM_RESP_STAT_INVALID_INITIATOR_ID = BIT(26),
	FTM_RESP_STAT_TIMER_MIN_DELTA = BIT(27),
	FTM_RESP_STAT_INITIATOR_REMOVED = BIT(28),
	FTM_RESP_STAT_INITIATOR_ADDED = BIT(29),
	FTM_RESP_STAT_ERR_LIST_FULL = BIT(30),
	FTM_RESP_STAT_INITIATOR_SCHED_NOW = BIT(31),
};  
struct iwl_ftm_responder_stats {
	u8 addr[ETH_ALEN];
	u8 success_ftm;
	u8 ftm_per_burst;
	__le32 flags;
	__le32 duration;
	__le32 allocated_duration;
	u8 bw;
	u8 rate;
	__le16 reserved;
} __packed;  
#define IWL_CSI_MAX_EXPECTED_CHUNKS		16
#define IWL_CSI_CHUNK_CTL_NUM_MASK_VER_1	0x0003
#define IWL_CSI_CHUNK_CTL_IDX_MASK_VER_1	0x000c
#define IWL_CSI_CHUNK_CTL_NUM_MASK_VER_2	0x00ff
#define IWL_CSI_CHUNK_CTL_IDX_MASK_VER_2	0xff00
struct iwl_csi_chunk_notification {
	__le32 token;
	__le16 seq;
	__le16 ctl;
	__le32 size;
	u8 data[];
} __packed;  
#endif  
