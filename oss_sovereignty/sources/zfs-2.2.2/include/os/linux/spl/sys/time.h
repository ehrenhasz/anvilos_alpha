

#ifndef _SPL_TIME_H
#define	_SPL_TIME_H

#include <linux/module.h>
#include <linux/time.h>
#include <sys/types.h>
#include <sys/timer.h>

#if defined(CONFIG_64BIT)
#define	TIME_MAX			INT64_MAX
#define	TIME_MIN			INT64_MIN
#else
#define	TIME_MAX			INT32_MAX
#define	TIME_MIN			INT32_MIN
#endif

#define	SEC				1
#define	MILLISEC			1000
#define	MICROSEC			1000000
#define	NANOSEC				1000000000

#define	MSEC2NSEC(m)	((hrtime_t)(m) * (NANOSEC / MILLISEC))
#define	NSEC2MSEC(n)	((n) / (NANOSEC / MILLISEC))

#define	USEC2NSEC(m)	((hrtime_t)(m) * (NANOSEC / MICROSEC))
#define	NSEC2USEC(n)	((n) / (NANOSEC / MICROSEC))

#define	NSEC2SEC(n)	((n) / (NANOSEC / SEC))
#define	SEC2NSEC(m)	((hrtime_t)(m) * (NANOSEC / SEC))

static const int hz = HZ;

typedef longlong_t		hrtime_t;
typedef struct timespec		timespec_t;

#define	TIMESPEC_OVERFLOW(ts)		\
	((ts)->tv_sec < TIME_MIN || (ts)->tv_sec > TIME_MAX)

#if defined(HAVE_INODE_TIMESPEC64_TIMES)
typedef struct timespec64	inode_timespec_t;
#else
typedef struct timespec		inode_timespec_t;
#endif


#define	timestruc_t	inode_timespec_t

static inline void
gethrestime(inode_timespec_t *ts)
{
#if defined(HAVE_INODE_TIMESPEC64_TIMES)

#if defined(HAVE_KTIME_GET_COARSE_REAL_TS64)
	ktime_get_coarse_real_ts64(ts);
#else
	*ts = current_kernel_time64();
#endif 

#else
	*ts = current_kernel_time();
#endif
}

static inline uint64_t
gethrestime_sec(void)
{
#if defined(HAVE_INODE_TIMESPEC64_TIMES)
#if defined(HAVE_KTIME_GET_COARSE_REAL_TS64)
	inode_timespec_t ts;
	ktime_get_coarse_real_ts64(&ts);
#else
	inode_timespec_t ts = current_kernel_time64();
#endif  

#else
	inode_timespec_t ts = current_kernel_time();
#endif
	return (ts.tv_sec);
}

static inline hrtime_t
gethrtime(void)
{
#if defined(HAVE_KTIME_GET_RAW_TS64)
	struct timespec64 ts;
	ktime_get_raw_ts64(&ts);
#else
	struct timespec ts;
	getrawmonotonic(&ts);
#endif
	return (((hrtime_t)ts.tv_sec * NSEC_PER_SEC) + ts.tv_nsec);
}

#endif  
