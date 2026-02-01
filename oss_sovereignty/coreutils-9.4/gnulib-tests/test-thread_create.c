 

#include <config.h>

#include "glthread/thread.h"

#include <stdio.h>
#include <string.h>

#include "macros.h"

static gl_thread_t main_thread_before;
static gl_thread_t main_thread_after;
static gl_thread_t worker_thread;

static int dummy;
static volatile int work_done;

static void *
worker_thread_func (void *arg)
{
  work_done = 1;
  return &dummy;
}

int
main ()
{
  main_thread_before = gl_thread_self ();

  if (glthread_create (&worker_thread, worker_thread_func, NULL) == 0)
    {
      void *ret;

       
      main_thread_after = gl_thread_self ();
      ASSERT (memcmp (&main_thread_before, &main_thread_after,
                      sizeof (gl_thread_t))
              == 0);

      gl_thread_join (worker_thread, &ret);

       
      ASSERT (ret == &dummy);

       
      ASSERT (work_done);

      return 0;
    }
  else
    {
#if USE_ISOC_THREADS || USE_POSIX_THREADS || USE_ISOC_AND_POSIX_THREADS || USE_WINDOWS_THREADS
      fputs ("glthread_create failed\n", stderr);
      return 1;
#else
      fputs ("Skipping test: multithreading not enabled\n", stderr);
      return 77;
#endif
    }
}
