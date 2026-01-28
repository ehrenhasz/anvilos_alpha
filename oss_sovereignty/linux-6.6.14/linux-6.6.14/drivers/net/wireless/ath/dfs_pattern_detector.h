#ifndef DFS_PATTERN_DETECTOR_H
#define DFS_PATTERN_DETECTOR_H
#include <linux/types.h>
#include <linux/list.h>
#include <linux/nl80211.h>
#define PRI_TOLERANCE	16
struct ath_dfs_pool_stats {
	u32 pool_reference;
	u32 pulse_allocated;
	u32 pulse_alloc_error;
	u32 pulse_used;
	u32 pseq_allocated;
	u32 pseq_alloc_error;
	u32 pseq_used;
};
struct pulse_event {
	u64 ts;
	u16 freq;
	u8 width;
	u8 rssi;
	bool chirp;
};
struct radar_detector_specs {
	u8 type_id;
	u8 width_min;
	u8 width_max;
	u16 pri_min;
	u16 pri_max;
	u8 num_pri;
	u8 ppb;
	u8 ppb_thresh;
	u8 max_pri_tolerance;
	bool chirp;
};
struct dfs_pattern_detector {
	void (*exit)(struct dfs_pattern_detector *dpd);
	bool (*set_dfs_domain)(struct dfs_pattern_detector *dpd,
			   enum nl80211_dfs_regions region);
	bool (*add_pulse)(struct dfs_pattern_detector *dpd,
			  struct pulse_event *pe,
			  struct radar_detector_specs *rs);
	struct ath_dfs_pool_stats (*get_stats)(struct dfs_pattern_detector *dpd);
	enum nl80211_dfs_regions region;
	u8 num_radar_types;
	u64 last_pulse_ts;
	struct ath_common *common;
	const struct radar_detector_specs *radar_spec;
	struct list_head channel_detectors;
};
extern struct dfs_pattern_detector *
dfs_pattern_detector_init(struct ath_common *common,
			  enum nl80211_dfs_regions region);
#endif  
