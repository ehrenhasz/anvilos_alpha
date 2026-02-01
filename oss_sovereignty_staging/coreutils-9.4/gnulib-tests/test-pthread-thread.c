 

#include <config.h>

#include <pthread.h>

#include <stdio.h>
#include <string.h>

#include "macros.h"

static pthread_t main_thread_before;
static pthread_t main_thread_after;
static pthread_t worker_thread;

#define MAGIC ((void *) 1266074729)
static volatile int work_done;

static void *
worker_thread_func (void *arg)
{
  work_done = 1;
  return MAGIC;
}

int
main ()
{
  main_thread_before = pthread_self ();

  if (pthread_create (&worker_thread, NULL, worker_thread_func, NULL) == 0)
    {
      void *ret;

       
      main_thread_after = pthread_self ();
      ASSERT (memcmp (&main_thread_before, &main_thread_after,
                      sizeof (pthread_t))
              == 0);

      ASSERT (pthread_join (worker_thread, &ret) == 0);

       
      ASSERT (ret == MAGIC);

       
      ASSERT (work_done);

      return 0;
    }
  else
    {
      fputs ("pthread_create failed\n", stderr);
      return 1;
    }
}
