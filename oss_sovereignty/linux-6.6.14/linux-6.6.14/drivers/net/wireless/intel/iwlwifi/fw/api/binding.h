#ifndef __iwl_fw_api_binding_h__
#define __iwl_fw_api_binding_h__
#include <fw/file.h>
#include <fw/img.h>
#define MAX_MACS_IN_BINDING	(3)
#define MAX_BINDINGS		(4)
struct iwl_binding_cmd_v1 {
	__le32 id_and_color;
	__le32 action;
	__le32 macs[MAX_MACS_IN_BINDING];
	__le32 phy;
} __packed;  
struct iwl_binding_cmd {
	__le32 id_and_color;
	__le32 action;
	__le32 macs[MAX_MACS_IN_BINDING];
	__le32 phy;
	__le32 lmac_id;
} __packed;  
#define IWL_BINDING_CMD_SIZE_V1	sizeof(struct iwl_binding_cmd_v1)
#define IWL_LMAC_24G_INDEX		0
#define IWL_LMAC_5G_INDEX		1
#define IWL_MVM_MAX_QUOTA 128
struct iwl_time_quota_data_v1 {
	__le32 id_and_color;
	__le32 quota;
	__le32 max_duration;
} __packed;  
struct iwl_time_quota_cmd_v1 {
	struct iwl_time_quota_data_v1 quotas[MAX_BINDINGS];
} __packed;  
enum iwl_quota_low_latency {
	IWL_QUOTA_LOW_LATENCY_NONE = 0,
	IWL_QUOTA_LOW_LATENCY_TX = BIT(0),
	IWL_QUOTA_LOW_LATENCY_RX = BIT(1),
	IWL_QUOTA_LOW_LATENCY_TX_RX =
		IWL_QUOTA_LOW_LATENCY_TX | IWL_QUOTA_LOW_LATENCY_RX,
};
struct iwl_time_quota_data {
	__le32 id_and_color;
	__le32 quota;
	__le32 max_duration;
	__le32 low_latency;
} __packed;  
struct iwl_time_quota_cmd {
	struct iwl_time_quota_data quotas[MAX_BINDINGS];
} __packed;  
#endif  
