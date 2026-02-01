 
 
#ifndef __iwl_fw_api_mac_cfg_h__
#define __iwl_fw_api_mac_cfg_h__

#include "mac.h"

 
enum iwl_mac_conf_subcmd_ids {
	 
	LOW_LATENCY_CMD = 0x3,
	 
	CHANNEL_SWITCH_TIME_EVENT_CMD = 0x4,
	 
	MISSED_VAP_NOTIF = 0xFA,
	 
	SESSION_PROTECTION_CMD = 0x5,
	 
	CANCEL_CHANNEL_SWITCH_CMD = 0x6,
	 
	MAC_CONFIG_CMD = 0x8,
	 
	LINK_CONFIG_CMD = 0x9,
	 
	STA_CONFIG_CMD = 0xA,
	 
	AUX_STA_CMD = 0xB,
	 
	STA_REMOVE_CMD = 0xC,
	 
	STA_DISABLE_TX_CMD = 0xD,
	 
	SESSION_PROTECTION_NOTIF = 0xFB,

	 
	PROBE_RESPONSE_DATA_NOTIF = 0xFC,

	 
	CHANNEL_SWITCH_START_NOTIF = 0xFF,

	 
	CHANNEL_SWITCH_ERROR_NOTIF = 0xF9,
};

#define IWL_P2P_NOA_DESC_COUNT	(2)

 
struct iwl_p2p_noa_attr {
	u8 id;
	u8 len_low;
	u8 len_high;
	u8 idx;
	u8 ctwin;
	struct ieee80211_p2p_noa_desc desc[IWL_P2P_NOA_DESC_COUNT];
	u8 reserved;
} __packed;

#define IWL_PROBE_RESP_DATA_NO_CSA (0xff)

 
struct iwl_probe_resp_data_notif {
	__le32 mac_id;
	__le32 noa_active;
	struct iwl_p2p_noa_attr noa_attr;
	u8 csa_counter;
	u8 reserved[3];
} __packed;  

 
struct iwl_missed_vap_notif {
	__le32 mac_id;
	u8 num_beacon_intervals_elapsed;
	u8 profile_periodicity;
	u8 reserved[2];
} __packed;  

 
struct iwl_channel_switch_start_notif_v1 {
	__le32 id_and_color;
} __packed;  

 
struct iwl_channel_switch_start_notif {
	__le32 link_id;
} __packed;  

#define CS_ERR_COUNT_ERROR BIT(0)
#define CS_ERR_LONG_DELAY_AFTER_CS BIT(1)
#define CS_ERR_LONG_TX_BLOCK BIT(2)
#define CS_ERR_TX_BLOCK_TIMER_EXPIRED BIT(3)

 
struct iwl_channel_switch_error_notif_v1 {
	__le32 mac_id;
	__le32 csa_err_mask;
} __packed;  

 
struct iwl_channel_switch_error_notif {
	__le32 link_id;
	__le32 csa_err_mask;
} __packed;  

 
struct iwl_cancel_channel_switch_cmd {
	__le32 id;
} __packed;  

 
struct iwl_chan_switch_te_cmd {
	__le32 mac_id;
	__le32 action;
	__le32 tsf;
	u8 cs_count;
	u8 cs_delayed_bcn_count;
	u8 cs_mode;
	u8 reserved;
} __packed;  

 
struct iwl_mac_low_latency_cmd {
	__le32 mac_id;
	u8 low_latency_rx;
	u8 low_latency_tx;
	__le16 reserved;
} __packed;  

 
struct iwl_mac_client_data {
	u8 is_assoc;
	u8 esr_transition_timeout;
	__le16 medium_sync_delay;

	__le16 assoc_id;
	__le16 reserved1;
	__le16 data_policy;
	__le16 reserved2;
	__le32 ctwin;
} __packed;  

 
struct iwl_mac_p2p_dev_data {
	__le32 is_disc_extended;
} __packed;  

 
enum iwl_mac_config_filter_flags {
	MAC_CFG_FILTER_PROMISC			= BIT(0),
	MAC_CFG_FILTER_ACCEPT_CONTROL_AND_MGMT	= BIT(1),
	MAC_CFG_FILTER_ACCEPT_GRP		= BIT(2),
	MAC_CFG_FILTER_ACCEPT_BEACON		= BIT(3),
	MAC_CFG_FILTER_ACCEPT_BCAST_PROBE_RESP	= BIT(4),
	MAC_CFG_FILTER_ACCEPT_PROBE_REQ		= BIT(5),
};  

 
struct iwl_mac_config_cmd {
	 
	__le32 id_and_color;
	__le32 action;
	 
	__le32 mac_type;
	u8 local_mld_addr[6];
	__le16 reserved_for_local_mld_addr;
	__le32 filter_flags;
	__le16 he_support;
	__le16 he_ap_support;
	__le32 eht_support;
	__le32 nic_not_ack_enabled;
	 
	union {
		struct iwl_mac_client_data client;
		struct iwl_mac_p2p_dev_data p2p_dev;
	};
} __packed;  

 
enum iwl_link_ctx_modify_flags {
	LINK_CONTEXT_MODIFY_ACTIVE		= BIT(0),
	LINK_CONTEXT_MODIFY_RATES_INFO		= BIT(1),
	LINK_CONTEXT_MODIFY_PROTECT_FLAGS	= BIT(2),
	LINK_CONTEXT_MODIFY_QOS_PARAMS		= BIT(3),
	LINK_CONTEXT_MODIFY_BEACON_TIMING	= BIT(4),
	LINK_CONTEXT_MODIFY_HE_PARAMS		= BIT(5),
	LINK_CONTEXT_MODIFY_BSS_COLOR_DISABLE	= BIT(6),
	LINK_CONTEXT_MODIFY_EHT_PARAMS		= BIT(7),
	LINK_CONTEXT_MODIFY_ALL			= 0xff,
};  

 
enum iwl_link_ctx_protection_flags {
	LINK_PROT_FLG_TGG_PROTECT	= BIT(0),
	LINK_PROT_FLG_HT_PROT		= BIT(1),
	LINK_PROT_FLG_FAT_PROT		= BIT(2),
	LINK_PROT_FLG_SELF_CTS_EN	= BIT(3),
};  

 
enum iwl_link_ctx_flags {
	LINK_FLG_BSS_COLOR_DIS		= BIT(0),
	LINK_FLG_MU_EDCA_CW		= BIT(1),
	LINK_FLG_RU_2MHZ_BLOCK		= BIT(2),
	LINK_FLG_NDP_FEEDBACK_ENABLED	= BIT(3),
};  

 
struct iwl_link_config_cmd {
	__le32 action;
	__le32 link_id;
	__le32 mac_id;
	__le32 phy_id;
	u8 local_link_addr[6];
	__le16 reserved_for_local_link_addr;
	__le32 modify_mask;
	__le32 active;
	__le32 listen_lmac;
	__le32 cck_rates;
	__le32 ofdm_rates;
	__le32 cck_short_preamble;
	__le32 short_slot;
	__le32 protection_flags;
	 
	__le32 qos_flags;
	struct iwl_ac_qos ac[AC_NUM + 1];
	u8 htc_trig_based_pkt_ext;
	u8 rand_alloc_ecwmin;
	u8 rand_alloc_ecwmax;
	u8 ndp_fdbk_buff_th_exp;
	struct iwl_he_backoff_conf trig_based_txf[AC_NUM];
	__le32 bi;
	__le32 dtim_interval;
	__le16 puncture_mask;
	__le16 frame_time_rts_th;
	__le32 flags;
	__le32 flags_mask;
	 
	u8 ref_bssid_addr[6];
	__le16 reserved_for_ref_bssid_addr;
	u8 bssid_index;
	u8 bss_color;
	u8 spec_link_id;
	u8 reserved;
	u8 ibss_bssid_addr[6];
	__le16 reserved_for_ibss_bssid_addr;
	__le32 reserved1[8];
} __packed;  

 
#define IWL_MVM_FW_MAX_ACTIVE_LINKS_NUM 2
#define IWL_MVM_FW_MAX_LINK_ID 3

 
enum iwl_fw_sta_type {
	STATION_TYPE_PEER,
	STATION_TYPE_BCAST_MGMT,
	STATION_TYPE_MCAST,
	STATION_TYPE_AUX,
};  

 
struct iwl_mvm_sta_cfg_cmd {
	__le32 sta_id;
	__le32 link_id;
	u8 peer_mld_address[ETH_ALEN];
	__le16 reserved_for_peer_mld_address;
	u8 peer_link_address[ETH_ALEN];
	__le16 reserved_for_peer_link_address;
	__le32 station_type;
	__le32 assoc_id;
	__le32 beamform_flags;
	__le32 mfp;
	__le32 mimo;
	__le32 mimo_protection;
	__le32 ack_enabled;
	__le32 trig_rnd_alloc;
	__le32 tx_ampdu_spacing;
	__le32 tx_ampdu_max_size;
	__le32 sp_length;
	__le32 uapsd_acs;
	struct iwl_he_pkt_ext_v2 pkt_ext;
	__le32 htc_flags;
} __packed;  

 
struct iwl_mvm_aux_sta_cmd {
	__le32 sta_id;
	__le32 lmac_id;
	u8 mac_addr[ETH_ALEN];
	__le16 reserved_for_mac_addr;

} __packed;  

 
struct iwl_mvm_remove_sta_cmd {
	__le32 sta_id;
} __packed;  

 
struct iwl_mvm_sta_disable_tx_cmd {
	__le32 sta_id;
	__le32 disable;
} __packed;  

#endif  
