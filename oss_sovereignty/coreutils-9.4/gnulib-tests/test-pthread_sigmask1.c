 

#include <config.h>

#include <signal.h>

#include "signature.h"
SIGNATURE_CHECK (pthread_sigmask, int, (int, const sigset_t *, sigset_t *));

#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "macros.h"

#if !(defined _WIN32 && !defined __CYGWIN__)

static volatile int sigint_occurred;

static void
sigint_handler (int sig)
{
  sigint_occurred++;
}

int
main (int argc, char *argv[])
{
  sigset_t set;
  pid_t pid = getpid ();
  char command[80];

  if (LONG_MAX < pid)
    {
      fputs ("Skipping test: pid too large\n", stderr);
      return 77;
    }

  signal (SIGINT, sigint_handler);

  sigemptyset (&set);
  sigaddset (&set, SIGINT);

   
  ASSERT (pthread_sigmask (1729, &set, NULL) == EINVAL);

   
  ASSERT (pthread_sigmask (SIG_BLOCK, &set, NULL) == 0);

   
  sprintf (command, "sh -c 'sleep 1; kill -INT %ld' &", (long) pid);
  ASSERT (system (command) == 0);

   
  sleep (2);

   
  ASSERT (sigint_occurred == 0);

   
  ASSERT (pthread_sigmask (SIG_UNBLOCK, &set, NULL) == 0);

   
  ASSERT (sigint_occurred == 1);

  return 0;
}

#else

 

int
main ()
{
  fputs ("Skipping test: native Windows platform\n", stderr);
  return 77;
}

#endif
