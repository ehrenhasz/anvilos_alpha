 
 
#ifndef __iwl_fw_api_nvm_reg_h__
#define __iwl_fw_api_nvm_reg_h__

 
enum iwl_regulatory_and_nvm_subcmd_ids {
	 
	NVM_ACCESS_COMPLETE = 0x0,

	 
	LARI_CONFIG_CHANGE = 0x1,

	 
	NVM_GET_INFO = 0x2,

	 
	TAS_CONFIG = 0x3,

	 
	SAR_OFFSET_MAPPING_TABLE_CMD = 0x4,

	 
	PNVM_INIT_COMPLETE_NTFY = 0xFE,
};

 
enum iwl_nvm_access_op {
	IWL_NVM_READ	= 0,
	IWL_NVM_WRITE	= 1,
};

 
enum iwl_nvm_access_target {
	NVM_ACCESS_TARGET_CACHE = 0,
	NVM_ACCESS_TARGET_OTP = 1,
	NVM_ACCESS_TARGET_EEPROM = 2,
};

 
enum iwl_nvm_section_type {
	NVM_SECTION_TYPE_SW = 1,
	NVM_SECTION_TYPE_REGULATORY = 3,
	NVM_SECTION_TYPE_CALIBRATION = 4,
	NVM_SECTION_TYPE_PRODUCTION = 5,
	NVM_SECTION_TYPE_REGULATORY_SDP = 8,
	NVM_SECTION_TYPE_MAC_OVERRIDE = 11,
	NVM_SECTION_TYPE_PHY_SKU = 12,
	NVM_MAX_NUM_SECTIONS = 13,
};

 
struct iwl_nvm_access_cmd {
	u8 op_code;
	u8 target;
	__le16 type;
	__le16 offset;
	__le16 length;
	u8 data[];
} __packed;  

 
struct iwl_nvm_access_resp {
	__le16 offset;
	__le16 length;
	__le16 type;
	__le16 status;
	u8 data[];
} __packed;  

 
struct iwl_nvm_get_info {
	__le32 reserved;
} __packed;  

 
enum iwl_nvm_info_general_flags {
	NVM_GENERAL_FLAGS_EMPTY_OTP	= BIT(0),
};

 
struct iwl_nvm_get_info_general {
	__le32 flags;
	__le16 nvm_version;
	u8 board_type;
	u8 n_hw_addrs;
} __packed;  

 
enum iwl_nvm_mac_sku_flags {
	NVM_MAC_SKU_FLAGS_BAND_2_4_ENABLED	= BIT(0),
	NVM_MAC_SKU_FLAGS_BAND_5_2_ENABLED	= BIT(1),
	NVM_MAC_SKU_FLAGS_802_11N_ENABLED	= BIT(2),
	NVM_MAC_SKU_FLAGS_802_11AC_ENABLED	= BIT(3),
	 
	NVM_MAC_SKU_FLAGS_802_11AX_ENABLED	= BIT(4),
	NVM_MAC_SKU_FLAGS_MIMO_DISABLED		= BIT(5),
	NVM_MAC_SKU_FLAGS_WAPI_ENABLED		= BIT(8),
	NVM_MAC_SKU_FLAGS_REG_CHECK_ENABLED	= BIT(14),
	NVM_MAC_SKU_FLAGS_API_LOCK_ENABLED	= BIT(15),
};

 
struct iwl_nvm_get_info_sku {
	__le32 mac_sku_flags;
} __packed;  

 
struct iwl_nvm_get_info_phy {
	__le32 tx_chains;
	__le32 rx_chains;
} __packed;  

#define IWL_NUM_CHANNELS_V1	51
#define IWL_NUM_CHANNELS	110

 
struct iwl_nvm_get_info_regulatory_v1 {
	__le32 lar_enabled;
	__le16 channel_profile[IWL_NUM_CHANNELS_V1];
	__le16 reserved;
} __packed;  

 
struct iwl_nvm_get_info_regulatory {
	__le32 lar_enabled;
	__le32 n_channels;
	__le32 channel_profile[IWL_NUM_CHANNELS];
} __packed;  

 
struct iwl_nvm_get_info_rsp_v3 {
	struct iwl_nvm_get_info_general general;
	struct iwl_nvm_get_info_sku mac_sku;
	struct iwl_nvm_get_info_phy phy_sku;
	struct iwl_nvm_get_info_regulatory_v1 regulatory;
} __packed;  

 
struct iwl_nvm_get_info_rsp {
	struct iwl_nvm_get_info_general general;
	struct iwl_nvm_get_info_sku mac_sku;
	struct iwl_nvm_get_info_phy phy_sku;
	struct iwl_nvm_get_info_regulatory regulatory;
} __packed;  

 
struct iwl_nvm_access_complete_cmd {
	__le32 reserved;
} __packed;  

 
struct iwl_mcc_update_cmd {
	__le16 mcc;
	u8 source_id;
	u8 reserved;
	__le32 key;
	u8 reserved2[20];
} __packed;  

 
enum iwl_geo_information {
	GEO_NO_INFO =			0,
	GEO_WMM_ETSI_5GHZ_INFO =	BIT(0),
};

 
struct iwl_mcc_update_resp_v3 {
	__le32 status;
	__le16 mcc;
	u8 cap;
	u8 source_id;
	__le16 time;
	__le16 geo_info;
	__le32 n_channels;
	__le32 channels[];
} __packed;  

 
struct iwl_mcc_update_resp_v4 {
	__le32 status;
	__le16 mcc;
	__le16 cap;
	__le16 time;
	__le16 geo_info;
	u8 source_id;
	u8 reserved[3];
	__le32 n_channels;
	__le32 channels[];
} __packed;  

 
struct iwl_mcc_update_resp_v8 {
	__le32 status;
	__le16 mcc;
	u8 padding[2];
	__le32 cap;
	__le16 time;
	__le16 geo_info;
	u8 source_id;
	u8 reserved[3];
	__le32 n_channels;
	__le32 channels[];
} __packed;  

 
struct iwl_mcc_chub_notif {
	__le16 mcc;
	u8 source_id;
	u8 reserved1;
} __packed;  

enum iwl_mcc_update_status {
	MCC_RESP_NEW_CHAN_PROFILE,
	MCC_RESP_SAME_CHAN_PROFILE,
	MCC_RESP_INVALID,
	MCC_RESP_NVM_DISABLED,
	MCC_RESP_ILLEGAL,
	MCC_RESP_LOW_PRIORITY,
	MCC_RESP_TEST_MODE_ACTIVE,
	MCC_RESP_TEST_MODE_NOT_ACTIVE,
	MCC_RESP_TEST_MODE_DENIAL_OF_SERVICE,
};

enum iwl_mcc_source {
	MCC_SOURCE_OLD_FW = 0,
	MCC_SOURCE_ME = 1,
	MCC_SOURCE_BIOS = 2,
	MCC_SOURCE_3G_LTE_HOST = 3,
	MCC_SOURCE_3G_LTE_DEVICE = 4,
	MCC_SOURCE_WIFI = 5,
	MCC_SOURCE_RESERVED = 6,
	MCC_SOURCE_DEFAULT = 7,
	MCC_SOURCE_UNINITIALIZED = 8,
	MCC_SOURCE_MCC_API = 9,
	MCC_SOURCE_GET_CURRENT = 0x10,
	MCC_SOURCE_GETTING_MCC_TEST_MODE = 0x11,
};

#define IWL_TAS_BLOCK_LIST_MAX 16
 
struct iwl_tas_config_cmd_v2 {
	__le32 block_list_size;
	__le32 block_list_array[IWL_TAS_BLOCK_LIST_MAX];
} __packed;  

 
struct iwl_tas_config_cmd_v3 {
	__le32 block_list_size;
	__le32 block_list_array[IWL_TAS_BLOCK_LIST_MAX];
	__le16 override_tas_iec;
	__le16 enable_tas_iec;
} __packed;  

 
struct iwl_tas_config_cmd_v4 {
	__le32 block_list_size;
	__le32 block_list_array[IWL_TAS_BLOCK_LIST_MAX];
	u8 override_tas_iec;
	u8 enable_tas_iec;
	u8 usa_tas_uhb_allowed;
	u8 reserved;
} __packed;  

union iwl_tas_config_cmd {
	struct iwl_tas_config_cmd_v2 v2;
	struct iwl_tas_config_cmd_v3 v3;
	struct iwl_tas_config_cmd_v4 v4;
};
 
enum iwl_lari_config_masks {
	LARI_CONFIG_DISABLE_11AC_UKRAINE_MSK		= BIT(0),
	LARI_CONFIG_CHANGE_ETSI_TO_PASSIVE_MSK		= BIT(1),
	LARI_CONFIG_CHANGE_ETSI_TO_DISABLED_MSK		= BIT(2),
	LARI_CONFIG_ENABLE_5G2_IN_INDONESIA_MSK		= BIT(3),
};

#define IWL_11AX_UKRAINE_MASK 3
#define IWL_11AX_UKRAINE_SHIFT 8

 
struct iwl_lari_config_change_cmd_v1 {
	__le32 config_bitmap;
} __packed;  

 
struct iwl_lari_config_change_cmd_v2 {
	__le32 config_bitmap;
	__le32 oem_uhb_allow_bitmap;
} __packed;  

 
struct iwl_lari_config_change_cmd_v3 {
	__le32 config_bitmap;
	__le32 oem_uhb_allow_bitmap;
	__le32 oem_11ax_allow_bitmap;
} __packed;  

 
struct iwl_lari_config_change_cmd_v4 {
	__le32 config_bitmap;
	__le32 oem_uhb_allow_bitmap;
	__le32 oem_11ax_allow_bitmap;
	__le32 oem_unii4_allow_bitmap;
} __packed;  

 
struct iwl_lari_config_change_cmd_v5 {
	__le32 config_bitmap;
	__le32 oem_uhb_allow_bitmap;
	__le32 oem_11ax_allow_bitmap;
	__le32 oem_unii4_allow_bitmap;
	__le32 chan_state_active_bitmap;
} __packed;  

 
struct iwl_lari_config_change_cmd_v6 {
	__le32 config_bitmap;
	__le32 oem_uhb_allow_bitmap;
	__le32 oem_11ax_allow_bitmap;
	__le32 oem_unii4_allow_bitmap;
	__le32 chan_state_active_bitmap;
	__le32 force_disable_channels_bitmap;
} __packed;  

 
struct iwl_pnvm_init_complete_ntfy {
	__le32 status;
} __packed;  

#endif  
