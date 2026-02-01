 

#include <config.h>

#include <sys/resource.h>

#include "signature.h"
SIGNATURE_CHECK (getrusage, int, (int, struct rusage *));

#include <sys/time.h>

#include "macros.h"

volatile unsigned int counter;

int
main (void)
{
  struct rusage before;
  struct rusage after;
  int ret;

  ret = getrusage (RUSAGE_SELF, &before);
  ASSERT (ret == 0);

   
  {
    struct timeval t0;
    ASSERT (gettimeofday (&t0, NULL) == 0);

    for (;;)
      {
        struct timeval t;
        int i;

        for (i = 0; i < 1000000; i++)
          counter++;

        ASSERT (gettimeofday (&t, NULL) == 0);
        if (t.tv_sec - t0.tv_sec > 1
            || (t.tv_sec - t0.tv_sec == 1 && t.tv_usec >= t0.tv_usec))
          break;
      }
  }

  ret = getrusage (RUSAGE_SELF, &after);
  ASSERT (ret == 0);

  ASSERT (after.ru_utime.tv_sec >= before.ru_utime.tv_sec);
  ASSERT (after.ru_stime.tv_sec >= before.ru_stime.tv_sec);
  {
     
    unsigned int spent_utime =
      (after.ru_utime.tv_sec > before.ru_utime.tv_sec
       ? (after.ru_utime.tv_sec - before.ru_utime.tv_sec - 1) * 1000000U
         + after.ru_utime.tv_usec + (1000000U - before.ru_utime.tv_usec)
       : after.ru_utime.tv_usec - before.ru_utime.tv_usec);
    unsigned int spent_stime =
      (after.ru_stime.tv_sec > before.ru_stime.tv_sec
       ? (after.ru_stime.tv_sec - before.ru_stime.tv_sec - 1) * 1000000U
         + after.ru_stime.tv_usec + (1000000U - before.ru_stime.tv_usec)
       : after.ru_stime.tv_usec - before.ru_stime.tv_usec);

    ASSERT (spent_utime + spent_stime <= 2 * 1000000U);
     
    ASSERT (spent_utime + spent_stime > 10000U);
  }

  return 0;
}
