 
 
#ifndef __iwl_fw_api_d3_h__
#define __iwl_fw_api_d3_h__
#include <iwl-trans.h>

 
enum iwl_d0i3_flags {
	IWL_D0I3_RESET_REQUIRE = BIT(0),
};

 
enum iwl_d3_wakeup_flags {
	IWL_WAKEUP_D3_CONFIG_FW_ERROR = BIT(0),
};  

 
struct iwl_d3_manager_config {
	__le32 min_sleep_time;
	__le32 wakeup_flags;
	__le32 wakeup_host_timer;
} __packed;  


 

 
enum iwl_proto_offloads {
	IWL_D3_PROTO_OFFLOAD_ARP = BIT(0),
	IWL_D3_PROTO_OFFLOAD_NS = BIT(1),
	IWL_D3_PROTO_IPV4_VALID = BIT(2),
	IWL_D3_PROTO_IPV6_VALID = BIT(3),
	IWL_D3_PROTO_OFFLOAD_BTM = BIT(4),
};

#define IWL_PROTO_OFFLOAD_NUM_IPV6_ADDRS_V1	2
#define IWL_PROTO_OFFLOAD_NUM_IPV6_ADDRS_V2	6
#define IWL_PROTO_OFFLOAD_NUM_IPV6_ADDRS_V3L	12
#define IWL_PROTO_OFFLOAD_NUM_IPV6_ADDRS_V3S	4
#define IWL_PROTO_OFFLOAD_NUM_IPV6_ADDRS_MAX	12

#define IWL_PROTO_OFFLOAD_NUM_NS_CONFIG_V3L	4
#define IWL_PROTO_OFFLOAD_NUM_NS_CONFIG_V3S	2

 
struct iwl_proto_offload_cmd_common {
	__le32 enabled;
	__be32 remote_ipv4_addr;
	__be32 host_ipv4_addr;
	u8 arp_mac_addr[ETH_ALEN];
	__le16 reserved;
} __packed;

 
struct iwl_proto_offload_cmd_v1 {
	struct iwl_proto_offload_cmd_common common;
	u8 remote_ipv6_addr[16];
	u8 solicited_node_ipv6_addr[16];
	u8 target_ipv6_addr[IWL_PROTO_OFFLOAD_NUM_IPV6_ADDRS_V1][16];
	u8 ndp_mac_addr[ETH_ALEN];
	__le16 reserved2;
} __packed;  

 
struct iwl_proto_offload_cmd_v2 {
	struct iwl_proto_offload_cmd_common common;
	u8 remote_ipv6_addr[16];
	u8 solicited_node_ipv6_addr[16];
	u8 target_ipv6_addr[IWL_PROTO_OFFLOAD_NUM_IPV6_ADDRS_V2][16];
	u8 ndp_mac_addr[ETH_ALEN];
	u8 num_valid_ipv6_addrs;
	u8 reserved2[3];
} __packed;  

struct iwl_ns_config {
	struct in6_addr source_ipv6_addr;
	struct in6_addr dest_ipv6_addr;
	u8 target_mac_addr[ETH_ALEN];
	__le16 reserved;
} __packed;  

struct iwl_targ_addr {
	struct in6_addr addr;
	__le32 config_num;
} __packed;  

 
struct iwl_proto_offload_cmd_v3_small {
	struct iwl_proto_offload_cmd_common common;
	__le32 num_valid_ipv6_addrs;
	struct iwl_targ_addr targ_addrs[IWL_PROTO_OFFLOAD_NUM_IPV6_ADDRS_V3S];
	struct iwl_ns_config ns_config[IWL_PROTO_OFFLOAD_NUM_NS_CONFIG_V3S];
} __packed;  

 
struct iwl_proto_offload_cmd_v3_large {
	struct iwl_proto_offload_cmd_common common;
	__le32 num_valid_ipv6_addrs;
	struct iwl_targ_addr targ_addrs[IWL_PROTO_OFFLOAD_NUM_IPV6_ADDRS_V3L];
	struct iwl_ns_config ns_config[IWL_PROTO_OFFLOAD_NUM_NS_CONFIG_V3L];
} __packed;  

 
struct iwl_proto_offload_cmd_v4 {
	__le32 sta_id;
	struct iwl_proto_offload_cmd_common common;
	__le32 num_valid_ipv6_addrs;
	struct iwl_targ_addr targ_addrs[IWL_PROTO_OFFLOAD_NUM_IPV6_ADDRS_V3L];
	struct iwl_ns_config ns_config[IWL_PROTO_OFFLOAD_NUM_NS_CONFIG_V3L];
} __packed;  

 
#define IWL_WOWLAN_MIN_PATTERN_LEN	16
#define IWL_WOWLAN_MAX_PATTERN_LEN	128

struct iwl_wowlan_pattern_v1 {
	u8 mask[IWL_WOWLAN_MAX_PATTERN_LEN / 8];
	u8 pattern[IWL_WOWLAN_MAX_PATTERN_LEN];
	u8 mask_size;
	u8 pattern_size;
	__le16 reserved;
} __packed;  

#define IWL_WOWLAN_MAX_PATTERNS	20

 
struct iwl_wowlan_patterns_cmd_v1 {
	 
	__le32 n_patterns;

	 
	struct iwl_wowlan_pattern_v1 patterns[];
} __packed;  

#define IPV4_ADDR_SIZE	4
#define IPV6_ADDR_SIZE	16

enum iwl_wowlan_pattern_type {
	WOWLAN_PATTERN_TYPE_BITMASK,
	WOWLAN_PATTERN_TYPE_IPV4_TCP_SYN,
	WOWLAN_PATTERN_TYPE_IPV6_TCP_SYN,
	WOWLAN_PATTERN_TYPE_IPV4_TCP_SYN_WILDCARD,
	WOWLAN_PATTERN_TYPE_IPV6_TCP_SYN_WILDCARD,
};  

 
struct iwl_wowlan_ipv4_tcp_syn {
	 
	u8 src_addr[IPV4_ADDR_SIZE];

	 
	u8 dst_addr[IPV4_ADDR_SIZE];

	 
	__le16 src_port;

	 
	__le16 dst_port;
} __packed;  

 
struct iwl_wowlan_ipv6_tcp_syn {
	 
	u8 src_addr[IPV6_ADDR_SIZE];

	 
	u8 dst_addr[IPV6_ADDR_SIZE];

	 
	__le16 src_port;

	 
	__le16 dst_port;
} __packed;  

 
union iwl_wowlan_pattern_data {
	 
	struct iwl_wowlan_pattern_v1 bitmask;

	 
	struct iwl_wowlan_ipv4_tcp_syn ipv4_tcp_syn;

	 
	struct iwl_wowlan_ipv6_tcp_syn ipv6_tcp_syn;
};  

 
struct iwl_wowlan_pattern_v2 {
	 
	u8 pattern_type;

	 
	u8 reserved[3];

	 
	union iwl_wowlan_pattern_data u;
} __packed;  

 
struct iwl_wowlan_patterns_cmd {
	 
	u8 n_patterns;

	 
	u8 sta_id;

	 
	__le16 reserved;

	 
	struct iwl_wowlan_pattern_v2 patterns[];
} __packed;  

enum iwl_wowlan_wakeup_filters {
	IWL_WOWLAN_WAKEUP_MAGIC_PACKET			= BIT(0),
	IWL_WOWLAN_WAKEUP_PATTERN_MATCH			= BIT(1),
	IWL_WOWLAN_WAKEUP_BEACON_MISS			= BIT(2),
	IWL_WOWLAN_WAKEUP_LINK_CHANGE			= BIT(3),
	IWL_WOWLAN_WAKEUP_GTK_REKEY_FAIL		= BIT(4),
	IWL_WOWLAN_WAKEUP_EAP_IDENT_REQ			= BIT(5),
	IWL_WOWLAN_WAKEUP_4WAY_HANDSHAKE		= BIT(6),
	IWL_WOWLAN_WAKEUP_ENABLE_NET_DETECT		= BIT(7),
	IWL_WOWLAN_WAKEUP_RF_KILL_DEASSERT		= BIT(8),
	IWL_WOWLAN_WAKEUP_REMOTE_LINK_LOSS		= BIT(9),
	IWL_WOWLAN_WAKEUP_REMOTE_SIGNATURE_TABLE	= BIT(10),
	IWL_WOWLAN_WAKEUP_REMOTE_TCP_EXTERNAL		= BIT(11),
	IWL_WOWLAN_WAKEUP_REMOTE_WAKEUP_PACKET		= BIT(12),
	IWL_WOWLAN_WAKEUP_IOAC_MAGIC_PACKET		= BIT(13),
	IWL_WOWLAN_WAKEUP_HOST_TIMER			= BIT(14),
	IWL_WOWLAN_WAKEUP_RX_FRAME			= BIT(15),
	IWL_WOWLAN_WAKEUP_BCN_FILTERING			= BIT(16),
};  

enum iwl_wowlan_flags {
	IS_11W_ASSOC		= BIT(0),
	ENABLE_L3_FILTERING	= BIT(1),
	ENABLE_NBNS_FILTERING	= BIT(2),
	ENABLE_DHCP_FILTERING	= BIT(3),
	ENABLE_STORE_BEACON	= BIT(4),
};

 
struct iwl_wowlan_config_cmd {
	__le32 wakeup_filter;
	__le16 non_qos_seq;
	__le16 qos_seq[8];
	u8 wowlan_ba_teardown_tids;
	u8 is_11n_connection;
	u8 offloading_tid;
	u8 flags;
	u8 sta_id;
	u8 reserved;
} __packed;  

#define IWL_NUM_RSC	16
#define WOWLAN_KEY_MAX_SIZE	32
#define WOWLAN_GTK_KEYS_NUM     2
#define WOWLAN_IGTK_KEYS_NUM	2
#define WOWLAN_IGTK_MIN_INDEX	4

 
struct tkip_sc {
	__le16 iv16;
	__le16 pad;
	__le32 iv32;
} __packed;  

struct iwl_tkip_rsc_tsc {
	struct tkip_sc unicast_rsc[IWL_NUM_RSC];
	struct tkip_sc multicast_rsc[IWL_NUM_RSC];
	struct tkip_sc tsc;
} __packed;  

struct aes_sc {
	__le64 pn;
} __packed;  

struct iwl_aes_rsc_tsc {
	struct aes_sc unicast_rsc[IWL_NUM_RSC];
	struct aes_sc multicast_rsc[IWL_NUM_RSC];
	struct aes_sc tsc;
} __packed;  

union iwl_all_tsc_rsc {
	struct iwl_tkip_rsc_tsc tkip;
	struct iwl_aes_rsc_tsc aes;
};  

struct iwl_wowlan_rsc_tsc_params_cmd_ver_2 {
	union iwl_all_tsc_rsc all_tsc_rsc;
} __packed;  

struct iwl_wowlan_rsc_tsc_params_cmd_v4 {
	struct iwl_wowlan_rsc_tsc_params_cmd_ver_2 params;
	__le32 sta_id;
} __packed;  

struct iwl_wowlan_rsc_tsc_params_cmd {
	__le64 ucast_rsc[IWL_MAX_TID_COUNT];
	__le64 mcast_rsc[WOWLAN_GTK_KEYS_NUM][IWL_MAX_TID_COUNT];
	__le32 sta_id;
#define IWL_MCAST_KEY_MAP_INVALID	0xff
	u8 mcast_key_id_map[4];
} __packed;  

#define IWL_MIC_KEY_SIZE	8
struct iwl_mic_keys {
	u8 tx[IWL_MIC_KEY_SIZE];
	u8 rx_unicast[IWL_MIC_KEY_SIZE];
	u8 rx_mcast[IWL_MIC_KEY_SIZE];
} __packed;  

#define IWL_P1K_SIZE		5
struct iwl_p1k_cache {
	__le16 p1k[IWL_P1K_SIZE];
} __packed;

#define IWL_NUM_RX_P1K_CACHE	2

struct iwl_wowlan_tkip_params_cmd_ver_1 {
	struct iwl_mic_keys mic_keys;
	struct iwl_p1k_cache tx;
	struct iwl_p1k_cache rx_uni[IWL_NUM_RX_P1K_CACHE];
	struct iwl_p1k_cache rx_multi[IWL_NUM_RX_P1K_CACHE];
} __packed;  

struct iwl_wowlan_tkip_params_cmd {
	struct iwl_mic_keys mic_keys;
	struct iwl_p1k_cache tx;
	struct iwl_p1k_cache rx_uni[IWL_NUM_RX_P1K_CACHE];
	struct iwl_p1k_cache rx_multi[IWL_NUM_RX_P1K_CACHE];
	u8     reversed[2];
	__le32 sta_id;
} __packed;  

#define IWL_KCK_MAX_SIZE	32
#define IWL_KEK_MAX_SIZE	32

struct iwl_wowlan_kek_kck_material_cmd_v2 {
	u8	kck[IWL_KCK_MAX_SIZE];
	u8	kek[IWL_KEK_MAX_SIZE];
	__le16	kck_len;
	__le16	kek_len;
	__le64	replay_ctr;
} __packed;  

struct iwl_wowlan_kek_kck_material_cmd_v3 {
	u8	kck[IWL_KCK_MAX_SIZE];
	u8	kek[IWL_KEK_MAX_SIZE];
	__le16	kck_len;
	__le16	kek_len;
	__le64	replay_ctr;
	__le32  akm;
	__le32  gtk_cipher;
	__le32  igtk_cipher;
	__le32  bigtk_cipher;
} __packed;  

struct iwl_wowlan_kek_kck_material_cmd_v4 {
	__le32  sta_id;
	u8	kck[IWL_KCK_MAX_SIZE];
	u8	kek[IWL_KEK_MAX_SIZE];
	__le16	kck_len;
	__le16	kek_len;
	__le64	replay_ctr;
	__le32  akm;
	__le32  gtk_cipher;
	__le32  igtk_cipher;
	__le32  bigtk_cipher;
} __packed;  

struct iwl_wowlan_get_status_cmd {
	__le32  sta_id;
} __packed;  

#define RF_KILL_INDICATOR_FOR_WOWLAN	0x87

enum iwl_wowlan_rekey_status {
	IWL_WOWLAN_REKEY_POST_REKEY = 0,
	IWL_WOWLAN_REKEY_WHILE_REKEY = 1,
};  

enum iwl_wowlan_wakeup_reason {
	IWL_WOWLAN_WAKEUP_BY_NON_WIRELESS			= 0,
	IWL_WOWLAN_WAKEUP_BY_MAGIC_PACKET			= BIT(0),
	IWL_WOWLAN_WAKEUP_BY_PATTERN				= BIT(1),
	IWL_WOWLAN_WAKEUP_BY_DISCONNECTION_ON_MISSED_BEACON	= BIT(2),
	IWL_WOWLAN_WAKEUP_BY_DISCONNECTION_ON_DEAUTH		= BIT(3),
	IWL_WOWLAN_WAKEUP_BY_GTK_REKEY_FAILURE			= BIT(4),
	IWL_WOWLAN_WAKEUP_BY_RFKILL_DEASSERTED			= BIT(5),
	IWL_WOWLAN_WAKEUP_BY_UCODE_ERROR			= BIT(6),
	IWL_WOWLAN_WAKEUP_BY_EAPOL_REQUEST			= BIT(7),
	IWL_WOWLAN_WAKEUP_BY_FOUR_WAY_HANDSHAKE			= BIT(8),
	IWL_WOWLAN_WAKEUP_BY_REM_WAKE_LINK_LOSS			= BIT(9),
	IWL_WOWLAN_WAKEUP_BY_REM_WAKE_SIGNATURE_TABLE		= BIT(10),
	IWL_WOWLAN_WAKEUP_BY_REM_WAKE_TCP_EXTERNAL		= BIT(11),
	IWL_WOWLAN_WAKEUP_BY_REM_WAKE_WAKEUP_PACKET		= BIT(12),
	IWL_WOWLAN_WAKEUP_BY_IOAC_MAGIC_PACKET			= BIT(13),
	IWL_WOWLAN_WAKEUP_BY_D3_WAKEUP_HOST_TIMER		= BIT(14),
	IWL_WOWLAN_WAKEUP_BY_RXFRAME_FILTERED_IN		= BIT(15),
	IWL_WOWLAN_WAKEUP_BY_BEACON_FILTERED_IN			= BIT(16),
	IWL_WAKEUP_BY_11W_UNPROTECTED_DEAUTH_OR_DISASSOC	= BIT(17),
	IWL_WAKEUP_BY_PATTERN_IPV4_TCP_SYN			= BIT(18),
	IWL_WAKEUP_BY_PATTERN_IPV4_TCP_SYN_WILDCARD		= BIT(19),
	IWL_WAKEUP_BY_PATTERN_IPV6_TCP_SYN			= BIT(20),
	IWL_WAKEUP_BY_PATTERN_IPV6_TCP_SYN_WILDCARD		= BIT(21),
};  

struct iwl_wowlan_gtk_status_v1 {
	u8 key_index;
	u8 reserved[3];
	u8 decrypt_key[16];
	u8 tkip_mic_key[8];
	struct iwl_wowlan_rsc_tsc_params_cmd_ver_2 rsc;
} __packed;  

 
struct iwl_wowlan_gtk_status_v2 {
	u8 key[WOWLAN_KEY_MAX_SIZE];
	u8 key_len;
	u8 key_flags;
	u8 reserved[2];
	u8 tkip_mic_key[8];
	struct iwl_wowlan_rsc_tsc_params_cmd_ver_2 rsc;
} __packed;  

 
struct iwl_wowlan_all_rsc_tsc_v5 {
	__le64 ucast_rsc[IWL_MAX_TID_COUNT];
	__le64 mcast_rsc[2][IWL_MAX_TID_COUNT];
	__le32 sta_id;
	u8 mcast_key_id_map[4];
} __packed;  

 
struct iwl_wowlan_gtk_status_v3 {
	u8 key[WOWLAN_KEY_MAX_SIZE];
	u8 key_len;
	u8 key_flags;
	u8 reserved[2];
	u8 tkip_mic_key[IWL_MIC_KEY_SIZE];
	struct iwl_wowlan_all_rsc_tsc_v5 sc;
} __packed;  

#define IWL_WOWLAN_GTK_IDX_MASK		(BIT(0) | BIT(1))
#define IWL_WOWLAN_IGTK_BIGTK_IDX_MASK	(BIT(0))

 
struct iwl_wowlan_igtk_status {
	u8 key[WOWLAN_KEY_MAX_SIZE];
	u8 ipn[6];
	u8 key_len;
	u8 key_flags;
} __packed;  

 
struct iwl_wowlan_status_v6 {
	struct iwl_wowlan_gtk_status_v1 gtk;
	__le64 replay_ctr;
	__le16 pattern_number;
	__le16 non_qos_seq_ctr;
	__le16 qos_seq_ctr[8];
	__le32 wakeup_reasons;
	__le32 num_of_gtk_rekeys;
	__le32 transmitted_ndps;
	__le32 received_beacons;
	__le32 wake_packet_length;
	__le32 wake_packet_bufsize;
	u8 wake_packet[];  
} __packed;  

 
struct iwl_wowlan_status_v7 {
	struct iwl_wowlan_gtk_status_v2 gtk[WOWLAN_GTK_KEYS_NUM];
	struct iwl_wowlan_igtk_status igtk[WOWLAN_IGTK_KEYS_NUM];
	__le64 replay_ctr;
	__le16 pattern_number;
	__le16 non_qos_seq_ctr;
	__le16 qos_seq_ctr[8];
	__le32 wakeup_reasons;
	__le32 num_of_gtk_rekeys;
	__le32 transmitted_ndps;
	__le32 received_beacons;
	__le32 wake_packet_length;
	__le32 wake_packet_bufsize;
	u8 wake_packet[];  
} __packed;  

 
struct iwl_wowlan_status_v9 {
	struct iwl_wowlan_gtk_status_v2 gtk[WOWLAN_GTK_KEYS_NUM];
	struct iwl_wowlan_igtk_status igtk[WOWLAN_IGTK_KEYS_NUM];
	__le64 replay_ctr;
	__le16 pattern_number;
	__le16 non_qos_seq_ctr;
	__le16 qos_seq_ctr[8];
	__le32 wakeup_reasons;
	__le32 num_of_gtk_rekeys;
	__le32 transmitted_ndps;
	__le32 received_beacons;
	__le32 wake_packet_length;
	__le32 wake_packet_bufsize;
	u8 tid_tear_down;
	u8 reserved[3];
	u8 wake_packet[];  
} __packed;  

 
struct iwl_wowlan_status_v12 {
	struct iwl_wowlan_gtk_status_v3 gtk[WOWLAN_GTK_KEYS_NUM];
	struct iwl_wowlan_igtk_status igtk[WOWLAN_IGTK_KEYS_NUM];
	__le64 replay_ctr;
	__le16 pattern_number;
	__le16 non_qos_seq_ctr;
	__le16 qos_seq_ctr[8];
	__le32 wakeup_reasons;
	__le32 num_of_gtk_rekeys;
	__le32 transmitted_ndps;
	__le32 received_beacons;
	__le32 wake_packet_length;
	__le32 wake_packet_bufsize;
	u8 tid_tear_down;
	u8 reserved[3];
	u8 wake_packet[];  
} __packed;  

 
struct iwl_wowlan_info_notif_v1 {
	struct iwl_wowlan_gtk_status_v3 gtk[WOWLAN_GTK_KEYS_NUM];
	struct iwl_wowlan_igtk_status igtk[WOWLAN_IGTK_KEYS_NUM];
	__le64 replay_ctr;
	__le16 pattern_number;
	__le16 reserved1;
	__le16 qos_seq_ctr[8];
	__le32 wakeup_reasons;
	__le32 num_of_gtk_rekeys;
	__le32 transmitted_ndps;
	__le32 received_beacons;
	__le32 wake_packet_length;
	__le32 wake_packet_bufsize;
	u8 tid_tear_down;
	u8 station_id;
	u8 reserved2[2];
} __packed;  

 
struct iwl_wowlan_info_notif {
	struct iwl_wowlan_gtk_status_v3 gtk[WOWLAN_GTK_KEYS_NUM];
	struct iwl_wowlan_igtk_status igtk[WOWLAN_IGTK_KEYS_NUM];
	__le64 replay_ctr;
	__le16 pattern_number;
	__le16 reserved1;
	__le16 qos_seq_ctr[8];
	__le32 wakeup_reasons;
	__le32 num_of_gtk_rekeys;
	__le32 transmitted_ndps;
	__le32 received_beacons;
	u8 tid_tear_down;
	u8 station_id;
	u8 reserved2[2];
} __packed;  

 
struct iwl_wowlan_wake_pkt_notif {
	__le32 wake_packet_length;
	u8 station_id;
	u8 reserved[3];
	u8 wake_packet[1];
} __packed;  

 
struct iwl_mvm_d3_end_notif {
	__le32 flags;
} __packed;

 

#endif  
