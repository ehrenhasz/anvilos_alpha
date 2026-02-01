 
 
#ifndef __iwl_fw_api_alive_h__
#define __iwl_fw_api_alive_h__

 
#define ALIVE_RESP_UCODE_OK	BIT(0)
#define ALIVE_RESP_RFKILL	BIT(1)

 
enum {
	FW_TYPE_HW = 0,
	FW_TYPE_PROT = 1,
	FW_TYPE_AP = 2,
	FW_TYPE_WOWLAN = 3,
	FW_TYPE_TIMING = 4,
	FW_TYPE_WIPAN = 5
};

 
enum {
	FW_SUBTYPE_FULL_FEATURE = 0,
	FW_SUBTYPE_BOOTSRAP = 1,  
	FW_SUBTYPE_REDUCED = 2,
	FW_SUBTYPE_ALIVE_ONLY = 3,
	FW_SUBTYPE_WOWLAN = 4,
	FW_SUBTYPE_AP_SUBTYPE = 5,
	FW_SUBTYPE_WIPAN = 6,
	FW_SUBTYPE_INITIALIZE = 9
};

#define IWL_ALIVE_STATUS_ERR 0xDEAD
#define IWL_ALIVE_STATUS_OK 0xCAFE

#define IWL_ALIVE_FLG_RFKILL	BIT(0)

struct iwl_lmac_debug_addrs {
	__le32 error_event_table_ptr;	 
	__le32 log_event_table_ptr;	 
	__le32 cpu_register_ptr;
	__le32 dbgm_config_ptr;
	__le32 alive_counter_ptr;
	__le32 scd_base_ptr;		 
	__le32 st_fwrd_addr;		 
	__le32 st_fwrd_size;
} __packed;  

struct iwl_lmac_alive {
	__le32 ucode_major;
	__le32 ucode_minor;
	u8 ver_subtype;
	u8 ver_type;
	u8 mac;
	u8 opt;
	__le32 timestamp;
	struct iwl_lmac_debug_addrs dbg_ptrs;
} __packed;  

struct iwl_umac_debug_addrs {
	__le32 error_info_addr;		 
	__le32 dbg_print_buff_addr;
} __packed;  

struct iwl_umac_alive {
	__le32 umac_major;		 
	__le32 umac_minor;		 
	struct iwl_umac_debug_addrs dbg_ptrs;
} __packed;  

struct iwl_sku_id {
	__le32 data[3];
} __packed;  

struct iwl_alive_ntf_v3 {
	__le16 status;
	__le16 flags;
	struct iwl_lmac_alive lmac_data;
	struct iwl_umac_alive umac_data;
} __packed;  

struct iwl_alive_ntf_v4 {
	__le16 status;
	__le16 flags;
	struct iwl_lmac_alive lmac_data[2];
	struct iwl_umac_alive umac_data;
} __packed;  

struct iwl_alive_ntf_v5 {
	__le16 status;
	__le16 flags;
	struct iwl_lmac_alive lmac_data[2];
	struct iwl_umac_alive umac_data;
	struct iwl_sku_id sku_id;
} __packed;  

struct iwl_imr_alive_info {
	__le64 base_addr;
	__le32 size;
	__le32 enabled;
} __packed;  

struct iwl_alive_ntf_v6 {
	__le16 status;
	__le16 flags;
	struct iwl_lmac_alive lmac_data[2];
	struct iwl_umac_alive umac_data;
	struct iwl_sku_id sku_id;
	struct iwl_imr_alive_info imr;
} __packed;  

 
enum iwl_extended_cfg_flags {
	IWL_INIT_DEBUG_CFG,
	IWL_INIT_NVM,
	IWL_INIT_PHY,
};

 
struct iwl_init_extended_cfg_cmd {
	__le32 init_flags;
} __packed;  

 
struct iwl_radio_version_notif {
	__le32 radio_flavor;
	__le32 radio_step;
	__le32 radio_dash;
} __packed;  

enum iwl_card_state_flags {
	CARD_ENABLED		= 0x00,
	HW_CARD_DISABLED	= 0x01,
	SW_CARD_DISABLED	= 0x02,
	CT_KILL_CARD_DISABLED	= 0x04,
	HALT_CARD_DISABLED	= 0x08,
	CARD_DISABLED_MSK	= 0x0f,
	CARD_IS_RX_ON		= 0x10,
};

 
enum iwl_error_recovery_flags {
	ERROR_RECOVERY_UPDATE_DB = BIT(0),
	ERROR_RECOVERY_END_OF_RECOVERY = BIT(1),
};

 
struct iwl_fw_error_recovery_cmd {
	__le32 flags;
	__le32 buf_size;
} __packed;  

#endif  
