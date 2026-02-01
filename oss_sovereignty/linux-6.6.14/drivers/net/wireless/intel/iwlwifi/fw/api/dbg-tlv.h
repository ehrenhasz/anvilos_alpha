 
 
#ifndef __iwl_fw_dbg_tlv_h__
#define __iwl_fw_dbg_tlv_h__

#include <linux/bitops.h>

#define IWL_FW_INI_MAX_REGION_ID		64
#define IWL_FW_INI_MAX_NAME			32
#define IWL_FW_INI_MAX_CFG_NAME			64
#define IWL_FW_INI_DOMAIN_ALWAYS_ON		0
#define IWL_FW_INI_REGION_ID_MASK		GENMASK(15, 0)
#define IWL_FW_INI_REGION_DUMP_POLICY_MASK	GENMASK(31, 16)
#define IWL_FW_INI_PRESET_DISABLE		0xff

 
struct iwl_fw_ini_hcmd {
	u8 id;
	u8 group;
	__le16 reserved;
	u8 data[];
} __packed;  

 
struct iwl_fw_ini_header {
	__le32 version;
	__le32 domain;
	 
} __packed;  

 
struct iwl_fw_ini_region_dev_addr {
	__le32 size;
	__le32 offset;
} __packed;  

 
struct iwl_fw_ini_region_fifos {
	__le32 fid[2];
	__le32 hdr_only;
	__le32 offset;
} __packed;  

 
struct iwl_fw_ini_region_err_table {
	__le32 version;
	__le32 base_addr;
	__le32 size;
	__le32 offset;
} __packed;  

 
struct iwl_fw_ini_region_special_device_memory {
	__le16 type;
	__le16 version;
	__le32 base_addr;
	__le32 size;
	__le32 offset;
} __packed;  

 
struct iwl_fw_ini_region_internal_buffer {
	__le32 alloc_id;
	__le32 base_addr;
	__le32 size;
} __packed;  

 
struct iwl_fw_ini_region_tlv {
	struct iwl_fw_ini_header hdr;
	__le32 id;
	u8 type;
	u8 sub_type;
	u8 sub_type_ver;
	u8 reserved;
	u8 name[IWL_FW_INI_MAX_NAME];
	union {
		struct iwl_fw_ini_region_dev_addr dev_addr;
		struct iwl_fw_ini_region_fifos fifos;
		struct iwl_fw_ini_region_err_table err_table;
		struct iwl_fw_ini_region_internal_buffer internal_buffer;
		struct iwl_fw_ini_region_special_device_memory special_mem;
		__le32 dram_alloc_id;
		__le32 tlv_mask;
	};  
	__le32 addrs[];
} __packed;  

 
struct iwl_fw_ini_debug_info_tlv {
	struct iwl_fw_ini_header hdr;
	__le32 image_type;
	u8 debug_cfg_name[IWL_FW_INI_MAX_CFG_NAME];
} __packed;  

 
struct iwl_fw_ini_allocation_tlv {
	struct iwl_fw_ini_header hdr;
	__le32 alloc_id;
	__le32 buf_location;
	__le32 req_size;
	__le32 max_frags_num;
	__le32 min_size;
} __packed;  

 
struct iwl_fw_ini_trigger_tlv {
	struct iwl_fw_ini_header hdr;
	__le32 time_point;
	__le32 trigger_reason;
	__le32 apply_policy;
	__le32 dump_delay;
	__le32 occurrences;
	__le32 reserved;
	__le32 ignore_consec;
	__le32 reset_fw;
	__le32 multi_dut;
	__le64 regions_mask;
	__le32 data[];
} __packed;  

 
struct iwl_fw_ini_hcmd_tlv {
	struct iwl_fw_ini_header hdr;
	__le32 time_point;
	__le32 period_msec;
	struct iwl_fw_ini_hcmd hcmd;
} __packed;  

 
struct iwl_fw_ini_addr_val {
	__le32 address;
	__le32 value;
} __packed;  

 
struct iwl_fw_ini_conf_set_tlv {
	struct iwl_fw_ini_header hdr;
	__le32 time_point;
	__le32 set_type;
	__le32 addr_offset;
	struct iwl_fw_ini_addr_val addr_val[];
} __packed;  

 

enum iwl_fw_ini_config_set_type {
	IWL_FW_INI_CONFIG_SET_TYPE_INVALID = 0,
	IWL_FW_INI_CONFIG_SET_TYPE_DEVICE_PERIPHERY_MAC,
	IWL_FW_INI_CONFIG_SET_TYPE_DEVICE_PERIPHERY_PHY,
	IWL_FW_INI_CONFIG_SET_TYPE_DEVICE_PERIPHERY_AUX,
	IWL_FW_INI_CONFIG_SET_TYPE_DEVICE_MEMORY,
	IWL_FW_INI_CONFIG_SET_TYPE_CSR,
	IWL_FW_INI_CONFIG_SET_TYPE_DBGC_DRAM_ADDR,
	IWL_FW_INI_CONFIG_SET_TYPE_PERIPH_SCRATCH_HWM,
	IWL_FW_INI_CONFIG_SET_TYPE_MAX_NUM,
} __packed;

 
enum iwl_fw_ini_allocation_id {
	IWL_FW_INI_ALLOCATION_INVALID,
	IWL_FW_INI_ALLOCATION_ID_DBGC1,
	IWL_FW_INI_ALLOCATION_ID_DBGC2,
	IWL_FW_INI_ALLOCATION_ID_DBGC3,
	IWL_FW_INI_ALLOCATION_ID_DBGC4,
	IWL_FW_INI_ALLOCATION_NUM,
};  

 
enum iwl_fw_ini_buffer_location {
	IWL_FW_INI_LOCATION_INVALID,
	IWL_FW_INI_LOCATION_SRAM_PATH,
	IWL_FW_INI_LOCATION_DRAM_PATH,
	IWL_FW_INI_LOCATION_NPK_PATH,
	IWL_FW_INI_LOCATION_NUM,
};  

 
enum iwl_fw_ini_region_type {
	IWL_FW_INI_REGION_INVALID,
	IWL_FW_INI_REGION_TLV,
	IWL_FW_INI_REGION_INTERNAL_BUFFER,
	IWL_FW_INI_REGION_DRAM_BUFFER,
	IWL_FW_INI_REGION_TXF,
	IWL_FW_INI_REGION_RXF,
	IWL_FW_INI_REGION_LMAC_ERROR_TABLE,
	IWL_FW_INI_REGION_UMAC_ERROR_TABLE,
	IWL_FW_INI_REGION_RSP_OR_NOTIF,
	IWL_FW_INI_REGION_DEVICE_MEMORY,
	IWL_FW_INI_REGION_PERIPHERY_MAC,
	IWL_FW_INI_REGION_PERIPHERY_PHY,
	IWL_FW_INI_REGION_PERIPHERY_AUX,
	IWL_FW_INI_REGION_PAGING,
	IWL_FW_INI_REGION_CSR,
	IWL_FW_INI_REGION_DRAM_IMR,
	IWL_FW_INI_REGION_PCI_IOSF_CONFIG,
	IWL_FW_INI_REGION_SPECIAL_DEVICE_MEMORY,
	IWL_FW_INI_REGION_DBGI_SRAM,
	IWL_FW_INI_REGION_NUM
};  

enum iwl_fw_ini_region_device_memory_subtype {
	IWL_FW_INI_REGION_DEVICE_MEMORY_SUBTYPE_HW_SMEM = 1,
	IWL_FW_INI_REGION_DEVICE_MEMORY_SUBTYPE_UMAC_ERROR_TABLE = 5,
	IWL_FW_INI_REGION_DEVICE_MEMORY_SUBTYPE_LMAC_1_ERROR_TABLE = 7,
	IWL_FW_INI_REGION_DEVICE_MEMORY_SUBTYPE_LMAC_2_ERROR_TABLE = 10,
	IWL_FW_INI_REGION_DEVICE_MEMORY_SUBTYPE_TCM_1_ERROR_TABLE = 14,
	IWL_FW_INI_REGION_DEVICE_MEMORY_SUBTYPE_TCM_2_ERROR_TABLE = 16,
	IWL_FW_INI_REGION_DEVICE_MEMORY_SUBTYPE_RCM_1_ERROR_TABLE = 18,
	IWL_FW_INI_REGION_DEVICE_MEMORY_SUBTYPE_RCM_2_ERROR_TABLE = 20,
};  

 
enum iwl_fw_ini_time_point {
	IWL_FW_INI_TIME_POINT_INVALID,
	IWL_FW_INI_TIME_POINT_EARLY,
	IWL_FW_INI_TIME_POINT_AFTER_ALIVE,
	IWL_FW_INI_TIME_POINT_POST_INIT,
	IWL_FW_INI_TIME_POINT_FW_ASSERT,
	IWL_FW_INI_TIME_POINT_FW_HW_ERROR,
	IWL_FW_INI_TIME_POINT_FW_TFD_Q_HANG,
	IWL_FW_INI_TIME_POINT_FW_DHC_NOTIFICATION,
	IWL_FW_INI_TIME_POINT_FW_RSP_OR_NOTIF,
	IWL_FW_INI_TIME_POINT_USER_TRIGGER,
	IWL_FW_INI_TIME_POINT_PERIODIC,
	IWL_FW_INI_TIME_POINT_RESERVED,
	IWL_FW_INI_TIME_POINT_HOST_ASSERT,
	IWL_FW_INI_TIME_POINT_HOST_ALIVE_TIMEOUT,
	IWL_FW_INI_TIME_POINT_HOST_DEVICE_ENABLE,
	IWL_FW_INI_TIME_POINT_HOST_DEVICE_DISABLE,
	IWL_FW_INI_TIME_POINT_HOST_D3_START,
	IWL_FW_INI_TIME_POINT_HOST_D3_END,
	IWL_FW_INI_TIME_POINT_MISSED_BEACONS,
	IWL_FW_INI_TIME_POINT_ASSOC_FAILED,
	IWL_FW_INI_TIME_POINT_TX_FAILED,
	IWL_FW_INI_TIME_POINT_TX_WFD_ACTION_FRAME_FAILED,
	IWL_FW_INI_TIME_POINT_TX_LATENCY_THRESHOLD,
	IWL_FW_INI_TIME_POINT_HANG_OCCURRED,
	IWL_FW_INI_TIME_POINT_EAPOL_FAILED,
	IWL_FW_INI_TIME_POINT_FAKE_TX,
	IWL_FW_INI_TIME_POINT_DEASSOC,
	IWL_FW_INI_TIME_POINT_NUM,
};  

 
enum iwl_fw_ini_trigger_apply_policy {
	IWL_FW_INI_APPLY_POLICY_MATCH_TIME_POINT	= BIT(0),
	IWL_FW_INI_APPLY_POLICY_MATCH_DATA		= BIT(1),
	IWL_FW_INI_APPLY_POLICY_OVERRIDE_REGIONS	= BIT(8),
	IWL_FW_INI_APPLY_POLICY_OVERRIDE_CFG		= BIT(9),
	IWL_FW_INI_APPLY_POLICY_OVERRIDE_DATA		= BIT(10),
	IWL_FW_INI_APPLY_POLICY_DUMP_COMPLETE_CMD	= BIT(16),
};

 
enum iwl_fw_ini_trigger_reset_fw_policy {
	IWL_FW_INI_RESET_FW_MODE_NOTHING = 0,
	IWL_FW_INI_RESET_FW_MODE_STOP_FW_ONLY,
	IWL_FW_INI_RESET_FW_MODE_STOP_AND_RELOAD_FW
};

 
enum iwl_fw_ini_dump_policy {
	IWL_FW_INI_DEBUG_DUMP_POLICY_NO_LIMIT           = BIT(0),
	IWL_FW_INI_DEBUG_DUMP_POLICY_MAX_LIMIT_600KB    = BIT(1),
	IWL_FW_IWL_DEBUG_DUMP_POLICY_MAX_LIMIT_5MB      = BIT(2),

};

 
enum iwl_fw_ini_dump_type {
	IWL_FW_INI_DUMP_BRIEF,
	IWL_FW_INI_DUMP_MEDIUM,
	IWL_FW_INI_DUMP_VERBOSE,
};
#endif
