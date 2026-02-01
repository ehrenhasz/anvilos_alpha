 

#include <config.h>

#include <signal.h>

#include <errno.h>
#include <pthread.h>
#include <stdio.h>
#include <unistd.h>

#include "macros.h"

#if USE_POSIX_THREADS || USE_ISOC_AND_POSIX_THREADS

static pthread_t main_thread;
static pthread_t killer_thread;

static void *
killer_thread_func (void *arg)
{
  sleep (1);
  pthread_kill (main_thread, SIGINT);
  return NULL;
}

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

  signal (SIGINT, sigint_handler);

  sigemptyset (&set);
  sigaddset (&set, SIGINT);

   
   
#if !defined __NetBSD__
  ASSERT (pthread_sigmask (1729, &set, NULL) == EINVAL);
#endif

   
  ASSERT (pthread_sigmask (SIG_BLOCK, &set, NULL) == 0);

   
  main_thread = pthread_self ();
  ASSERT (pthread_create (&killer_thread, NULL, killer_thread_func, NULL) == 0);

   
  sleep (2);

   
  ASSERT (sigint_occurred == 0);

   
  ASSERT (pthread_sigmask (SIG_UNBLOCK, &set, NULL) == 0);

   
  ASSERT (sigint_occurred == 1);

   
  ASSERT (pthread_join (killer_thread, NULL) == 0);

  return 0;
}

#else

int
main ()
{
  fputs ("Skipping test: POSIX threads not enabled\n", stderr);
  return 77;
}

#endif
