#ifndef __iwl_fw_api_time_event_h__
#define __iwl_fw_api_time_event_h__
#include "fw/api/phy-ctxt.h"
enum iwl_time_event_type {
	TE_BSS_STA_AGGRESSIVE_ASSOC,
	TE_BSS_STA_ASSOC,
	TE_BSS_EAP_DHCP_PROT,
	TE_BSS_QUIET_PERIOD,
	TE_P2P_DEVICE_DISCOVERABLE,
	TE_P2P_DEVICE_LISTEN,
	TE_P2P_DEVICE_ACTION_SCAN,
	TE_P2P_DEVICE_FULL_SCAN,
	TE_P2P_CLIENT_AGGRESSIVE_ASSOC,
	TE_P2P_CLIENT_ASSOC,
	TE_P2P_CLIENT_QUIET_PERIOD,
	TE_P2P_GO_ASSOC_PROT,
	TE_P2P_GO_REPETITIVET_NOA,
	TE_P2P_GO_CT_WINDOW,
	TE_WIDI_TX_SYNC,
	TE_CHANNEL_SWITCH_PERIOD,
	TE_MAX
};  
enum {
	TE_V1_FRAG_NONE = 0,
	TE_V1_FRAG_SINGLE = 1,
	TE_V1_FRAG_DUAL = 2,
	TE_V1_FRAG_ENDLESS = 0xffffffff
};
#define TE_V1_FRAG_MAX_MSK	0x0fffffff
#define TE_V1_REPEAT_ENDLESS	0xffffffff
#define TE_V1_REPEAT_MAX_MSK_V1	0x0fffffff
enum {
	TE_V1_INDEPENDENT		= 0,
	TE_V1_DEP_OTHER			= BIT(0),
	TE_V1_DEP_TSF			= BIT(1),
	TE_V1_EVENT_SOCIOPATHIC		= BIT(2),
};  
enum {
	TE_V1_NOTIF_NONE = 0,
	TE_V1_NOTIF_HOST_EVENT_START = BIT(0),
	TE_V1_NOTIF_HOST_EVENT_END = BIT(1),
	TE_V1_NOTIF_INTERNAL_EVENT_START = BIT(2),
	TE_V1_NOTIF_INTERNAL_EVENT_END = BIT(3),
	TE_V1_NOTIF_HOST_FRAG_START = BIT(4),
	TE_V1_NOTIF_HOST_FRAG_END = BIT(5),
	TE_V1_NOTIF_INTERNAL_FRAG_START = BIT(6),
	TE_V1_NOTIF_INTERNAL_FRAG_END = BIT(7),
};  
enum {
	TE_V2_FRAG_NONE = 0,
	TE_V2_FRAG_SINGLE = 1,
	TE_V2_FRAG_DUAL = 2,
	TE_V2_FRAG_MAX = 0xfe,
	TE_V2_FRAG_ENDLESS = 0xff
};
#define TE_V2_REPEAT_ENDLESS	0xff
#define TE_V2_REPEAT_MAX	0xfe
#define TE_V2_PLACEMENT_POS	12
#define TE_V2_ABSENCE_POS	15
enum iwl_time_event_policy {
	TE_V2_DEFAULT_POLICY = 0x0,
	TE_V2_NOTIF_HOST_EVENT_START = BIT(0),
	TE_V2_NOTIF_HOST_EVENT_END = BIT(1),
	TE_V2_NOTIF_INTERNAL_EVENT_START = BIT(2),
	TE_V2_NOTIF_INTERNAL_EVENT_END = BIT(3),
	TE_V2_NOTIF_HOST_FRAG_START = BIT(4),
	TE_V2_NOTIF_HOST_FRAG_END = BIT(5),
	TE_V2_NOTIF_INTERNAL_FRAG_START = BIT(6),
	TE_V2_NOTIF_INTERNAL_FRAG_END = BIT(7),
	TE_V2_START_IMMEDIATELY = BIT(11),
	TE_V2_DEP_OTHER = BIT(TE_V2_PLACEMENT_POS),
	TE_V2_DEP_TSF = BIT(TE_V2_PLACEMENT_POS + 1),
	TE_V2_EVENT_SOCIOPATHIC = BIT(TE_V2_PLACEMENT_POS + 2),
	TE_V2_ABSENCE = BIT(TE_V2_ABSENCE_POS),
};
struct iwl_time_event_cmd {
	__le32 id_and_color;
	__le32 action;
	__le32 id;
	__le32 apply_time;
	__le32 max_delay;
	__le32 depends_on;
	__le32 interval;
	__le32 duration;
	u8 repeat;
	u8 max_frags;
	__le16 policy;
} __packed;  
struct iwl_time_event_resp {
	__le32 status;
	__le32 id;
	__le32 unique_id;
	__le32 id_and_color;
} __packed;  
struct iwl_time_event_notif {
	__le32 timestamp;
	__le32 session_id;
	__le32 unique_id;
	__le32 id_and_color;
	__le32 action;
	__le32 status;
} __packed;  
struct iwl_hs20_roc_req_tail {
	u8 node_addr[ETH_ALEN];
	__le16 reserved;
	__le32 apply_time;
	__le32 apply_time_max_delay;
	__le32 duration;
} __packed;
struct iwl_hs20_roc_req {
	__le32 id_and_color;
	__le32 action;
	__le32 event_unique_id;
	__le32 sta_id_and_color;
	struct iwl_fw_channel_info channel_info;
	struct iwl_hs20_roc_req_tail tail;
} __packed;  
enum iwl_mvm_hot_spot {
	HOT_SPOT_RSP_STATUS_OK,
	HOT_SPOT_RSP_STATUS_TOO_MANY_EVENTS,
	HOT_SPOT_MAX_NUM_OF_SESSIONS,
};
struct iwl_hs20_roc_res {
	__le32 event_unique_id;
	__le32 status;
} __packed;  
enum iwl_mvm_session_prot_conf_id {
	SESSION_PROTECT_CONF_ASSOC,
	SESSION_PROTECT_CONF_GO_CLIENT_ASSOC,
	SESSION_PROTECT_CONF_P2P_DEVICE_DISCOV,
	SESSION_PROTECT_CONF_P2P_GO_NEGOTIATION,
	SESSION_PROTECT_CONF_MAX_ID,
};  
struct iwl_mvm_session_prot_cmd {
	__le32 id_and_color;
	__le32 action;
	__le32 conf_id;
	__le32 duration_tu;
	__le32 repetition_count;
	__le32 interval;
} __packed;  
struct iwl_mvm_session_prot_notif {
	__le32 mac_id;
	__le32 status;
	__le32 start;
	__le32 conf_id;
} __packed;  
#endif  
