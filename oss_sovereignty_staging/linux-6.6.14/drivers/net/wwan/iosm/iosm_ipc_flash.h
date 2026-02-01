 

#ifndef _IOSM_IPC_FLASH_H
#define _IOSM_IPC_FLASH_H

 
#define IOSM_FLS_BUF_SIZE 0x00100000
 
#define IOSM_ERASE_START_ADDR 0x00000000
 
#define IOSM_ERASE_LEN 0xFFFFFFFF
 
#define IOSM_EBL_HEAD_SIZE  8
 
#define IOSM_EBL_W_PAYL_SIZE  2048
 
#define IOSM_EBL_W_PACK_SIZE  (IOSM_EBL_HEAD_SIZE + IOSM_EBL_W_PAYL_SIZE)
 
#define IOSM_EBL_DW_PAYL_SIZE  16384
 
#define IOSM_EBL_DW_PACK_SIZE  (IOSM_EBL_HEAD_SIZE + IOSM_EBL_DW_PAYL_SIZE)
 
#define IOSM_EBL_NAME  32
 
#define IOSM_MAX_ERRORS 8
 
#define IOSM_READ_SIZE 2
 
#define IOSM_LER_ACK_SIZE 2
 
#define IOSM_PSI_ACK 8
 
#define IOSM_EXT_CAP_SWID_OOS_PACK     0x02
 
#define IOSM_EBL_RSP_BUFF 0x0041
 
#define IOSM_SWID_STR 64
 
#define IOSM_RPSI_LOAD_SIZE 0
 
#define IOSM_EBL_CKSM 0x0000FFFF
 
#define IOSM_MSG_LEN_ARG 0
 
#define IOSM_MDM_SEND_DATA 0x0000
 
#define IOSM_MDM_ERASE_RSP 0x0001
 
#define IOSM_EBL_PAYL_SHIFT 16
 
#define IOSM_SET_FLAG 1
 
#define IOSM_FLASH_ERASE_CHECK_TIMEOUT 100
 
#define IOSM_FLASH_ERASE_CHECK_INTERVAL 20
 
#define IOSM_LER_RSP_SIZE 60

 
enum iosm_flash_package_type {
	FLASH_SET_PROT_CONF = 0x0086,
	FLASH_SEC_START = 0x0204,
	FLASH_SEC_END,
	FLASH_SET_ADDRESS = 0x0802,
	FLASH_ERASE_START = 0x0805,
	FLASH_ERASE_CHECK,
	FLASH_OOS_CONTROL = 0x080C,
	FLASH_OOS_DATA_READ = 0x080E,
	FLASH_WRITE_IMAGE_RAW,
};

 
enum iosm_out_of_session_action {
	FLASH_OOSC_ACTION_READ = 2,
	FLASH_OOSC_ACTION_ERASE = 3,
};

 
enum iosm_out_of_session_type {
	FLASH_OOSC_TYPE_ALL_FLASH = 8,
	FLASH_OOSC_TYPE_SWID_TABLE = 16,
};

 
enum iosm_ebl_caps {
	IOSM_CAP_NOT_ENHANCED = 0x00,
	IOSM_CAP_USE_EXT_CAP = 0x01,
	IOSM_EXT_CAP_ERASE_ALL = 0x08,
	IOSM_EXT_CAP_COMMIT_ALL = 0x20,
};

 
enum iosm_ebl_rsp {
	EBL_CAPS_FLAG = 50,
	EBL_SKIP_ERASE = 54,
	EBL_SKIP_CRC = 55,
	EBL_EXT_CAPS_HANDLED = 57,
	EBL_OOS_CONFIG = 64,
	EBL_RSP_SW_INFO_VER = 70,
};

 
enum iosm_mdm_send_recv_data {
	IOSM_MDM_SEND_2 = 2,
	IOSM_MDM_SEND_4 = 4,
	IOSM_MDM_SEND_8 = 8,
	IOSM_MDM_SEND_16 = 16,
};

 
struct iosm_ebl_one_error {
	u16 error_class;
	u16 error_code;
};

 
struct iosm_ebl_error {
	struct iosm_ebl_one_error error[IOSM_MAX_ERRORS];
};

 
struct iosm_swid_table {
	u32 number_of_data_sets;
	char sw_id_type[IOSM_EBL_NAME];
	u32 sw_id_val;
	char rf_engine_id_type[IOSM_EBL_NAME];
	u32 rf_engine_id_val;
};

 
struct iosm_flash_msg_control {
	__le32 action;
	__le32 type;
	__le32 length;
	__le32 arguments;
};

 
struct iosm_flash_data {
	__le16  checksum;
	__le16  pack_id;
	__le32  msg_length;
};

int ipc_flash_boot_psi(struct iosm_devlink *ipc_devlink,
		       const struct firmware *fw);

int ipc_flash_boot_ebl(struct iosm_devlink *ipc_devlink,
		       const struct firmware *fw);

int ipc_flash_boot_set_capabilities(struct iosm_devlink *ipc_devlink,
				    u8 *mdm_rsp);

int ipc_flash_link_establish(struct iosm_imem *ipc_imem);

int ipc_flash_read_swid(struct iosm_devlink *ipc_devlink, u8 *mdm_rsp);

int ipc_flash_send_fls(struct iosm_devlink *ipc_devlink,
		       const struct firmware *fw, u8 *mdm_rsp);
#endif
