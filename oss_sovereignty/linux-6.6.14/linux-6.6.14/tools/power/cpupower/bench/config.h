#define GAUGECOUNT	1500
#define SCHEDULER	SCHED_OTHER
#define PRIORITY_DEFAULT 0
#define PRIORITY_HIGH	 sched_get_priority_max(SCHEDULER)
#define PRIORITY_LOW	 sched_get_priority_min(SCHEDULER)
#ifdef DEBUG
#define dprintf printf
#else
#define dprintf(...) do { } while (0)
#endif
