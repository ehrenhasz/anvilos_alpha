 

 

#include "config.h"

#if !defined (HAVE_GETTIMEOFDAY)

#include "posixtime.h"

 
int
gettimeofday (struct timeval *tv, void *tz)
{
  tv->tv_sec = (time_t) time ((time_t *)0);
  tv->tv_usec = 0;
  return 0;
}
#endif
