 

#include <config.h>

#if USE_ISOC_THREADS || USE_POSIX_THREADS || USE_ISOC_AND_POSIX_THREADS || USE_WINDOWS_THREADS

#include "glthread/lock.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "glthread/thread.h"

 

#define SUCCEED() exit (0)
#define FAILURE() exit (1)
#define UNEXPECTED(n) (fprintf (stderr, "Unexpected outcome %d\n", n), abort ())

 

static gl_rwlock_t lock;
static gl_thread_t reader1;
static gl_thread_t writer;
static gl_thread_t reader2;
static gl_thread_t timer;
 
static gl_lock_t baton;

static void *
timer_func (void *ignored)
{
   
  if (glthread_lock_lock (&baton))
    UNEXPECTED (13);
  usleep (100000);
   
  SUCCEED ();
}

static void *
reader2_func (void *ignored)
{
  int err;

   
  if (glthread_lock_lock (&baton))
    UNEXPECTED (8);
  usleep (100000);
   
   
  if (glthread_create (&timer, timer_func, NULL))
    UNEXPECTED (10);
   
  if (glthread_lock_unlock (&baton))
    UNEXPECTED (11);
   
  err = glthread_rwlock_rdlock (&lock);
  if (err == 0)
    FAILURE ();
  else
    UNEXPECTED (12);
}

static void *
writer_func (void *ignored)
{
   
  if (glthread_lock_lock (&baton))
    UNEXPECTED (4);
   
  if (glthread_create (&reader2, reader2_func, NULL))
    UNEXPECTED (5);
   
  if (glthread_lock_unlock (&baton))
    UNEXPECTED (6);
   
  if (glthread_rwlock_wrlock (&lock))
    UNEXPECTED (7);
  return NULL;
}

int
main ()
{
  reader1 = gl_thread_self ();

   
  if (glthread_rwlock_init (&lock))
    UNEXPECTED (1);
  if (glthread_lock_init (&baton))
    UNEXPECTED (1);
   
  if (glthread_rwlock_rdlock (&lock))
    UNEXPECTED (2);
   
  if (glthread_create (&writer, writer_func, NULL))
    UNEXPECTED (3);
   
  for (;;)
    {
      sleep (1);
    }
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
