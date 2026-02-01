 

#ifndef DFS_PRI_DETECTOR_H
#define DFS_PRI_DETECTOR_H

#include <linux/list.h>

extern struct ath_dfs_pool_stats global_dfs_pool_stats;

 
struct pri_sequence {
	struct list_head head;
	u32 pri;
	u32 dur;
	u32 count;
	u32 count_falses;
	u64 first_ts;
	u64 last_ts;
	u64 deadline_ts;
};

 
struct pri_detector {
	void (*exit)     (struct pri_detector *de);
	struct pri_sequence *
	     (*add_pulse)(struct pri_detector *de, struct pulse_event *e);
	void (*reset)    (struct pri_detector *de, u64 ts);

	const struct radar_detector_specs *rs;

 
	u64 last_ts;
	struct list_head sequences;
	struct list_head pulses;
	u32 count;
	u32 max_count;
	u32 window_size;
};

struct pri_detector *pri_detector_init(const struct radar_detector_specs *rs);

#endif  
