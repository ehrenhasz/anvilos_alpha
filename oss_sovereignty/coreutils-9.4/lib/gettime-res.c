 

#include <config.h>

#include "timespec.h"

static long int _GL_ATTRIBUTE_CONST
gcd (long int a, long int b)
{
  while (b != 0)
    {
      long int r = a % b;
      a = b;
      b = r;
    }
  return a;
}

 

long int
gettime_res (void)
{
  struct timespec res;
#if defined CLOCK_REALTIME && HAVE_CLOCK_GETRES
  clock_getres (CLOCK_REALTIME, &res);
#elif defined HAVE_TIMESPEC_GETRES
  timespec_getres (&res, TIME_UTC);
#else
   
  res.tv_sec = 1;
  res.tv_nsec = 0;
#endif

   

  long int hz = TIMESPEC_HZ;
  long int r = res.tv_nsec <= 0 ? hz : res.tv_nsec;
  struct timespec earlier = { .tv_nsec = -1 };

   
  int nsamples = 32;
  for (int i = 0; 1 < r && i < nsamples; i++)
    {
       
      struct timespec now;
      for (;; i = nsamples)
        {
          now = current_timespec ();
          if (earlier.tv_nsec != now.tv_nsec || earlier.tv_sec != now.tv_sec)
            break;
        }
      earlier = now;

      if (0 < now.tv_nsec)
        r = gcd (r, now.tv_nsec);
    }

  return r;
}

 
