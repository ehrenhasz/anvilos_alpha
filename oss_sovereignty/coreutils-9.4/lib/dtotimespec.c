 

 

#include <config.h>

#include "timespec.h"

#include "intprops.h"

struct timespec
dtotimespec (double sec)
{
  if (! (TYPE_MINIMUM (time_t) < sec))
    return make_timespec (TYPE_MINIMUM (time_t), 0);
  else if (! (sec < 1.0 + TYPE_MAXIMUM (time_t)))
    return make_timespec (TYPE_MAXIMUM (time_t), TIMESPEC_HZ - 1);
  else
    {
      time_t s = sec;
      double frac = TIMESPEC_HZ * (sec - s);
      long ns = frac;
      ns += ns < frac;
      s += ns / TIMESPEC_HZ;
      ns %= TIMESPEC_HZ;

      if (ns < 0)
        {
          s--;
          ns += TIMESPEC_HZ;
        }

      return make_timespec (s, ns);
    }
}
