 
 
#ifndef __fw_error_dump_h__
#define __fw_error_dump_h__

#include <linux/types.h>
#include "fw/api/cmdhdr.h"

#define IWL_FW_ERROR_DUMP_BARKER	0x14789632
#define IWL_FW_INI_ERROR_DUMP_BARKER	0x14789633

 
enum iwl_fw_error_dump_type {
	 
	IWL_FW_ERROR_DUMP_CSR = 1,
	IWL_FW_ERROR_DUMP_RXF = 2,
	IWL_FW_ERROR_DUMP_TXCMD = 3,
	IWL_FW_ERROR_DUMP_DEV_FW_INFO = 4,
	IWL_FW_ERROR_DUMP_FW_MONITOR = 5,
	IWL_FW_ERROR_DUMP_PRPH = 6,
	IWL_FW_ERROR_DUMP_TXF = 7,
	IWL_FW_ERROR_DUMP_FH_REGS = 8,
	IWL_FW_ERROR_DUMP_MEM = 9,
	IWL_FW_ERROR_DUMP_ERROR_INFO = 10,
	IWL_FW_ERROR_DUMP_RB = 11,
	IWL_FW_ERROR_DUMP_PAGING = 12,
	IWL_FW_ERROR_DUMP_RADIO_REG = 13,
	IWL_FW_ERROR_DUMP_INTERNAL_TXF = 14,
	IWL_FW_ERROR_DUMP_EXTERNAL = 15,  
	IWL_FW_ERROR_DUMP_MEM_CFG = 16,
	IWL_FW_ERROR_DUMP_D3_DEBUG_DATA = 17,

	IWL_FW_ERROR_DUMP_MAX,
};

 
struct iwl_fw_error_dump_data {
	__le32 type;
	__le32 len;
	__u8 data[];
} __packed;

 
struct iwl_dump_file_name_info {
	__le32 type;
	__le32 len;
	__u8 data[];
} __packed;

 
struct iwl_fw_error_dump_file {
	__le32 barker;
	__le32 file_len;
	u8 data[];
} __packed;

 
struct iwl_fw_error_dump_txcmd {
	__le32 cmdlen;
	__le32 caplen;
	u8 data[];
} __packed;

 
struct iwl_fw_error_dump_fifo {
	__le32 fifo_num;
	__le32 available_bytes;
	__le32 wr_ptr;
	__le32 rd_ptr;
	__le32 fence_ptr;
	__le32 fence_mode;
	u8 data[];
} __packed;

enum iwl_fw_error_dump_family {
	IWL_FW_ERROR_DUMP_FAMILY_7 = 7,
	IWL_FW_ERROR_DUMP_FAMILY_8 = 8,
};

#define MAX_NUM_LMAC 2

 
struct iwl_fw_error_dump_info {
	__le32 hw_type;
	__le32 hw_step;
	u8 fw_human_readable[FW_VER_HUMAN_READABLE_SZ];
	u8 dev_human_readable[64];
	u8 bus_human_readable[8];
	u8 num_of_lmacs;
	__le32 umac_err_id;
	__le32 lmac_err_id[MAX_NUM_LMAC];
} __packed;

 
struct iwl_fw_error_dump_fw_mon {
	__le32 fw_mon_wr_ptr;
	__le32 fw_mon_base_ptr;
	__le32 fw_mon_cycle_cnt;
	__le32 fw_mon_base_high_ptr;
	__le32 reserved[2];
	u8 data[];
} __packed;

#define MAX_NUM_LMAC 2
#define TX_FIFO_INTERNAL_MAX_NUM	6
#define TX_FIFO_MAX_NUM			15
 
struct iwl_fw_error_dump_smem_cfg {
	__le32 num_lmacs;
	__le32 num_txfifo_entries;
	struct {
		__le32 txfifo_size[TX_FIFO_MAX_NUM];
		__le32 rxfifo1_size;
	} lmac[MAX_NUM_LMAC];
	__le32 rxfifo2_size;
	__le32 internal_txfifo_addr;
	__le32 internal_txfifo_size[TX_FIFO_INTERNAL_MAX_NUM];
} __packed;
 
struct iwl_fw_error_dump_prph {
	__le32 prph_start;
	__le32 data[];
};

enum iwl_fw_error_dump_mem_type {
	IWL_FW_ERROR_DUMP_MEM_SRAM,
	IWL_FW_ERROR_DUMP_MEM_SMEM,
	IWL_FW_ERROR_DUMP_MEM_NAMED_MEM = 10,
};

 
struct iwl_fw_error_dump_mem {
	__le32 type;
	__le32 offset;
	u8 data[];
};

 
#define IWL_INI_DUMP_VER 1

 
#define IWL_INI_DUMP_INFO_TYPE BIT(31)

 
#define IWL_INI_DUMP_NAME_TYPE (BIT(31) | BIT(24))

 
struct iwl_fw_ini_error_dump_data {
	u8 type;
	u8 sub_type;
	u8 sub_type_ver;
	u8 reserved;
	__le32 len;
	__u8 data[];
} __packed;

 
struct iwl_fw_ini_dump_entry {
	struct list_head list;
	u32 size;
	u8 data[];
} __packed;

 
struct iwl_fw_ini_dump_file_hdr {
	__le32 barker;
	__le32 file_len;
} __packed;

 
struct iwl_fw_ini_fifo_hdr {
	__le32 fifo_num;
	__le32 num_of_registers;
} __packed;

 
struct iwl_fw_ini_error_dump_range {
	__le32 range_data_size;
	union {
		__le32 internal_base_addr __packed;
		__le64 dram_base_addr __packed;
		__le32 page_num __packed;
		struct iwl_fw_ini_fifo_hdr fifo_hdr;
		struct iwl_cmd_header fw_pkt_hdr;
	};
	__le32 data[];
} __packed;

 
struct iwl_fw_ini_error_dump_header {
	__le32 version;
	__le32 region_id;
	__le32 num_of_ranges;
	__le32 name_len;
	u8 name[IWL_FW_INI_MAX_NAME];
};

 
struct iwl_fw_ini_error_dump {
	struct iwl_fw_ini_error_dump_header header;
	u8 data[];
} __packed;

 
#define IWL_RXF_UMAC_BIT BIT(31)

 
struct iwl_fw_ini_error_dump_register {
	__le32 addr;
	__le32 data;
} __packed;

 
struct iwl_fw_ini_dump_cfg_name {
	__le32 image_type;
	__le32 cfg_name_len;
	u8 cfg_name[IWL_FW_INI_MAX_CFG_NAME];
} __packed;

 
#define IWL_AX210_HW_TYPE 0x42
 
#define IWL_AX210_HW_TYPE_ADDITION_SHIFT 12

 
struct iwl_fw_ini_dump_info {
	__le32 version;
	__le32 time_point;
	__le32 trigger_reason;
	__le32 external_cfg_state;
	__le32 ver_type;
	__le32 ver_subtype;
	__le32 hw_step;
	__le32 hw_type;
	__le32 rf_id_flavor;
	__le32 rf_id_dash;
	__le32 rf_id_step;
	__le32 rf_id_type;
	__le32 lmac_major;
	__le32 lmac_minor;
	__le32 umac_major;
	__le32 umac_minor;
	__le32 fw_mon_mode;
	__le64 regions_mask;
	__le32 build_tag_len;
	u8 build_tag[FW_VER_HUMAN_READABLE_SZ];
	__le32 num_of_cfg_names;
	struct iwl_fw_ini_dump_cfg_name cfg_names[];
} __packed;

 
struct iwl_fw_ini_err_table_dump {
	struct iwl_fw_ini_error_dump_header header;
	__le32 version;
	u8 data[];
} __packed;

 
struct iwl_fw_error_dump_rb {
	__le32 index;
	__le32 rxq;
	__le32 reserved;
	u8 data[];
};

 
struct iwl_fw_ini_monitor_dump {
	struct iwl_fw_ini_error_dump_header header;
	__le32 write_ptr;
	__le32 cycle_cnt;
	__le32 cur_frag;
	u8 data[];
} __packed;

 
struct iwl_fw_ini_special_device_memory {
	struct iwl_fw_ini_error_dump_header header;
	__le16 type;
	__le16 version;
	u8 data[];
} __packed;

 
struct iwl_fw_error_dump_paging {
	__le32 index;
	__le32 reserved;
	u8 data[];
};

 
static inline struct iwl_fw_error_dump_data *
iwl_fw_error_next_data(struct iwl_fw_error_dump_data *data)
{
	return (void *)(data->data + le32_to_cpu(data->len));
}

 
enum iwl_fw_dbg_trigger {
	FW_DBG_TRIGGER_INVALID = 0,
	FW_DBG_TRIGGER_USER,
	FW_DBG_TRIGGER_FW_ASSERT,
	FW_DBG_TRIGGER_MISSED_BEACONS,
	FW_DBG_TRIGGER_CHANNEL_SWITCH,
	FW_DBG_TRIGGER_FW_NOTIF,
	FW_DBG_TRIGGER_MLME,
	FW_DBG_TRIGGER_STATS,
	FW_DBG_TRIGGER_RSSI,
	FW_DBG_TRIGGER_TXQ_TIMERS,
	FW_DBG_TRIGGER_TIME_EVENT,
	FW_DBG_TRIGGER_BA,
	FW_DBG_TRIGGER_TX_LATENCY,
	FW_DBG_TRIGGER_TDLS,
	FW_DBG_TRIGGER_TX_STATUS,
	FW_DBG_TRIGGER_ALIVE_TIMEOUT,
	FW_DBG_TRIGGER_DRIVER,

	 
	FW_DBG_TRIGGER_MAX,
};

 
struct iwl_fw_error_dump_trigger_desc {
	__le32 type;
	u8 data[];
};

#endif  
