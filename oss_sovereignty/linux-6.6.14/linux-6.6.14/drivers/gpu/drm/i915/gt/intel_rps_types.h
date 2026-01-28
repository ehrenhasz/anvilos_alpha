#ifndef INTEL_RPS_TYPES_H
#define INTEL_RPS_TYPES_H
#include <linux/atomic.h>
#include <linux/ktime.h>
#include <linux/mutex.h>
#include <linux/types.h>
#include <linux/workqueue.h>
struct intel_ips {
	u64 last_count1;
	unsigned long last_time1;
	unsigned long chipset_power;
	u64 last_count2;
	u64 last_time2;
	unsigned long gfx_power;
	u8 corr;
	int c, m;
};
struct intel_rps_ei {
	ktime_t ktime;
	u32 render_c0;
	u32 media_c0;
};
enum {
	INTEL_RPS_ENABLED = 0,
	INTEL_RPS_ACTIVE,
	INTEL_RPS_INTERRUPTS,
	INTEL_RPS_TIMER,
};
struct intel_rps_freq_caps {
	u8 rp0_freq;
	u8 rp1_freq;
	u8 min_freq;
};
struct intel_rps {
	struct mutex lock;  
	struct timer_list timer;
	struct work_struct work;
	unsigned long flags;
	ktime_t pm_timestamp;
	u32 pm_interval;
	u32 pm_iir;
	u32 pm_intrmsk_mbz;
	u32 pm_events;
	u8 cur_freq;		 
	u8 last_freq;		 
	u8 min_freq_softlimit;	 
	u8 max_freq_softlimit;	 
	u8 max_freq;		 
	u8 min_freq;		 
	u8 boost_freq;		 
	u8 idle_freq;		 
	u8 efficient_freq;	 
	u8 rp1_freq;		 
	u8 rp0_freq;		 
	u16 gpll_ref_freq;	 
	int last_adj;
	struct {
		struct mutex mutex;
		enum { LOW_POWER, BETWEEN, HIGH_POWER } mode;
		unsigned int interactive;
		u8 up_threshold;  
		u8 down_threshold;  
	} power;
	atomic_t num_waiters;
	unsigned int boosts;
	struct intel_rps_ei ei;
	struct intel_ips ips;
};
#endif  
