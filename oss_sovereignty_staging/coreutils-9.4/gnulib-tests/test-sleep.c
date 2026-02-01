 

#include <config.h>

#include <unistd.h>

#include "signature.h"
SIGNATURE_CHECK (sleep, unsigned int, (unsigned int));

#include <signal.h>

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
  ASSERT (sleep (1) <= 1);

  ASSERT (sleep (0) == 0);

#if HAVE_DECL_ALARM
  {
    const unsigned int pentecost = 50 * 24 * 60 * 60;  
    unsigned int remaining;
    signal (SIGALRM, handle_alarm);
    alarm (1);
    remaining = sleep (pentecost);
    ASSERT (pentecost - 10 < remaining && remaining <= pentecost);
  }
#endif

  return 0;
}
