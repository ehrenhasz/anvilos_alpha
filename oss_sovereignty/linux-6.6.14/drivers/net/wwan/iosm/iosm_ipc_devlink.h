 

#ifndef _IOSM_IPC_DEVLINK_H_
#define _IOSM_IPC_DEVLINK_H_

#include <net/devlink.h>

#include "iosm_ipc_imem.h"
#include "iosm_ipc_imem_ops.h"
#include "iosm_ipc_pcie.h"

 
#define IOSM_DEVLINK_MAX_IMG_LEN 3
 
#define IOSM_DEVLINK_MAGIC_HEADER "IOSM_DEVLINK_HEADER"
 
#define IOSM_DEVLINK_MAGIC_HEADER_LEN 20
 
#define IOSM_DEVLINK_IMG_TYPE 4
 
#define IOSM_DEVLINK_RESERVED 34
 
#define IOSM_DEVLINK_HDR_SIZE sizeof(struct iosm_devlink_image)
 
#define IOSM_MAX_FILENAME_LEN 32
 
#define IOSM_EBL_RSP_SIZE 76
 
#define IOSM_NOF_CD_REGION 6
 
#define MAX_SNAPSHOTS 1
 
#define REPORT_JSON_SIZE 0x800
#define COREDUMP_FCD_SIZE 0x10E00000
#define CDD_LOG_SIZE 0x30000
#define EEPROM_BIN_SIZE 0x10000
#define BOOTCORE_TRC_BIN_SIZE 0x8000
#define BOOTCORE_PREV_TRC_BIN_SIZE 0x20000

 

enum iosm_devlink_param_id {
	IOSM_DEVLINK_PARAM_ID_BASE = DEVLINK_PARAM_GENERIC_ID_MAX,
	IOSM_DEVLINK_PARAM_ID_ERASE_FULL_FLASH,
};

 
enum iosm_rpsi_cmd_code {
	rpsi_cmd_code_ebl = 0x02,
	rpsi_cmd_coredump_start = 0x10,
	rpsi_cmd_coredump_get   = 0x11,
	rpsi_cmd_coredump_end   = 0x12,
};

 
enum iosm_flash_comp_type {
	FLASH_COMP_TYPE_PSI,
	FLASH_COMP_TYPE_EBL,
	FLASH_COMP_TYPE_FLS,
	FLASH_COMP_TYPE_INVAL,
};

 
struct iosm_devlink_sio {
	struct sk_buff_head rx_list;
	struct completion read_sem;
	int channel_id;
	struct ipc_mem_channel *channel;
	u32 devlink_read_pend;
};

 
struct iosm_flash_params {
	u8 erase_full_flash;
	u8 erase_full_flash_done;
};

 
struct iosm_devlink_image {
	char magic_header[IOSM_DEVLINK_MAGIC_HEADER_LEN];
	char image_type[IOSM_DEVLINK_IMG_TYPE];
	__le32 region_address;
	u8 download_region;
	u8 last_region;
	u8 reserved[IOSM_DEVLINK_RESERVED];
} __packed;

 
struct iosm_ebl_ctx_data {
	u8 ebl_sw_info_version;
	u8 m_ebl_resp[IOSM_EBL_RSP_SIZE];
};

 
struct iosm_coredump_file_info {
	char filename[IOSM_MAX_FILENAME_LEN];
	u32 default_size;
	u32 actual_size;
	u32 entry;
};

 
struct iosm_devlink {
	struct iosm_devlink_sio devlink_sio;
	struct iosm_pcie *pcie;
	struct device *dev;
	struct devlink *devlink_ctx;
	struct iosm_flash_params param;
	struct iosm_ebl_ctx_data ebl_ctx;
	struct iosm_coredump_file_info *cd_file_info;
	struct devlink_region_ops iosm_devlink_mdm_coredump[IOSM_NOF_CD_REGION];
	struct devlink_region *cd_regions[IOSM_NOF_CD_REGION];
};

 
union iosm_rpsi_param_u {
	__le16 word[2];
	__le32 dword;
};

 
struct iosm_rpsi_cmd {
	union iosm_rpsi_param_u param;
	__le16	cmd;
	__le16	crc;
};

struct iosm_devlink *ipc_devlink_init(struct iosm_imem *ipc_imem);

void ipc_devlink_deinit(struct iosm_devlink *ipc_devlink);

int ipc_devlink_send_cmd(struct iosm_devlink *ipc_devlink, u16 cmd, u32 entry);

#endif  
