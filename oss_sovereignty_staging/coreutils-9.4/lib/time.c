 

#include <config.h>

 
#include <time.h>

#include <stdlib.h>
#include <sys/time.h>

time_t
time (time_t *tp)
{
  struct timeval tv;
  time_t tt;

  if (gettimeofday (&tv, NULL) < 0)
    abort ();
  tt = tv.tv_sec;

  if (tp)
    *tp = tt;

  return tt;
}
