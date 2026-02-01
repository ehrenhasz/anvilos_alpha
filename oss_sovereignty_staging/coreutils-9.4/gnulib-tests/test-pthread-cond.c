 
#define DO_TEST_COND 1
#define DO_TEST_TIMEDCOND 1

 
#define EXPLICIT_YIELD 1

 
#define ENABLE_DEBUGGING 0

#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>

#if EXPLICIT_YIELD
# include <sched.h>
#endif

#if HAVE_DECL_ALARM
# include <signal.h>
# include <unistd.h>
#endif

#include "macros.h"

#if ENABLE_DEBUGGING
# define dbgprintf printf
#else
# define dbgprintf if (0) printf
#endif

#if EXPLICIT_YIELD
# define yield() sched_yield ()
#else
# define yield()
#endif


 
static int cond_value = 0;
static pthread_cond_t condtest;
static pthread_mutex_t lockcond;

static void *
pthread_cond_wait_routine (void *arg)
{
  ASSERT (pthread_mutex_lock (&lockcond) == 0);
  while (!cond_value)
    {
      ASSERT (pthread_cond_wait (&condtest, &lockcond) == 0);
    }
  ASSERT (pthread_mutex_unlock (&lockcond) == 0);

  cond_value = 2;

  return NULL;
}

static void
test_pthread_cond_wait ()
{
  struct timespec remain;
  pthread_t thread;
  int ret;

  remain.tv_sec = 2;
  remain.tv_nsec = 0;

  cond_value = 0;

  ASSERT (pthread_create (&thread, NULL, pthread_cond_wait_routine, NULL) == 0);
  do
    {
      yield ();
      ret = nanosleep (&remain, &remain);
      ASSERT (ret >= -1);
    }
  while (ret == -1 && (remain.tv_sec != 0 || remain.tv_nsec != 0));

   
  ASSERT (pthread_mutex_lock (&lockcond) == 0);
  cond_value = 1;
  ASSERT (pthread_cond_signal (&condtest) == 0);
  ASSERT (pthread_mutex_unlock (&lockcond) == 0);

  ASSERT (pthread_join (thread, NULL) == 0);

  if (cond_value != 2)
    abort ();
}


 
static int cond_timeout;

static void
get_ts (struct timespec *ts)
{
  struct timeval now;

  gettimeofday (&now, NULL);

  ts->tv_sec = now.tv_sec + 1;
  ts->tv_nsec = now.tv_usec * 1000;
}

static void *
pthread_cond_timedwait_routine (void *arg)
{
  int ret;
  struct timespec ts;

  ASSERT (pthread_mutex_lock (&lockcond) == 0);
  while (!cond_value)
    {
      get_ts (&ts);
      ret = pthread_cond_timedwait (&condtest, &lockcond, &ts);
      if (ret == ETIMEDOUT)
        cond_timeout = 1;
    }
  ASSERT (pthread_mutex_unlock (&lockcond) == 0);

  return NULL;
}

static void
test_pthread_cond_timedwait (void)
{
  struct timespec remain;
  pthread_t thread;
  int ret;

  remain.tv_sec = 2;
  remain.tv_nsec = 0;

  cond_value = cond_timeout = 0;

  ASSERT (pthread_create (&thread, NULL, pthread_cond_timedwait_routine, NULL)
          == 0);
  do
    {
      yield ();
      ret = nanosleep (&remain, &remain);
      ASSERT (ret >= -1);
    }
  while (ret == -1 && (remain.tv_sec != 0 || remain.tv_nsec != 0));

   
  ASSERT (pthread_mutex_lock (&lockcond) == 0);
  cond_value = 1;
  ASSERT (pthread_cond_signal (&condtest) == 0);
  ASSERT (pthread_mutex_unlock (&lockcond) == 0);

  ASSERT (pthread_join (thread, NULL) == 0);

  if (!cond_timeout)
    abort ();
}

int
main ()
{
#if HAVE_DECL_ALARM
   
  int alarm_value = 600;
  signal (SIGALRM, SIG_DFL);
  alarm (alarm_value);
#endif

  ASSERT (pthread_cond_init (&condtest, NULL) == 0);

  {
    pthread_mutexattr_t attr;

    ASSERT (pthread_mutexattr_init (&attr) == 0);
    ASSERT (pthread_mutexattr_settype (&attr, PTHREAD_MUTEX_NORMAL) == 0);
    ASSERT (pthread_mutex_init (&lockcond, &attr) == 0);
    ASSERT (pthread_mutexattr_destroy (&attr) == 0);
  }

#if DO_TEST_COND
  printf ("Starting test_pthread_cond_wait ..."); fflush (stdout);
  test_pthread_cond_wait ();
  printf (" OK\n"); fflush (stdout);
#endif
#if DO_TEST_TIMEDCOND
  printf ("Starting test_pthread_cond_timedwait ..."); fflush (stdout);
  test_pthread_cond_timedwait ();
  printf (" OK\n"); fflush (stdout);
#endif

  return 0;
}

#else

 

#include <stdio.h>

int
main ()
{
  fputs ("Skipping test: multithreading not enabled\n", stderr);
  return 77;
}

#endif
