#ifndef	_SYS_FS_ZFS_DELAY_H
#define	_SYS_FS_ZFS_DELAY_H
#include <sys/timer.h>
#define	zfs_sleep_until(wakeup)						\
	do {								\
		hrtime_t delta = wakeup - gethrtime();			\
									\
		if (delta > 0) {					\
			unsigned long delta_us;				\
			delta_us = delta / (NANOSEC / MICROSEC);	\
			usleep_range(delta_us, delta_us + 100);		\
		}							\
	} while (0)
#endif	 
