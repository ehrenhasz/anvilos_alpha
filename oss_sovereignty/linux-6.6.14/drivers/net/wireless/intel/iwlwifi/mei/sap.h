 
 

#ifndef __sap_h__
#define __sap_h__

#include "mei/iwl-mei.h"

 

 

 

 

 

 

 
enum iwl_sap_me_msg_id {
	SAP_ME_MSG_START	= 1,
	SAP_ME_MSG_START_OK,
	SAP_ME_MSG_CHECK_SHARED_AREA,
};

 
struct iwl_sap_me_msg_hdr {
	__le32 type;
	__le32 seq_num;
	__le32 len;
} __packed;

 
struct iwl_sap_me_msg_start {
	struct iwl_sap_me_msg_hdr hdr;
	__le64 shared_mem;
	__le16 init_data_seq_num;
	__le16 init_notif_seq_num;
	u8 supported_versions[64];
} __packed;

 
struct iwl_sap_me_msg_start_ok {
	struct iwl_sap_me_msg_hdr hdr;
	__le16 init_data_seq_num;
	__le16 init_notif_seq_num;
	u8 supported_version;
	u8 reserved[3];
} __packed;

 
enum iwl_sap_msg {
	SAP_MSG_NOTIF_BOTH_WAYS_MIN			= 0,
	SAP_MSG_NOTIF_PING				= 1,
	SAP_MSG_NOTIF_PONG				= 2,
	SAP_MSG_NOTIF_BOTH_WAYS_MAX,

	SAP_MSG_NOTIF_FROM_CSME_MIN			= 500,
	SAP_MSG_NOTIF_CSME_FILTERS			= SAP_MSG_NOTIF_FROM_CSME_MIN,
	 
	SAP_MSG_NOTIF_AMT_STATE				= 502,
	SAP_MSG_NOTIF_CSME_REPLY_TO_HOST_OWNERSHIP_REQ	= 503,
	SAP_MSG_NOTIF_CSME_TAKING_OWNERSHIP		= 504,
	SAP_MSG_NOTIF_TRIGGER_IP_REFRESH		= 505,
	SAP_MSG_NOTIF_CSME_CAN_RELEASE_OWNERSHIP	= 506,
	 
	 
	 
	 
	SAP_MSG_NOTIF_NIC_OWNER				= 511,
	SAP_MSG_NOTIF_CSME_CONN_STATUS			= 512,
	SAP_MSG_NOTIF_NVM				= 513,
	 
	SAP_MSG_NOTIF_PLDR_ACK				= 518,
	SAP_MSG_NOTIF_FROM_CSME_MAX,

	SAP_MSG_NOTIF_FROM_HOST_MIN			= 1000,
	SAP_MSG_NOTIF_BAND_SELECTION			= SAP_MSG_NOTIF_FROM_HOST_MIN,
	SAP_MSG_NOTIF_RADIO_STATE			= 1001,
	SAP_MSG_NOTIF_NIC_INFO				= 1002,
	SAP_MSG_NOTIF_HOST_ASKS_FOR_NIC_OWNERSHIP	= 1003,
	SAP_MSG_NOTIF_HOST_SUSPENDS			= 1004,
	SAP_MSG_NOTIF_HOST_RESUMES			= 1005,
	SAP_MSG_NOTIF_HOST_GOES_DOWN			= 1006,
	SAP_MSG_NOTIF_CSME_OWNERSHIP_CONFIRMED		= 1007,
	SAP_MSG_NOTIF_COUNTRY_CODE			= 1008,
	SAP_MSG_NOTIF_HOST_LINK_UP			= 1009,
	SAP_MSG_NOTIF_HOST_LINK_DOWN			= 1010,
	SAP_MSG_NOTIF_WHO_OWNS_NIC			= 1011,
	SAP_MSG_NOTIF_WIFIDR_DOWN			= 1012,
	SAP_MSG_NOTIF_WIFIDR_UP				= 1013,
	 
	SAP_MSG_NOTIF_HOST_OWNERSHIP_CONFIRMED		= 1015,
	SAP_MSG_NOTIF_SAR_LIMITS			= 1016,
	SAP_MSG_NOTIF_GET_NVM				= 1017,
	 
	SAP_MSG_NOTIF_PLDR				= 1024,
	SAP_MSG_NOTIF_PLDR_END				= 1025,
	SAP_MSG_NOTIF_FROM_HOST_MAX,

	SAP_MSG_DATA_MIN				= 2000,
	SAP_MSG_DATA_PACKET				= SAP_MSG_DATA_MIN,
	SAP_MSG_CB_DATA_PACKET				= 2001,
	SAP_MSG_DATA_MAX,
};

 
struct iwl_sap_hdr {
	__le16 type;
	__le16 len;
	__le32 seq_num;
	u8 payload[];
};

 
struct iwl_sap_msg_dw {
	struct iwl_sap_hdr hdr;
	__le32 val;
};

 
enum iwl_sap_nic_owner {
	SAP_NIC_OWNER_UNKNOWN,
	SAP_NIC_OWNER_HOST,
	SAP_NIC_OWNER_ME,
};

enum iwl_sap_wifi_auth_type {
	SAP_WIFI_AUTH_TYPE_OPEN		= IWL_MEI_AKM_AUTH_OPEN,
	SAP_WIFI_AUTH_TYPE_RSNA		= IWL_MEI_AKM_AUTH_RSNA,
	SAP_WIFI_AUTH_TYPE_RSNA_PSK	= IWL_MEI_AKM_AUTH_RSNA_PSK,
	SAP_WIFI_AUTH_TYPE_SAE		= IWL_MEI_AKM_AUTH_SAE,
	SAP_WIFI_AUTH_TYPE_MAX,
};

 
enum iwl_sap_wifi_cipher_alg {
	SAP_WIFI_CIPHER_ALG_NONE	= IWL_MEI_CIPHER_NONE,
	SAP_WIFI_CIPHER_ALG_TKIP	= IWL_MEI_CIPHER_TKIP,
	SAP_WIFI_CIPHER_ALG_CCMP	= IWL_MEI_CIPHER_CCMP,
	SAP_WIFI_CIPHER_ALG_GCMP	= IWL_MEI_CIPHER_GCMP,
	SAP_WIFI_CIPHER_ALG_GCMP_256	= IWL_MEI_CIPHER_GCMP_256,
};

 
struct iwl_sap_notif_connection_info {
	__le32 ssid_len;
	u8 ssid[32];
	__le32 auth_mode;
	__le32 pairwise_cipher;
	u8 channel;
	u8 band;
	__le16 reserved;
	u8 bssid[6];
	__le16 reserved1;
} __packed;

 
enum iwl_sap_scan_request {
	SCAN_REQUEST_FILTERING	= 1 << 0,
	SCAN_REQUEST_FAST	= 1 << 1,
};

 
struct iwl_sap_notif_conn_status {
	struct iwl_sap_hdr hdr;
	__le32 link_prot_state;
	__le32 scan_request;
	struct iwl_sap_notif_connection_info conn_info;
} __packed;

 
enum iwl_sap_radio_state_bitmap {
	SAP_SW_RFKILL_DEASSERTED	= 1 << 0,
	SAP_HW_RFKILL_DEASSERTED	= 1 << 1,
};

 
enum iwl_sap_notif_host_suspends_bitmap {
	SAP_OFFER_NIC		= 1 << 0,
	SAP_FILTER_CONFIGURED	= 1 << 1,
	SAP_NLO_CONFIGURED	= 1 << 2,
	SAP_HOST_OWNS_NIC	= 1 << 3,
	SAP_LINK_PROTECTED	= 1 << 4,
};

 
struct iwl_sap_notif_country_code {
	struct iwl_sap_hdr hdr;
	__le16 mcc;
	u8 source_id;
	u8 reserved;
	__le32 diff_time;
} __packed;

 
struct iwl_sap_notif_host_link_up {
	struct iwl_sap_hdr hdr;
	struct iwl_sap_notif_connection_info conn_info;
	u8 colloc_channel;
	u8 colloc_band;
	__le16 reserved;
	u8 colloc_bssid[6];
	__le16 reserved1;
} __packed;

 
enum iwl_sap_notif_link_down_type {
	HOST_LINK_DOWN_TYPE_NONE,
	HOST_LINK_DOWN_TYPE_TEMPORARY,
	HOST_LINK_DOWN_TYPE_LONG,
};

 
struct iwl_sap_notif_host_link_down {
	struct iwl_sap_hdr hdr;
	u8 type;
	u8 reserved[2];
	u8 reason_valid;
	__le32 reason;
} __packed;

 
struct iwl_sap_notif_host_nic_info {
	struct iwl_sap_hdr hdr;
	u8 mac_address[6];
	u8 nvm_address[6];
} __packed;

 
struct iwl_sap_notif_dw {
	struct iwl_sap_hdr hdr;
	__le32 dw;
} __packed;

 
struct iwl_sap_notif_sar_limits {
	struct iwl_sap_hdr hdr;
	__le16 sar_chain_info_table[2][5];
} __packed;

 
enum iwl_sap_nvm_caps {
	SAP_NVM_CAPS_LARI_SUPPORT	= BIT(0),
	SAP_NVM_CAPS_11AX_SUPPORT	= BIT(1),
};

 
struct iwl_sap_nvm {
	struct iwl_sap_hdr hdr;
	u8 hw_addr[6];
	u8 n_hw_addrs;
	u8 reserved;
	__le32 radio_cfg;
	__le32 caps;
	__le32 nvm_version;
	__le32 channels[110];
} __packed;

 
enum iwl_sap_eth_filter_flags {
	SAP_ETH_FILTER_STOP    = BIT(0),
	SAP_ETH_FILTER_COPY    = BIT(1),
	SAP_ETH_FILTER_ENABLED = BIT(2),
};

 
struct iwl_sap_eth_filter {
	u8 mac_address[6];
	u8 flags;
} __packed;

 
enum iwl_sap_flex_filter_flags {
	SAP_FLEX_FILTER_COPY		= BIT(0),
	SAP_FLEX_FILTER_ENABLED		= BIT(1),
	SAP_FLEX_FILTER_IPV6		= BIT(2),
	SAP_FLEX_FILTER_IPV4		= BIT(3),
	SAP_FLEX_FILTER_TCP		= BIT(4),
	SAP_FLEX_FILTER_UDP		= BIT(5),
};

 
struct iwl_sap_flex_filter {
	__be16 src_port;
	__be16 dst_port;
	u8 flags;
	u8 reserved;
} __packed;

 
enum iwl_sap_ipv4_filter_flags {
	SAP_IPV4_FILTER_ICMP_PASS	= BIT(0),
	SAP_IPV4_FILTER_ICMP_COPY	= BIT(1),
	SAP_IPV4_FILTER_ARP_REQ_PASS	= BIT(2),
	SAP_IPV4_FILTER_ARP_REQ_COPY	= BIT(3),
	SAP_IPV4_FILTER_ARP_RESP_PASS	= BIT(4),
	SAP_IPV4_FILTER_ARP_RESP_COPY	= BIT(5),
};

 
struct iwl_sap_ipv4_filter {
	__be32 ipv4_addr;
	__le32 flags;
} __packed;

 
enum iwl_sap_ipv6_filter_flags {
	SAP_IPV6_ADDR_FILTER_COPY	= BIT(0),
	SAP_IPV6_ADDR_FILTER_ENABLED	= BIT(1),
};

 
struct iwl_sap_ipv6_filter {
	u8 addr_lo24[3];
	u8 flags;
} __packed;

 
enum iwl_sap_icmpv6_filter_flags {
	SAP_ICMPV6_FILTER_ENABLED	= BIT(0),
	SAP_ICMPV6_FILTER_COPY		= BIT(1),
};

 
enum iwl_sap_vlan_filter_flags {
	SAP_VLAN_FILTER_VLAN_ID_MSK	= 0x0FFF,
	SAP_VLAN_FILTER_ENABLED		= BIT(15),
};

 
struct iwl_sap_oob_filters {
	struct iwl_sap_flex_filter flex_filters[14];
	__le32 icmpv6_flags;
	struct iwl_sap_ipv6_filter ipv6_filters[4];
	struct iwl_sap_eth_filter eth_filters[5];
	u8 reserved;
	struct iwl_sap_ipv4_filter ipv4_filter;
	__le16 vlan[4];
} __packed;

 
struct iwl_sap_csme_filters {
	struct iwl_sap_hdr hdr;
	__le32 mode;
	u8 mac_address[6];
	__le16 reserved;
	u8 cbfilters[1728];
	struct iwl_sap_oob_filters filters;
} __packed;

#define CB_TX_DHCP_FILT_IDX 30
 
struct iwl_sap_cb_data {
	struct iwl_sap_hdr hdr;
	__le32 reserved[7];
	__le32 to_me_filt_status;
	__le32 reserved2;
	__le32 data_len;
	u8 payload[];
};

 
struct iwl_sap_pldr_data {
	struct iwl_sap_hdr hdr;
	__le32 version;
} __packed;

 
enum iwl_sap_pldr_status {
	SAP_PLDR_STATUS_SUCCESS	= 0,
	SAP_PLDR_STATUS_FAILURE	= 1,
};

 
struct iwl_sap_pldr_end_data {
	struct iwl_sap_hdr hdr;
	__le32 version;
	__le32 status;
} __packed;

 
struct iwl_sap_pldr_ack_data {
	struct iwl_sap_hdr hdr;
	__le32 version;
	__le32 status;
} __packed;

#endif  
