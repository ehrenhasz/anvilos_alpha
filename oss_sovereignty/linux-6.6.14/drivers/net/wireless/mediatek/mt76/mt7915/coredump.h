 
 

#ifndef _COREDUMP_H_
#define _COREDUMP_H_

#include "mt7915.h"

struct trace {
	u32 id;
	u32 timestamp;
};

struct mt7915_coredump {
	char magic[16];

	u32 len;

	guid_t guid;

	 
	u64 tv_sec;
	 
	u64 tv_nsec;
	 
	char kernel[64];
	 
	char fw_ver[ETHTOOL_FWVERS_LEN];

	u32 device_id;

	 
	char fw_state[12];

	u32 last_msg_id;
	u32 eint_info_idx;
	u32 irq_info_idx;
	u32 sched_info_idx;

	 
	char trace_sched[32];
	struct {
		struct trace t;
		u32 pc;
	} sched[60];

	 
	char trace_irq[32];
	struct trace irq[60];

	 
	char task_qid[32];
	struct {
		u32 read;
		u32 write;
	} taskq[2];

	 
	char task_info[32];
	struct {
		u32 start;
		u32 end;
		u32 size;
	} taski[2];

	 
	char fw_context[24];
	struct {
		u32 idx;
		u32 handler;
	} context;

	 
	u32 call_stack[16];

	 
	u8 data[];
} __packed;

struct mt7915_coredump_mem {
	u32 len;
	u8 data[];
} __packed;

struct mt7915_mem_hdr {
	u32 start;
	u32 len;
	u8 data[];
};

struct mt7915_mem_region {
	u32 start;
	size_t len;

	const char *name;
};

#ifdef CONFIG_DEV_COREDUMP

const struct mt7915_mem_region *
mt7915_coredump_get_mem_layout(struct mt7915_dev *dev, u32 *num);
struct mt7915_crash_data *mt7915_coredump_new(struct mt7915_dev *dev);
int mt7915_coredump_submit(struct mt7915_dev *dev);
int mt7915_coredump_register(struct mt7915_dev *dev);
void mt7915_coredump_unregister(struct mt7915_dev *dev);

#else  

static inline const struct mt7915_mem_region *
mt7915_coredump_get_mem_layout(struct mt7915_dev *dev, u32 *num)
{
	return NULL;
}

static inline int mt7915_coredump_submit(struct mt7915_dev *dev)
{
	return 0;
}

static inline struct mt7915_crash_data *mt7915_coredump_new(struct mt7915_dev *dev)
{
	return NULL;
}

static inline int mt7915_coredump_register(struct mt7915_dev *dev)
{
	return 0;
}

static inline void mt7915_coredump_unregister(struct mt7915_dev *dev)
{
}

#endif  

#endif  
