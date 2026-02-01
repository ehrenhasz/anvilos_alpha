 

#include <config.h>

#include "timespec.h"

#include <sys/time.h>
#include <unistd.h>

#include <errno.h>

 

int
settime (struct timespec const *ts)
{
#if defined CLOCK_REALTIME && HAVE_CLOCK_SETTIME
  {
    int r = clock_settime (CLOCK_REALTIME, ts);
    if (r == 0 || errno == EPERM)
      return r;
  }
#endif

#if HAVE_SETTIMEOFDAY
  {
    struct timeval tv = { .tv_sec = ts->tv_sec,
                          .tv_usec = ts->tv_nsec / 1000 };
    return settimeofday (&tv, 0);
  }
#elif HAVE_STIME
   
  return stime (&ts->tv_sec);
#else
  errno = ENOSYS;
  return -1;
#endif
}
