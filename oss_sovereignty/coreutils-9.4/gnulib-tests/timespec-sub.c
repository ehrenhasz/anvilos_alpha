 

 

#include <config.h>
#include "timespec.h"

#include <stdckdint.h>
#include "intprops.h"

struct timespec
timespec_sub (struct timespec a, struct timespec b)
{
  time_t rs = a.tv_sec;
  time_t bs = b.tv_sec;
  int ns = a.tv_nsec - b.tv_nsec;
  int rns = ns;

  if (ns < 0)
    {
      rns = ns + TIMESPEC_HZ;
      time_t bs1;
      if (!ckd_add (&bs1, bs, 1))
        bs = bs1;
      else if (- TYPE_SIGNED (time_t) < rs)
        rs--;
      else
        goto low_overflow;
    }

  if (ckd_sub (&rs, rs, bs))
    {
      if (0 < bs)
        {
        low_overflow:
          rs = TYPE_MINIMUM (time_t);
          rns = 0;
        }
      else
        {
          rs = TYPE_MAXIMUM (time_t);
          rns = TIMESPEC_HZ - 1;
        }
    }

  return make_timespec (rs, rns);
}
