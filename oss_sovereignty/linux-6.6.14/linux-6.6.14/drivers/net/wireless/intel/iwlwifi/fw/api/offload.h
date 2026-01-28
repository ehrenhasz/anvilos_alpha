#ifndef __iwl_fw_api_offload_h__
#define __iwl_fw_api_offload_h__
enum iwl_prot_offload_subcmd_ids {
	WOWLAN_WAKE_PKT_NOTIFICATION = 0xFC,
	WOWLAN_INFO_NOTIFICATION = 0xFD,
	D3_END_NOTIFICATION = 0xFE,
	STORED_BEACON_NTF = 0xFF,
};
#define MAX_STORED_BEACON_SIZE 600
struct iwl_stored_beacon_notif_common {
	__le32 system_time;
	__le64 tsf;
	__le32 beacon_timestamp;
	__le16 band;
	__le16 channel;
	__le32 rates;
	__le32 byte_count;
} __packed;
struct iwl_stored_beacon_notif_v2 {
	struct iwl_stored_beacon_notif_common common;
	u8 data[MAX_STORED_BEACON_SIZE];
} __packed;  
struct iwl_stored_beacon_notif_v3 {
	struct iwl_stored_beacon_notif_common common;
	u8 sta_id;
	u8 reserved[3];
	u8 data[MAX_STORED_BEACON_SIZE];
} __packed;  
#endif  
