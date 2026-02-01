 

#include <config.h>

#define GETHRXTIME_INLINE _GL_EXTERN_INLINE
#include "gethrxtime.h"

#if ! (HAVE_ARITHMETIC_HRTIME_T && HAVE_DECL_GETHRTIME)

#include <sys/time.h>
#include "timespec.h"

 

xtime_t
gethrxtime (void)
{
# if HAVE_NANOUPTIME
  {
    struct timespec ts;
    nanouptime (&ts);
    return xtime_make (ts.tv_sec, ts.tv_nsec);
  }
# else

#  if defined CLOCK_MONOTONIC && HAVE_CLOCK_GETTIME
  {
    struct timespec ts;
    if (clock_gettime (CLOCK_MONOTONIC, &ts) == 0)
      return xtime_make (ts.tv_sec, ts.tv_nsec);
  }
#  endif

#  if HAVE_MICROUPTIME
  {
    struct timeval tv;
    microuptime (&tv);
    return xtime_make (tv.tv_sec, 1000 * tv.tv_usec);
  }

#  else
   
  {
    struct timespec ts;
    gettime (&ts);
    return xtime_make (ts.tv_sec, ts.tv_nsec);
  }
#  endif
# endif
}

#endif
