 

#include <config.h>

#include <time.h>

#include "signature.h"
SIGNATURE_CHECK (nanosleep, int, (struct timespec const *, struct timespec *));

#include <errno.h>
#include <signal.h>
#include <unistd.h>

#include "macros.h"

#if HAVE_DECL_ALARM
static void
handle_alarm (int sig)
{
  if (sig != SIGALRM)
    _exit (1);
}
#endif

int
main (void)
{
  struct timespec ts;

   
  ts.tv_sec = 1;
  ts.tv_nsec = -1;
  errno = 0;
  ASSERT (nanosleep (&ts, NULL) == -1);
  ASSERT (errno == EINVAL);

  ts.tv_sec = 1000;
  ts.tv_nsec = -1;
  errno = 0;
  ASSERT (nanosleep (&ts, NULL) == -1);
  ASSERT (errno == EINVAL);

   
  ts.tv_sec = 1000;
  ts.tv_nsec = 1000000000;
  errno = 0;
  ASSERT (nanosleep (&ts, NULL) == -1);
  ASSERT (errno == EINVAL);

   
  ts.tv_sec = 0;
  ts.tv_nsec = 1;
  ASSERT (nanosleep (&ts, &ts) == 0);
   
  ASSERT (ts.tv_sec == 0);
  ASSERT (ts.tv_nsec == 0 || ts.tv_nsec == 1);
  ts.tv_nsec = 0;
  ASSERT (nanosleep (&ts, NULL) == 0);

#if HAVE_DECL_ALARM
  {
    const time_t pentecost = 50 * 24 * 60 * 60;  
    signal (SIGALRM, handle_alarm);
    alarm (1);
    ts.tv_sec = pentecost;
    ts.tv_nsec = 999999999;
    errno = 0;
    ASSERT (nanosleep (&ts, &ts) == -1);
    ASSERT (errno == EINTR);
    ASSERT (pentecost - 10 < ts.tv_sec && ts.tv_sec <= pentecost);
    ASSERT (0 <= ts.tv_nsec && ts.tv_nsec <= 999999999);
  }
#endif

  return 0;
}
