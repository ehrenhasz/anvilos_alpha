#ifndef __iwl_fw_api_mac_h__
#define __iwl_fw_api_mac_h__
#define MAC_INDEX_AUX		4
#define MAC_INDEX_MIN_DRIVER	0
#define NUM_MAC_INDEX_DRIVER	MAC_INDEX_AUX
#define NUM_MAC_INDEX		(NUM_MAC_INDEX_DRIVER + 1)
#define NUM_MAC_INDEX_CDB	(NUM_MAC_INDEX_DRIVER + 2)
#define IWL_MVM_STATION_COUNT_MAX	16
#define IWL_MVM_INVALID_STA		0xFF
enum iwl_ac {
	AC_BK,
	AC_BE,
	AC_VI,
	AC_VO,
	AC_NUM,
};
enum iwl_mac_protection_flags {
	MAC_PROT_FLG_TGG_PROTECT	= BIT(3),
	MAC_PROT_FLG_HT_PROT		= BIT(23),
	MAC_PROT_FLG_FAT_PROT		= BIT(24),
	MAC_PROT_FLG_SELF_CTS_EN	= BIT(30),
};
#define MAC_FLG_SHORT_SLOT		BIT(4)
#define MAC_FLG_SHORT_PREAMBLE		BIT(5)
enum iwl_mac_types {
	FW_MAC_TYPE_FIRST = 1,
	FW_MAC_TYPE_AUX = FW_MAC_TYPE_FIRST,
	FW_MAC_TYPE_LISTENER,
	FW_MAC_TYPE_PIBSS,
	FW_MAC_TYPE_IBSS,
	FW_MAC_TYPE_BSS_STA,
	FW_MAC_TYPE_P2P_DEVICE,
	FW_MAC_TYPE_P2P_STA,
	FW_MAC_TYPE_GO,
	FW_MAC_TYPE_TEST,
	FW_MAC_TYPE_MAX = FW_MAC_TYPE_TEST
};  
enum iwl_tsf_id {
	TSF_ID_A = 0,
	TSF_ID_B = 1,
	TSF_ID_C = 2,
	TSF_ID_D = 3,
	NUM_TSF_IDS = 4,
};  
struct iwl_mac_data_ap {
	__le32 beacon_time;
	__le64 beacon_tsf;
	__le32 bi;
	__le32 reserved1;
	__le32 dtim_interval;
	__le32 reserved2;
	__le32 mcast_qid;
	__le32 beacon_template;
} __packed;  
struct iwl_mac_data_ibss {
	__le32 beacon_time;
	__le64 beacon_tsf;
	__le32 bi;
	__le32 reserved;
	__le32 beacon_template;
} __packed;  
enum iwl_mac_data_policy {
	TWT_SUPPORTED = BIT(0),
	MORE_DATA_ACK_SUPPORTED = BIT(1),
	FLEXIBLE_TWT_SUPPORTED = BIT(2),
	PROTECTED_TWT_SUPPORTED = BIT(3),
	BROADCAST_TWT_SUPPORTED = BIT(4),
	COEX_HIGH_PRIORITY_ENABLE = BIT(5),
};
struct iwl_mac_data_sta {
	__le32 is_assoc;
	__le32 dtim_time;
	__le64 dtim_tsf;
	__le32 bi;
	__le32 reserved1;
	__le32 dtim_interval;
	__le32 data_policy;
	__le32 listen_interval;
	__le32 assoc_id;
	__le32 assoc_beacon_arrive_time;
} __packed;  
struct iwl_mac_data_go {
	struct iwl_mac_data_ap ap;
	__le32 ctwin;
	__le32 opp_ps_enabled;
} __packed;  
struct iwl_mac_data_p2p_sta {
	struct iwl_mac_data_sta sta;
	__le32 ctwin;
} __packed;  
struct iwl_mac_data_pibss {
	__le32 stats_interval;
} __packed;  
struct iwl_mac_data_p2p_dev {
	__le32 is_disc_extended;
} __packed;  
enum iwl_mac_filter_flags {
	MAC_FILTER_IN_PROMISC		= BIT(0),
	MAC_FILTER_IN_CONTROL_AND_MGMT	= BIT(1),
	MAC_FILTER_ACCEPT_GRP		= BIT(2),
	MAC_FILTER_DIS_DECRYPT		= BIT(3),
	MAC_FILTER_DIS_GRP_DECRYPT	= BIT(4),
	MAC_FILTER_IN_BEACON		= BIT(6),
	MAC_FILTER_OUT_BCAST		= BIT(8),
	MAC_FILTER_IN_CRC32		= BIT(11),
	MAC_FILTER_IN_PROBE_REQUEST	= BIT(12),
	MAC_FILTER_IN_11AX		= BIT(14),
};
enum iwl_mac_qos_flags {
	MAC_QOS_FLG_UPDATE_EDCA	= BIT(0),
	MAC_QOS_FLG_TGN		= BIT(1),
	MAC_QOS_FLG_TXOP_TYPE	= BIT(4),
};
struct iwl_ac_qos {
	__le16 cw_min;
	__le16 cw_max;
	u8 aifsn;
	u8 fifos_mask;
	__le16 edca_txop;
} __packed;  
struct iwl_mac_ctx_cmd {
	__le32 id_and_color;
	__le32 action;
	__le32 mac_type;
	__le32 tsf_id;
	u8 node_addr[6];
	__le16 reserved_for_node_addr;
	u8 bssid_addr[6];
	__le16 reserved_for_bssid_addr;
	__le32 cck_rates;
	__le32 ofdm_rates;
	__le32 protection_flags;
	__le32 cck_short_preamble;
	__le32 short_slot;
	__le32 filter_flags;
	__le32 qos_flags;
	struct iwl_ac_qos ac[AC_NUM+1];
	union {
		struct iwl_mac_data_ap ap;
		struct iwl_mac_data_go go;
		struct iwl_mac_data_sta sta;
		struct iwl_mac_data_p2p_sta p2p_sta;
		struct iwl_mac_data_p2p_dev p2p_dev;
		struct iwl_mac_data_pibss pibss;
		struct iwl_mac_data_ibss ibss;
	};
} __packed;  
#define IWL_NONQOS_SEQ_GET	0x1
#define IWL_NONQOS_SEQ_SET	0x2
struct iwl_nonqos_seq_query_cmd {
	__le32 get_set_flag;
	__le32 mac_id_n_color;
	__le16 value;
	__le16 reserved;
} __packed;  
struct iwl_missed_beacons_notif_ver_3 {
	__le32 mac_id;
	__le32 consec_missed_beacons_since_last_rx;
	__le32 consec_missed_beacons;
	__le32 num_expected_beacons;
	__le32 num_recvd_beacons;
} __packed;  
struct iwl_missed_beacons_notif {
	__le32 link_id;
	__le32 consec_missed_beacons_since_last_rx;
	__le32 consec_missed_beacons;
	__le32 num_expected_beacons;
	__le32 num_recvd_beacons;
} __packed;  
struct iwl_he_backoff_conf {
	__le16 cwmin;
	__le16 cwmax;
	__le16 aifsn;
	__le16 mu_time;
} __packed;  
enum iwl_he_pkt_ext_constellations {
	IWL_HE_PKT_EXT_BPSK = 0,
	IWL_HE_PKT_EXT_QPSK,
	IWL_HE_PKT_EXT_16QAM,
	IWL_HE_PKT_EXT_64QAM,
	IWL_HE_PKT_EXT_256QAM,
	IWL_HE_PKT_EXT_1024QAM,
	IWL_HE_PKT_EXT_4096QAM,
	IWL_HE_PKT_EXT_NONE,
};
#define MAX_HE_SUPP_NSS	2
#define MAX_CHANNEL_BW_INDX_API_D_VER_2	4
#define MAX_CHANNEL_BW_INDX_API_D_VER_3	5
struct iwl_he_pkt_ext_v1 {
	u8 pkt_ext_qam_th[MAX_HE_SUPP_NSS][MAX_CHANNEL_BW_INDX_API_D_VER_2][2];
} __packed;  
struct iwl_he_pkt_ext_v2 {
	u8 pkt_ext_qam_th[MAX_HE_SUPP_NSS][MAX_CHANNEL_BW_INDX_API_D_VER_3][2];
} __packed;  
enum iwl_he_sta_ctxt_flags {
	STA_CTXT_HE_REF_BSSID_VALID		= BIT(4),
	STA_CTXT_HE_BSS_COLOR_DIS		= BIT(5),
	STA_CTXT_HE_PARTIAL_BSS_COLOR		= BIT(6),
	STA_CTXT_HE_32BIT_BA_BITMAP		= BIT(7),
	STA_CTXT_HE_PACKET_EXT			= BIT(8),
	STA_CTXT_HE_TRIG_RND_ALLOC		= BIT(9),
	STA_CTXT_HE_CONST_TRIG_RND_ALLOC	= BIT(10),
	STA_CTXT_HE_ACK_ENABLED			= BIT(11),
	STA_CTXT_HE_MU_EDCA_CW			= BIT(12),
	STA_CTXT_HE_NIC_NOT_ACK_ENABLED		= BIT(13),
	STA_CTXT_HE_RU_2MHZ_BLOCK		= BIT(14),
	STA_CTXT_HE_NDP_FEEDBACK_ENABLED	= BIT(15),
	STA_CTXT_EHT_PUNCTURE_MASK_VALID	= BIT(16),
	STA_CTXT_EHT_LONG_PPE_ENABLED		= BIT(17),
};
enum iwl_he_htc_flags {
	IWL_HE_HTC_SUPPORT			= BIT(0),
	IWL_HE_HTC_UL_MU_RESP_SCHED		= BIT(3),
	IWL_HE_HTC_BSR_SUPP			= BIT(4),
	IWL_HE_HTC_OMI_SUPP			= BIT(5),
	IWL_HE_HTC_BQR_SUPP			= BIT(6),
};
#define IWL_HE_HTC_LINK_ADAP_POS		(1)
#define IWL_HE_HTC_LINK_ADAP_NO_FEEDBACK	(0)
#define IWL_HE_HTC_LINK_ADAP_UNSOLICITED	(2 << IWL_HE_HTC_LINK_ADAP_POS)
#define IWL_HE_HTC_LINK_ADAP_BOTH		(3 << IWL_HE_HTC_LINK_ADAP_POS)
struct iwl_he_sta_context_cmd_v1 {
	u8 sta_id;
	u8 tid_limit;
	u8 reserved1;
	u8 reserved2;
	__le32 flags;
	u8 ref_bssid_addr[6];
	__le16 reserved0;
	__le32 htc_flags;
	u8 frag_flags;
	u8 frag_level;
	u8 frag_max_num;
	u8 frag_min_size;
	struct iwl_he_pkt_ext_v1 pkt_ext;
	u8 bss_color;
	u8 htc_trig_based_pkt_ext;
	__le16 frame_time_rts_th;
	u8 rand_alloc_ecwmin;
	u8 rand_alloc_ecwmax;
	__le16 reserved3;
	struct iwl_he_backoff_conf trig_based_txf[AC_NUM];
} __packed;  
struct iwl_he_sta_context_cmd_v2 {
	u8 sta_id;
	u8 tid_limit;
	u8 reserved1;
	u8 reserved2;
	__le32 flags;
	u8 ref_bssid_addr[6];
	__le16 reserved0;
	__le32 htc_flags;
	u8 frag_flags;
	u8 frag_level;
	u8 frag_max_num;
	u8 frag_min_size;
	struct iwl_he_pkt_ext_v1 pkt_ext;
	u8 bss_color;
	u8 htc_trig_based_pkt_ext;
	__le16 frame_time_rts_th;
	u8 rand_alloc_ecwmin;
	u8 rand_alloc_ecwmax;
	__le16 reserved3;
	struct iwl_he_backoff_conf trig_based_txf[AC_NUM];
	u8 max_bssid_indicator;
	u8 bssid_index;
	u8 ema_ap;
	u8 profile_periodicity;
	u8 bssid_count;
	u8 reserved4[3];
} __packed;  
struct iwl_he_sta_context_cmd_v3 {
	u8 sta_id;
	u8 tid_limit;
	u8 reserved1;
	u8 reserved2;
	__le32 flags;
	u8 ref_bssid_addr[6];
	__le16 reserved0;
	__le32 htc_flags;
	u8 frag_flags;
	u8 frag_level;
	u8 frag_max_num;
	u8 frag_min_size;
	struct iwl_he_pkt_ext_v2 pkt_ext;
	u8 bss_color;
	u8 htc_trig_based_pkt_ext;
	__le16 frame_time_rts_th;
	u8 rand_alloc_ecwmin;
	u8 rand_alloc_ecwmax;
	__le16 puncture_mask;
	struct iwl_he_backoff_conf trig_based_txf[AC_NUM];
	u8 max_bssid_indicator;
	u8 bssid_index;
	u8 ema_ap;
	u8 profile_periodicity;
	u8 bssid_count;
	u8 reserved4[3];
} __packed;  
struct iwl_he_monitor_cmd {
	u8 bssid[6];
	__le16 reserved1;
	__le16 aid;
	u8 reserved2[6];
} __packed;  
#endif  
