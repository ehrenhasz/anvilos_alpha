 
 
#ifndef __iwl_fw_api_filter_h__
#define __iwl_fw_api_filter_h__

#include "fw/api/mac.h"

#define MAX_PORT_ID_NUM	2
#define MAX_MCAST_FILTERING_ADDRESSES 256

 
struct iwl_mcast_filter_cmd {
	u8 filter_own;
	u8 port_id;
	u8 count;
	u8 pass_all;
	u8 bssid[6];
	u8 reserved[2];
	u8 addr_list[];
} __packed;  

#endif  
