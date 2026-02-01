 
 
#ifndef __iwl_fw_api_rfi_h__
#define __iwl_fw_api_rfi_h__

#define IWL_RFI_LUT_ENTRY_CHANNELS_NUM 15
#define IWL_RFI_LUT_SIZE 24
#define IWL_RFI_LUT_INSTALLED_SIZE 4

 
struct iwl_rfi_lut_entry {
	__le16 freq;
	u8 channels[IWL_RFI_LUT_ENTRY_CHANNELS_NUM];
	u8 bands[IWL_RFI_LUT_ENTRY_CHANNELS_NUM];
} __packed;

 
struct iwl_rfi_config_cmd {
	struct iwl_rfi_lut_entry table[IWL_RFI_LUT_SIZE];
	u8 oem;
	u8 reserved[3];
} __packed;  

 
enum iwl_rfi_freq_table_status {
	RFI_FREQ_TABLE_OK,
	RFI_FREQ_TABLE_DVFS_NOT_READY,
	RFI_FREQ_TABLE_DISABLED,
};

 
struct iwl_rfi_freq_table_resp_cmd {
	struct iwl_rfi_lut_entry table[IWL_RFI_LUT_INSTALLED_SIZE];
	__le32 status;
} __packed;  

 
struct iwl_rfi_deactivate_notif {
	__le32 reason;
} __packed;  
#endif  
