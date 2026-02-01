 
 

#ifndef _COREDUMP_H_
#define _COREDUMP_H_

#include "core.h"

#define ATH10K_FW_CRASH_DUMP_VERSION 1

 
enum ath10k_fw_crash_dump_type {
	ATH10K_FW_CRASH_DUMP_REGISTERS = 0,
	ATH10K_FW_CRASH_DUMP_CE_DATA = 1,

	 
	ATH10K_FW_CRASH_DUMP_RAM_DATA = 2,

	ATH10K_FW_CRASH_DUMP_MAX,
};

struct ath10k_tlv_dump_data {
	 
	__le32 type;

	 
	__le32 tlv_len;

	 
	u8 tlv_data[];
} __packed;

struct ath10k_dump_file_data {
	 

	 
	char df_magic[16];

	__le32 len;

	 
	__le32 version;

	 

	guid_t guid;

	__le32 chip_id;

	 
	__le32 bus_type;

	__le32 target_version;
	__le32 fw_version_major;
	__le32 fw_version_minor;
	__le32 fw_version_release;
	__le32 fw_version_build;
	__le32 phy_capability;
	__le32 hw_min_tx_power;
	__le32 hw_max_tx_power;
	__le32 ht_cap_info;
	__le32 vht_cap_info;
	__le32 num_rf_chains;

	 
	char fw_ver[ETHTOOL_FWVERS_LEN];

	 

	 
	__le64 tv_sec;

	 
	__le64 tv_nsec;

	 
	__le32 kernel_ver_code;

	 
	char kernel_ver[64];

	 
	u8 unused[128];

	 
	u8 data[];
} __packed;

struct ath10k_dump_ram_data_hdr {
	 
	__le32 region_type;

	__le32 start;

	 
	__le32 length;

	u8 data[];
};

 
#define ATH10K_MAGIC_NOT_COPIED		0xAA

 
enum ath10k_mem_region_type {
	ATH10K_MEM_REGION_TYPE_REG	= 1,
	ATH10K_MEM_REGION_TYPE_DRAM	= 2,
	ATH10K_MEM_REGION_TYPE_AXI	= 3,
	ATH10K_MEM_REGION_TYPE_IRAM1	= 4,
	ATH10K_MEM_REGION_TYPE_IRAM2	= 5,
	ATH10K_MEM_REGION_TYPE_IOSRAM	= 6,
	ATH10K_MEM_REGION_TYPE_IOREG	= 7,
	ATH10K_MEM_REGION_TYPE_MSA	= 8,
};

 
struct ath10k_mem_section {
	u32 start;
	u32 end;
};

 
struct ath10k_mem_region {
	enum ath10k_mem_region_type type;
	u32 start;
	u32 len;

	const char *name;

	struct {
		const struct ath10k_mem_section *sections;
		u32 size;
	} section_table;
};

 
struct ath10k_hw_mem_layout {
	u32 hw_id;
	u32 hw_rev;
	enum ath10k_bus bus;

	struct {
		const struct ath10k_mem_region *regions;
		int size;
	} region_table;
};

 
extern unsigned long ath10k_coredump_mask;

#ifdef CONFIG_DEV_COREDUMP

int ath10k_coredump_submit(struct ath10k *ar);
struct ath10k_fw_crash_data *ath10k_coredump_new(struct ath10k *ar);
int ath10k_coredump_create(struct ath10k *ar);
int ath10k_coredump_register(struct ath10k *ar);
void ath10k_coredump_unregister(struct ath10k *ar);
void ath10k_coredump_destroy(struct ath10k *ar);

const struct ath10k_hw_mem_layout *_ath10k_coredump_get_mem_layout(struct ath10k *ar);
const struct ath10k_hw_mem_layout *ath10k_coredump_get_mem_layout(struct ath10k *ar);

#else  

static inline int ath10k_coredump_submit(struct ath10k *ar)
{
	return 0;
}

static inline struct ath10k_fw_crash_data *ath10k_coredump_new(struct ath10k *ar)
{
	return NULL;
}

static inline int ath10k_coredump_create(struct ath10k *ar)
{
	return 0;
}

static inline int ath10k_coredump_register(struct ath10k *ar)
{
	return 0;
}

static inline void ath10k_coredump_unregister(struct ath10k *ar)
{
}

static inline void ath10k_coredump_destroy(struct ath10k *ar)
{
}

static inline const struct ath10k_hw_mem_layout *
ath10k_coredump_get_mem_layout(struct ath10k *ar)
{
	return NULL;
}

static inline const struct ath10k_hw_mem_layout *
_ath10k_coredump_get_mem_layout(struct ath10k *ar)
{
	return NULL;
}

#endif  

#endif  
