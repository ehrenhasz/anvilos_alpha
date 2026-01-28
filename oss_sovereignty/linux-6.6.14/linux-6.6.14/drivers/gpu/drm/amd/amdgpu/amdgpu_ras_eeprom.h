#ifndef _AMDGPU_RAS_EEPROM_H
#define _AMDGPU_RAS_EEPROM_H
#include <linux/i2c.h>
#define RAS_TABLE_VER_V1           0x00010000
#define RAS_TABLE_VER_V2_1         0x00021000
struct amdgpu_device;
enum amdgpu_ras_gpu_health_status {
	GPU_HEALTH_USABLE = 0,
	GPU_RETIRED__ECC_REACH_THRESHOLD = 2,
};
enum amdgpu_ras_eeprom_err_type {
	AMDGPU_RAS_EEPROM_ERR_NA,
	AMDGPU_RAS_EEPROM_ERR_RECOVERABLE,
	AMDGPU_RAS_EEPROM_ERR_NON_RECOVERABLE,
	AMDGPU_RAS_EEPROM_ERR_COUNT,
};
struct amdgpu_ras_eeprom_table_header {
	uint32_t header;
	uint32_t version;
	uint32_t first_rec_offset;
	uint32_t tbl_size;
	uint32_t checksum;
} __packed;
struct amdgpu_ras_eeprom_table_ras_info {
	u8  rma_status;
	u8  health_percent;
	u16 ecc_page_threshold;
	u32 padding[64 - 1];
} __packed;
struct amdgpu_ras_eeprom_control {
	struct amdgpu_ras_eeprom_table_header tbl_hdr;
	struct amdgpu_ras_eeprom_table_ras_info tbl_rai;
	u32 i2c_address;
	u32 ras_header_offset;
	u32 ras_info_offset;
	u32 ras_record_offset;
	u32 ras_num_recs;
	u32 ras_fri;
	u32 ras_max_record_count;
	struct mutex ras_tbl_mutex;
	u32 bad_channel_bitmap;
};
struct eeprom_table_record {
	union {
		uint64_t address;
		uint64_t offset;
	};
	uint64_t retired_page;
	uint64_t ts;
	enum amdgpu_ras_eeprom_err_type err_type;
	union {
		unsigned char bank;
		unsigned char cu;
	};
	unsigned char mem_channel;
	unsigned char mcumc_id;
} __packed;
int amdgpu_ras_eeprom_init(struct amdgpu_ras_eeprom_control *control,
			   bool *exceed_err_limit);
int amdgpu_ras_eeprom_reset_table(struct amdgpu_ras_eeprom_control *control);
bool amdgpu_ras_eeprom_check_err_threshold(struct amdgpu_device *adev);
int amdgpu_ras_eeprom_read(struct amdgpu_ras_eeprom_control *control,
			   struct eeprom_table_record *records, const u32 num);
int amdgpu_ras_eeprom_append(struct amdgpu_ras_eeprom_control *control,
			     struct eeprom_table_record *records, const u32 num);
uint32_t amdgpu_ras_eeprom_max_record_count(struct amdgpu_ras_eeprom_control *control);
void amdgpu_ras_debugfs_set_ret_size(struct amdgpu_ras_eeprom_control *control);
extern const struct file_operations amdgpu_ras_debugfs_eeprom_size_ops;
extern const struct file_operations amdgpu_ras_debugfs_eeprom_table_ops;
#endif  
