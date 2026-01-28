#ifndef __iwl_fw_api_debug_h__
#define __iwl_fw_api_debug_h__
enum iwl_debug_cmds {
	LMAC_RD_WR = 0x0,
	UMAC_RD_WR = 0x1,
	HOST_EVENT_CFG = 0x3,
	INVALID_WR_PTR_CMD = 0x6,
	DBGC_SUSPEND_RESUME = 0x7,
	BUFFER_ALLOCATION = 0x8,
	GET_TAS_STATUS = 0xA,
	FW_DUMP_COMPLETE_CMD = 0xB,
	MFU_ASSERT_DUMP_NTF = 0xFE,
};
enum {
	FW_ERR_UNKNOWN_CMD = 0x0,
	FW_ERR_INVALID_CMD_PARAM = 0x1,
	FW_ERR_SERVICE = 0x2,
	FW_ERR_ARC_MEMORY = 0x3,
	FW_ERR_ARC_CODE = 0x4,
	FW_ERR_WATCH_DOG = 0x5,
	FW_ERR_WEP_GRP_KEY_INDX = 0x10,
	FW_ERR_WEP_KEY_SIZE = 0x11,
	FW_ERR_OBSOLETE_FUNC = 0x12,
	FW_ERR_UNEXPECTED = 0xFE,
	FW_ERR_FATAL = 0xFF
};
enum iwl_dbg_suspend_resume_cmds {
	DBGC_RESUME_CMD,
	DBGC_SUSPEND_CMD,
};
struct iwl_error_resp {
	__le32 error_type;
	u8 cmd_id;
	u8 reserved1;
	__le16 bad_cmd_seq_num;
	__le32 error_service;
	__le64 timestamp;
} __packed;
#define TX_FIFO_MAX_NUM_9000		8
#define TX_FIFO_MAX_NUM			15
#define RX_FIFO_MAX_NUM			2
#define TX_FIFO_INTERNAL_MAX_NUM	6
struct iwl_shared_mem_cfg_v2 {
	__le32 shared_mem_addr;
	__le32 shared_mem_size;
	__le32 sample_buff_addr;
	__le32 sample_buff_size;
	__le32 txfifo_addr;
	__le32 txfifo_size[TX_FIFO_MAX_NUM_9000];
	__le32 rxfifo_size[RX_FIFO_MAX_NUM];
	__le32 page_buff_addr;
	__le32 page_buff_size;
	__le32 rxfifo_addr;
	__le32 internal_txfifo_addr;
	__le32 internal_txfifo_size[TX_FIFO_INTERNAL_MAX_NUM];
} __packed;  
struct iwl_shared_mem_lmac_cfg {
	__le32 txfifo_addr;
	__le32 txfifo_size[TX_FIFO_MAX_NUM];
	__le32 rxfifo1_addr;
	__le32 rxfifo1_size;
} __packed;  
struct iwl_shared_mem_cfg {
	__le32 shared_mem_addr;
	__le32 shared_mem_size;
	__le32 sample_buff_addr;
	__le32 sample_buff_size;
	__le32 rxfifo2_addr;
	__le32 rxfifo2_size;
	__le32 page_buff_addr;
	__le32 page_buff_size;
	__le32 lmac_num;
	struct iwl_shared_mem_lmac_cfg lmac_smem[3];
	__le32 rxfifo2_control_addr;
	__le32 rxfifo2_control_size;
} __packed;  
struct iwl_mfuart_load_notif_v1 {
	__le32 installed_ver;
	__le32 external_ver;
	__le32 status;
	__le32 duration;
} __packed;  
struct iwl_mfuart_load_notif {
	__le32 installed_ver;
	__le32 external_ver;
	__le32 status;
	__le32 duration;
	__le32 image_size;
} __packed;  
struct iwl_mfu_assert_dump_notif {
	__le32   assert_id;
	__le32   curr_reset_num;
	__le16   index_num;
	__le16   parts_num;
	__le32   data_size;
	__le32   data[];
} __packed;  
enum iwl_mvm_marker_id {
	MARKER_ID_TX_FRAME_LATENCY = 1,
	MARKER_ID_SYNC_CLOCK = 2,
};  
struct iwl_mvm_marker {
	u8 dw_len;
	u8 marker_id;
	__le16 reserved;
	__le64 timestamp;
	__le32 metadata[];
} __packed;  
struct iwl_mvm_marker_rsp {
	__le32 gp2;
} __packed;
enum {
	DEBUG_MEM_OP_READ = 0,
	DEBUG_MEM_OP_WRITE = 1,
	DEBUG_MEM_OP_WRITE_BYTES = 2,
};
#define DEBUG_MEM_MAX_SIZE_DWORDS 32
struct iwl_dbg_mem_access_cmd {
	__le32 op;
	__le32 addr;
	__le32 len;
	__le32 data[];
} __packed;  
enum {
	DEBUG_MEM_STATUS_SUCCESS = 0x0,
	DEBUG_MEM_STATUS_FAILED = 0x1,
	DEBUG_MEM_STATUS_LOCKED = 0x2,
	DEBUG_MEM_STATUS_HIDDEN = 0x3,
	DEBUG_MEM_STATUS_LENGTH = 0x4,
};
struct iwl_dbg_mem_access_rsp {
	__le32 status;
	__le32 len;
	__le32 data[];
} __packed;  
struct iwl_dbg_suspend_resume_cmd {
	__le32 operation;
} __packed;
#define BUF_ALLOC_MAX_NUM_FRAGS 16
struct iwl_buf_alloc_frag {
	__le64 addr;
	__le32 size;
} __packed;  
struct iwl_buf_alloc_cmd {
	__le32 alloc_id;
	__le32 buf_location;
	__le32 num_frags;
	struct iwl_buf_alloc_frag frags[BUF_ALLOC_MAX_NUM_FRAGS];
} __packed;  
#define DRAM_INFO_FIRST_MAGIC_WORD 0x76543210
#define DRAM_INFO_SECOND_MAGIC_WORD 0x89ABCDEF
struct iwl_dram_info {
	__le32 first_word;
	__le32 second_word;
	struct iwl_buf_alloc_cmd dram_frags[IWL_FW_INI_ALLOCATION_NUM - 1];
} __packed;  
struct iwl_dbgc1_info {
	__le32 first_word;
	__le32 dbgc1_add_lsb;
	__le32 dbgc1_add_msb;
	__le32 dbgc1_size;
} __packed;  
struct iwl_dbg_host_event_cfg_cmd {
	__le32 enabled_severities;
} __packed;  
struct iwl_dbg_dump_complete_cmd {
	__le32 tp;
	__le32 tp_data;
} __packed;  
#define TAS_LMAC_BAND_HB       0
#define TAS_LMAC_BAND_LB       1
#define TAS_LMAC_BAND_UHB      2
#define TAS_LMAC_BAND_INVALID  3
struct iwl_mvm_tas_status_per_mac {
	u8 static_status;
	u8 static_dis_reason;
	u8 dynamic_status;
	u8 near_disconnection;
	__le16 max_reg_pwr_limit;
	__le16 sar_limit;
	u8 band;
	u8 reserved[3];
} __packed;  
struct iwl_mvm_tas_status_resp {
	u8 tas_fw_version;
	u8 is_uhb_for_usa_enable;
	__le16 curr_mcc;
	__le16 block_list[16];
	struct iwl_mvm_tas_status_per_mac tas_status_mac[2];
	u8 in_dual_radio;
	u8 reserved[3];
} __packed;  
enum iwl_mvm_tas_dyna_status {
	TAS_DYNA_INACTIVE,
	TAS_DYNA_INACTIVE_MVM_MODE,
	TAS_DYNA_INACTIVE_TRIGGER_MODE,
	TAS_DYNA_INACTIVE_BLOCK_LISTED,
	TAS_DYNA_INACTIVE_UHB_NON_US,
	TAS_DYNA_ACTIVE,
	TAS_DYNA_STATUS_MAX,
};  
enum iwl_mvm_tas_statically_disabled_reason {
	TAS_DISABLED_DUE_TO_BIOS,
	TAS_DISABLED_DUE_TO_SAR_6DBM,
	TAS_DISABLED_REASON_INVALID,
	TAS_DISABLED_REASON_MAX,
};  
#endif  
