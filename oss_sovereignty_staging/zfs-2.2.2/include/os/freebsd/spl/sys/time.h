 

#ifndef _OPENSOLARIS_SYS_TIME_H_
#define	_OPENSOLARIS_SYS_TIME_H_
#pragma once
#include_next <sys/time.h>
#include <sys/debug.h>
#ifndef _SYS_KERNEL_H_
extern int hz;
#endif

#define	SEC		1
#define	MILLISEC	1000UL
#define	MICROSEC	1000000UL
#define	NANOSEC	1000000000UL
#define	TIME_MAX	LLONG_MAX

#define	MSEC2NSEC(m)	((hrtime_t)(m) * (NANOSEC / MILLISEC))
#define	NSEC2MSEC(n)	((n) / (NANOSEC / MILLISEC))

#define	USEC2NSEC(m)	((hrtime_t)(m) * (NANOSEC / MICROSEC))
#define	NSEC2USEC(n)	((n) / (NANOSEC / MICROSEC))

#define	NSEC2SEC(n)	((n) / (NANOSEC / SEC))
#define	SEC2NSEC(m)	((hrtime_t)(m) * (NANOSEC / SEC))

typedef longlong_t	hrtime_t;

#if defined(__i386__) || defined(__powerpc__)
#define	TIMESPEC_OVERFLOW(ts)						\
	((ts)->tv_sec < INT32_MIN || (ts)->tv_sec > INT32_MAX)
#else
#define	TIMESPEC_OVERFLOW(ts)						\
	((ts)->tv_sec < INT64_MIN || (ts)->tv_sec > INT64_MAX)
#endif

#define	SEC_TO_TICK(sec)	((sec) * hz)
#define	NSEC_TO_TICK(nsec)	((nsec) / (NANOSEC / hz))

static __inline hrtime_t
gethrtime(void)
{
	struct timespec ts;
	hrtime_t nsec;

	nanouptime(&ts);
	nsec = ((hrtime_t)ts.tv_sec * NANOSEC) + ts.tv_nsec;
	return (nsec);
}

#define	gethrestime_sec()	(time_second)
#define	gethrestime(ts)		getnanotime(ts)
#define	gethrtime_waitfree()	gethrtime()

extern int nsec_per_tick;	 

#define	ddi_get_lbolt64()				\
	(int64_t)(((getsbinuptime() >> 16) * hz) >> 16)
#define	ddi_get_lbolt()		(clock_t)ddi_get_lbolt64()

#else

static __inline hrtime_t
gethrtime(void)
{
	struct timespec ts;
	clock_gettime(CLOCK_UPTIME, &ts);
	return (((u_int64_t)ts.tv_sec) * NANOSEC + ts.tv_nsec);
}
#endif	 
