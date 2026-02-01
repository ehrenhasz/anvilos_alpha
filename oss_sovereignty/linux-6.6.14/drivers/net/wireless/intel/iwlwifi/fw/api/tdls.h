 
 
#ifndef __iwl_fw_api_tdls_h__
#define __iwl_fw_api_tdls_h__

#include "fw/api/tx.h"
#include "fw/api/phy-ctxt.h"

#define IWL_MVM_TDLS_STA_COUNT	4

 
enum iwl_tdls_channel_switch_type {
	TDLS_SEND_CHAN_SW_REQ = 0,
	TDLS_SEND_CHAN_SW_RESP_AND_MOVE_CH,
	TDLS_MOVE_CH,
};  

 
struct iwl_tdls_channel_switch_timing {
	__le32 frame_timestamp;  
	__le32 max_offchan_duration;  
	__le32 switch_time;  
	__le32 switch_timeout;  
} __packed;  

#define IWL_TDLS_CH_SW_FRAME_MAX_SIZE 200

 
struct iwl_tdls_channel_switch_frame {
	__le32 switch_time_offset;
	struct iwl_tx_cmd tx_cmd;
	u8 data[IWL_TDLS_CH_SW_FRAME_MAX_SIZE];
} __packed;  

 
struct iwl_tdls_channel_switch_cmd_tail {
	struct iwl_tdls_channel_switch_timing timing;
	struct iwl_tdls_channel_switch_frame frame;
} __packed;

 
struct iwl_tdls_channel_switch_cmd {
	u8 switch_type;
	__le32 peer_sta_id;
	struct iwl_fw_channel_info ci;
	struct iwl_tdls_channel_switch_cmd_tail tail;
} __packed;  

 
struct iwl_tdls_channel_switch_notif {
	__le32 status;
	__le32 offchannel_duration;
	__le32 sta_id;
} __packed;  

 
struct iwl_tdls_sta_info {
	u8 sta_id;
	u8 tx_to_peer_tid;
	__le16 tx_to_peer_ssn;
	__le32 is_initiator;
} __packed;  

 
struct iwl_tdls_config_cmd {
	__le32 id_and_color;  
	u8 tdls_peer_count;
	u8 tx_to_ap_tid;
	__le16 tx_to_ap_ssn;
	struct iwl_tdls_sta_info sta_info[IWL_MVM_TDLS_STA_COUNT];

	__le32 pti_req_data_offset;
	struct iwl_tx_cmd pti_req_tx_cmd;
	u8 pti_req_template[];
} __packed;  

 
struct iwl_tdls_config_sta_info_res {
	__le16 sta_id;
	__le16 tx_to_peer_last_seq;
} __packed;  

 
struct iwl_tdls_config_res {
	__le32 tx_to_ap_last_seq;
	struct iwl_tdls_config_sta_info_res sta_info[IWL_MVM_TDLS_STA_COUNT];
} __packed;  

#endif  
