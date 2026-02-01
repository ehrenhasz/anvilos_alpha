 

#include <config.h>

#include <time.h>

#include "signature.h"
SIGNATURE_CHECK (time, time_t, (time_t *));

#include <sys/time.h>

#include "macros.h"

int
main (void)
{
   
  struct timeval tv1;
  struct timeval tv2;
  time_t tt3;

   
  ASSERT (gettimeofday (&tv1, NULL) == 0);
  do
    ASSERT (gettimeofday (&tv2, NULL) == 0);
  while (tv2.tv_sec == tv1.tv_sec);
   
  tt3 = time (NULL);
  ASSERT (tt3 >= tv2.tv_sec);

  return 0;
}
