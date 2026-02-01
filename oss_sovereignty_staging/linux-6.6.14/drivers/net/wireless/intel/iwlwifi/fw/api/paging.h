 
 
#ifndef __iwl_fw_api_paging_h__
#define __iwl_fw_api_paging_h__

#define NUM_OF_FW_PAGING_BLOCKS	33  

 
struct iwl_fw_paging_cmd {
	__le32 flags;
	__le32 block_size;
	__le32 block_num;
	__le32 device_phy_addr[NUM_OF_FW_PAGING_BLOCKS];
} __packed;  

#endif  
