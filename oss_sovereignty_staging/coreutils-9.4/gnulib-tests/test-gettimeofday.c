 

#include <config.h>

#include <sys/time.h>

#include "signature.h"
SIGNATURE_CHECK (gettimeofday, int,
                 (struct timeval *, GETTIMEOFDAY_TIMEZONE *));

#include <time.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "macros.h"

static void
test_clobber ()
{
  time_t t = 0;
  struct tm *lt;
  struct tm saved_lt;
  struct timeval tv;
  lt = localtime (&t);
  saved_lt = *lt;
  gettimeofday (&tv, NULL);
  if (memcmp (lt, &saved_lt, sizeof (struct tm)) != 0)
    {
      fprintf (stderr, "gettimeofday still clobbers the localtime buffer!\n");
      exit (1);
    }
}

static void
test_consistency ()
{
  struct timeval tv1;
  time_t tt2;
  struct timeval tv3;
  time_t tt4;

  ASSERT (gettimeofday (&tv1, NULL) == 0);
  tt2 = time (NULL);
  ASSERT (gettimeofday (&tv3, NULL) == 0);
  tt4 = time (NULL);

   
  ASSERT (tv1.tv_sec < tv3.tv_sec
          || (tv1.tv_sec == tv3.tv_sec && tv1.tv_usec <= tv3.tv_usec));

   
  ASSERT (tt2 <= tt4);

   
  /* Note: It's here that the dependency to the 'time' module is needed.
     Without it, this assertion would sometimes fail on glibc systems, see
     https:
  ASSERT (tv1.tv_sec <= tt2);
  ASSERT (tt2 <= tv3.tv_sec);
  ASSERT (tv3.tv_sec <= tt4);
}

int
main (void)
{
  test_clobber ();
  test_consistency ();

  return 0;
}
