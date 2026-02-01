 

#include <config.h>

#include "xnanosleep.h"

#include <intprops.h>
#include <timespec.h>

#include <errno.h>
#include <time.h>
#include <unistd.h>

 

int
xnanosleep (double seconds)
{
#if HAVE_PAUSE
  if (1.0 + TYPE_MAXIMUM (time_t) <= seconds)
    {
      do
        pause ();
      while (errno == EINTR);

       
    }
#endif

  struct timespec ts_sleep = dtotimespec (seconds);

  for (;;)
    {
       
      errno = 0;
      if (nanosleep (&ts_sleep, &ts_sleep) == 0)
        break;
      if (errno != EINTR && errno != 0)
        return -1;
    }

  return 0;
}
