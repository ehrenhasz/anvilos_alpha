


#include <linux/hrtimer.h>
#include <linux/kthread.h>

#define MAX_DOGS	32	


struct watchdog_core_data {
	struct device dev;
	struct cdev cdev;
	struct watchdog_device *wdd;
	struct mutex lock;
	ktime_t last_keepalive;
	ktime_t last_hw_keepalive;
	ktime_t open_deadline;
	struct hrtimer timer;
	struct kthread_work work;
#if IS_ENABLED(CONFIG_WATCHDOG_HRTIMER_PRETIMEOUT)
	struct hrtimer pretimeout_timer;
#endif
	unsigned long status;		
#define _WDOG_DEV_OPEN		0	
#define _WDOG_ALLOW_RELEASE	1	
#define _WDOG_KEEPALIVE		2	
};


extern int watchdog_dev_register(struct watchdog_device *);
extern void watchdog_dev_unregister(struct watchdog_device *);
extern int __init watchdog_dev_init(void);
extern void __exit watchdog_dev_exit(void);

static inline bool watchdog_have_pretimeout(struct watchdog_device *wdd)
{
	return wdd->info->options & WDIOF_PRETIMEOUT ||
	       IS_ENABLED(CONFIG_WATCHDOG_HRTIMER_PRETIMEOUT);
}

#if IS_ENABLED(CONFIG_WATCHDOG_HRTIMER_PRETIMEOUT)
void watchdog_hrtimer_pretimeout_init(struct watchdog_device *wdd);
void watchdog_hrtimer_pretimeout_start(struct watchdog_device *wdd);
void watchdog_hrtimer_pretimeout_stop(struct watchdog_device *wdd);
#else
static inline void watchdog_hrtimer_pretimeout_init(struct watchdog_device *wdd) {}
static inline void watchdog_hrtimer_pretimeout_start(struct watchdog_device *wdd) {}
static inline void watchdog_hrtimer_pretimeout_stop(struct watchdog_device *wdd) {}
#endif
