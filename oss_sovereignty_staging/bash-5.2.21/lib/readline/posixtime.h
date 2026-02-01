 

 

#ifndef _POSIXTIME_H_
#define _POSIXTIME_H_

 
 
#if defined (HAVE_SYS_TIME_H)
#  include <sys/time.h>
#endif
#include <time.h>

#if !defined (HAVE_SYSCONF) || !defined (_SC_CLK_TCK)
#  if !defined (CLK_TCK)
#    if defined (HZ)
#      define CLK_TCK	HZ
#    else
#      define CLK_TCK	60			 
#    endif
#  endif  
#endif  

#if !HAVE_TIMEVAL
struct timeval
{
  time_t tv_sec;
  long int tv_usec;
};
#endif

#if !HAVE_GETTIMEOFDAY
extern int gettimeofday PARAMS((struct timeval *, void *));
#endif

 
#if !defined (timerclear)
#  define timerclear(tvp)	do { (tvp)->tv_sec = 0; (tvp)->tv_usec = 0; } while (0)
#endif
#if !defined (timerisset)
#  define timerisset(tvp)	((tvp)->tv_sec || (tvp)->tv_usec)
#endif
#if !defined (timercmp)
#  define timercmp(a, b, CMP) \
	(((a)->tv_sec == (b)->tv_sec) ? ((a)->tv_usec CMP (b)->tv_usec) \
				      : ((a)->tv_sec CMP (b)->tv_sec))
#endif

 
#if !defined (timerisunset)
#  define timerisunset(tvp)	((tvp)->tv_sec == 0 && (tvp)->tv_usec == 0)
#endif
#if !defined (timerset)
#  define timerset(tvp, s, u)	do { tvp->tv_sec = s; tvp->tv_usec = u; } while (0)
#endif

#ifndef TIMEVAL_TO_TIMESPEC
#  define TIMEVAL_TO_TIMESPEC(tv, ts) \
  do { \
    (ts)->tv_sec = (tv)->tv_sec; \
    (ts)->tv_nsec = (tv)->tv_usec * 1000; \
  } while (0)
#endif

#endif  
