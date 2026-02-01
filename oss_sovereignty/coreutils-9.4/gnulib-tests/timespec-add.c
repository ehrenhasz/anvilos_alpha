 

 

#include <config.h>
#include "timespec.h"

#include <stdckdint.h>
#include "intprops.h"

struct timespec
timespec_add (struct timespec a, struct timespec b)
{
  time_t rs = a.tv_sec;
  time_t bs = b.tv_sec;
  int ns = a.tv_nsec + b.tv_nsec;
  int nsd = ns - TIMESPEC_HZ;
  int rns = ns;

  if (0 <= nsd)
    {
      rns = nsd;
      time_t bs1;
      if (!ckd_add (&bs1, bs, 1))
        bs = bs1;
      else if (rs < 0)
        rs++;
      else
        goto high_overflow;
    }

  if (ckd_add (&rs, rs, bs))
    {
      if (bs < 0)
        {
          rs = TYPE_MINIMUM (time_t);
          rns = 0;
        }
      else
        {
        high_overflow:
          rs = TYPE_MAXIMUM (time_t);
          rns = TIMESPEC_HZ - 1;
        }
    }

  return make_timespec (rs, rns);
}
