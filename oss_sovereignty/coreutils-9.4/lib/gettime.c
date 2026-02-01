 

#include <config.h>

#include "timespec.h"

#include <sys/time.h>

 

void
gettime (struct timespec *ts)
{
#if defined CLOCK_REALTIME && HAVE_CLOCK_GETTIME
  clock_gettime (CLOCK_REALTIME, ts);
#elif defined HAVE_TIMESPEC_GET
  timespec_get (ts, TIME_UTC);
#else
  struct timeval tv;
  gettimeofday (&tv, NULL);
  *ts = (struct timespec) { .tv_sec  = tv.tv_sec,
                            .tv_nsec = tv.tv_usec * 1000 };
#endif
}

 

struct timespec
current_timespec (void)
{
  struct timespec ts;
  gettime (&ts);
  return ts;
}
