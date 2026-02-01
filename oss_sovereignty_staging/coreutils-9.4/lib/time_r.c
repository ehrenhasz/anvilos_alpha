 

#include <config.h>

#include <time.h>

static struct tm *
copy_tm_result (struct tm *dest, struct tm const *src)
{
  if (! src)
    return 0;
  *dest = *src;
  return dest;
}


struct tm *
gmtime_r (time_t const * restrict t, struct tm * restrict tp)
{
  return copy_tm_result (tp, gmtime (t));
}

struct tm *
localtime_r (time_t const * restrict t, struct tm * restrict tp)
{
  return copy_tm_result (tp, localtime (t));
}
