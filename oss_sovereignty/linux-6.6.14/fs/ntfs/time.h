 
 

#ifndef _LINUX_NTFS_TIME_H
#define _LINUX_NTFS_TIME_H

#include <linux/time.h>		 
#include <asm/div64.h>		 

#include "endian.h"

#define NTFS_TIME_OFFSET ((s64)(369 * 365 + 89) * 24 * 3600 * 10000000)

 
static inline sle64 utc2ntfs(const struct timespec64 ts)
{
	 
	return cpu_to_sle64((s64)ts.tv_sec * 10000000 + ts.tv_nsec / 100 +
			NTFS_TIME_OFFSET);
}

 
static inline sle64 get_current_ntfs_time(void)
{
	struct timespec64 ts;

	ktime_get_coarse_real_ts64(&ts);
	return utc2ntfs(ts);
}

 
static inline struct timespec64 ntfs2utc(const sle64 time)
{
	struct timespec64 ts;

	 
	u64 t = (u64)(sle64_to_cpu(time) - NTFS_TIME_OFFSET);
	 
	ts.tv_nsec = do_div(t, 10000000) * 100;
	ts.tv_sec = t;
	return ts;
}

#endif  
