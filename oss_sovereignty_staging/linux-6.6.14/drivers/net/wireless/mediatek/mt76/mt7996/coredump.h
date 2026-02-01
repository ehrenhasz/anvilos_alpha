 
 

#ifndef _COREDUMP_H_
#define _COREDUMP_H_

#include "mt7996.h"

struct mt7996_coredump {
	char magic[16];

	u32 len;

	guid_t guid;

	 
	u64 tv_sec;
	 
	u64 tv_nsec;
	 
	char kernel[64];
	 
	char fw_ver[ETHTOOL_FWVERS_LEN];

	u32 device_id;

	 
	char fw_state[12];

	 
	char pc_current[16];
	u32 pc_stack[17];
	 
	u32 lr_stack[16];

	 
	u8 data[];
} __packed;

struct mt7996_coredump_mem {
	u32 len;
	u8 data[];
} __packed;

struct mt7996_mem_hdr {
	u32 start;
	u32 len;
	u8 data[];
};

struct mt7996_mem_region {
	u32 start;
	size_t len;

	const char *name;
};

#ifdef CONFIG_DEV_COREDUMP

const struct mt7996_mem_region *
mt7996_coredump_get_mem_layout(struct mt7996_dev *dev, u32 *num);
struct mt7996_crash_data *mt7996_coredump_new(struct mt7996_dev *dev);
int mt7996_coredump_submit(struct mt7996_dev *dev);
int mt7996_coredump_register(struct mt7996_dev *dev);
void mt7996_coredump_unregister(struct mt7996_dev *dev);

#else  

static inline const struct mt7996_mem_region *
mt7996_coredump_get_mem_layout(struct mt7996_dev *dev, u32 *num)
{
	return NULL;
}

static inline int mt7996_coredump_submit(struct mt7996_dev *dev)
{
	return 0;
}

static inline struct
mt7996_crash_data *mt7996_coredump_new(struct mt7996_dev *dev)
{
	return NULL;
}

static inline int mt7996_coredump_register(struct mt7996_dev *dev)
{
	return 0;
}

static inline void mt7996_coredump_unregister(struct mt7996_dev *dev)
{
}

#endif  

#endif  
