 
 

#ifndef _BMI_H_
#define _BMI_H_

#include "core.h"

 

 
#define BMI_MAX_DATA_SIZE	256

 
#define BMI_MAX_CMDBUF_SIZE (BMI_MAX_DATA_SIZE + \
			sizeof(u32) + \
			sizeof(u32) + \
			sizeof(u32))

 
#define BMI_MAX_LARGE_DATA_SIZE	2048

 
#define BMI_MAX_LARGE_CMDBUF_SIZE (BMI_MAX_LARGE_DATA_SIZE + \
			sizeof(u32) + \
			sizeof(u32) + \
			sizeof(u32))

 

enum bmi_cmd_id {
	BMI_NO_COMMAND          = 0,
	BMI_DONE                = 1,
	BMI_READ_MEMORY         = 2,
	BMI_WRITE_MEMORY        = 3,
	BMI_EXECUTE             = 4,
	BMI_SET_APP_START       = 5,
	BMI_READ_SOC_REGISTER   = 6,
	BMI_READ_SOC_WORD       = 6,
	BMI_WRITE_SOC_REGISTER  = 7,
	BMI_WRITE_SOC_WORD      = 7,
	BMI_GET_TARGET_ID       = 8,
	BMI_GET_TARGET_INFO     = 8,
	BMI_ROMPATCH_INSTALL    = 9,
	BMI_ROMPATCH_UNINSTALL  = 10,
	BMI_ROMPATCH_ACTIVATE   = 11,
	BMI_ROMPATCH_DEACTIVATE = 12,
	BMI_LZ_STREAM_START     = 13,  
	BMI_LZ_DATA             = 14,
	BMI_NVRAM_PROCESS       = 15,
};

#define BMI_NVRAM_SEG_NAME_SZ 16

#define BMI_PARAM_GET_EEPROM_BOARD_ID 0x10
#define BMI_PARAM_GET_FLASH_BOARD_ID 0x8000
#define BMI_PARAM_FLASH_SECTION_ALL 0x10000

 
#define BMI_PARAM_GET_EXT_BOARD_ID 0x40000
#define ATH10K_BMI_EXT_BOARD_ID_SUPPORT 0x40000

#define ATH10K_BMI_BOARD_ID_FROM_OTP_MASK   0x7c00
#define ATH10K_BMI_BOARD_ID_FROM_OTP_LSB    10

#define ATH10K_BMI_CHIP_ID_FROM_OTP_MASK    0x18000
#define ATH10K_BMI_CHIP_ID_FROM_OTP_LSB     15

#define ATH10K_BMI_BOARD_ID_STATUS_MASK 0xff
#define ATH10K_BMI_EBOARD_ID_STATUS_MASK 0xff

struct bmi_cmd {
	__le32 id;  
	union {
		struct {
		} done;
		struct {
			__le32 addr;
			__le32 len;
		} read_mem;
		struct {
			__le32 addr;
			__le32 len;
			u8 payload[];
		} write_mem;
		struct {
			__le32 addr;
			__le32 param;
		} execute;
		struct {
			__le32 addr;
		} set_app_start;
		struct {
			__le32 addr;
		} read_soc_reg;
		struct {
			__le32 addr;
			__le32 value;
		} write_soc_reg;
		struct {
		} get_target_info;
		struct {
			__le32 rom_addr;
			__le32 ram_addr;  
			__le32 size;
			__le32 activate;  
		} rompatch_install;
		struct {
			__le32 patch_id;
		} rompatch_uninstall;
		struct {
			__le32 count;
			__le32 patch_ids[];  
		} rompatch_activate;
		struct {
			__le32 count;
			__le32 patch_ids[];  
		} rompatch_deactivate;
		struct {
			__le32 addr;
		} lz_start;
		struct {
			__le32 len;  
			u8 payload[];  
		} lz_data;
		struct {
			u8 name[BMI_NVRAM_SEG_NAME_SZ];
		} nvram_process;
		u8 payload[BMI_MAX_CMDBUF_SIZE];
	};
} __packed;

union bmi_resp {
	struct {
		DECLARE_FLEX_ARRAY(u8, payload);
	} read_mem;
	struct {
		__le32 result;
	} execute;
	struct {
		__le32 value;
	} read_soc_reg;
	struct {
		__le32 len;
		__le32 version;
		__le32 type;
	} get_target_info;
	struct {
		__le32 patch_id;
	} rompatch_install;
	struct {
		__le32 patch_id;
	} rompatch_uninstall;
	struct {
		 
		__le32 result;
	} nvram_process;
	u8 payload[BMI_MAX_CMDBUF_SIZE];
} __packed;

struct bmi_target_info {
	u32 version;
	u32 type;
};

struct bmi_segmented_file_header {
	__le32 magic_num;
	__le32 file_flags;
	u8 data[];
};

struct bmi_segmented_metadata {
	__le32 addr;
	__le32 length;
	u8 data[];
};

#define BMI_SGMTFILE_MAGIC_NUM          0x544d4753  
#define BMI_SGMTFILE_FLAG_COMPRESS      1

 

 
#define BMI_SGMTFILE_DONE               0xffffffff

 
#define BMI_SGMTFILE_BDDATA             0xfffffffe

 
#define BMI_SGMTFILE_BEGINADDR          0xfffffffd

 
#define BMI_SGMTFILE_EXEC               0xfffffffc

 
#define BMI_COMMUNICATION_TIMEOUT_HZ (3 * HZ)

#define BMI_CE_NUM_TO_TARG 0
#define BMI_CE_NUM_TO_HOST 1

void ath10k_bmi_start(struct ath10k *ar);
int ath10k_bmi_done(struct ath10k *ar);
int ath10k_bmi_get_target_info(struct ath10k *ar,
			       struct bmi_target_info *target_info);
int ath10k_bmi_get_target_info_sdio(struct ath10k *ar,
				    struct bmi_target_info *target_info);
int ath10k_bmi_read_memory(struct ath10k *ar, u32 address,
			   void *buffer, u32 length);
int ath10k_bmi_write_memory(struct ath10k *ar, u32 address,
			    const void *buffer, u32 length);

#define ath10k_bmi_read32(ar, item, val)				\
	({								\
		int ret;						\
		u32 addr;						\
		__le32 tmp;						\
									\
		addr = host_interest_item_address(HI_ITEM(item));	\
		ret = ath10k_bmi_read_memory(ar, addr, (u8 *)&tmp, 4); \
		if (!ret)						\
			*val = __le32_to_cpu(tmp);			\
		ret;							\
	 })

#define ath10k_bmi_write32(ar, item, val)				\
	({								\
		int ret;						\
		u32 address;						\
		__le32 v = __cpu_to_le32(val);				\
									\
		address = host_interest_item_address(HI_ITEM(item));	\
		ret = ath10k_bmi_write_memory(ar, address,		\
					      (u8 *)&v, sizeof(v));	\
		ret;							\
	})

int ath10k_bmi_execute(struct ath10k *ar, u32 address, u32 param, u32 *result);
int ath10k_bmi_lz_stream_start(struct ath10k *ar, u32 address);
int ath10k_bmi_lz_data(struct ath10k *ar, const void *buffer, u32 length);

int ath10k_bmi_fast_download(struct ath10k *ar, u32 address,
			     const void *buffer, u32 length);
int ath10k_bmi_read_soc_reg(struct ath10k *ar, u32 address, u32 *reg_val);
int ath10k_bmi_write_soc_reg(struct ath10k *ar, u32 address, u32 reg_val);
int ath10k_bmi_set_start(struct ath10k *ar, u32 address);

#endif  
