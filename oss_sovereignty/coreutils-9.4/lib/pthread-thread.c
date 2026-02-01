 

#include <config.h>

 
#include <pthread.h>

#if (defined _WIN32 && ! defined __CYGWIN__) && USE_WINDOWS_THREADS
# include "windows-thread.h"
#else
# include <stdlib.h>
#endif

typedef void * (* pthread_main_function_t) (void *);

#if ((defined _WIN32 && ! defined __CYGWIN__) && USE_WINDOWS_THREADS) || !HAVE_PTHREAD_H

int
pthread_attr_init (pthread_attr_t *attr)
{
  *attr = PTHREAD_CREATE_JOINABLE;
  return 0;
}

int
pthread_attr_getdetachstate (const pthread_attr_t *attr, int *detachstatep)
{
  *detachstatep = *attr & (PTHREAD_CREATE_JOINABLE | PTHREAD_CREATE_DETACHED);
  return 0;
}

int
pthread_attr_setdetachstate (pthread_attr_t *attr, int detachstate)
{
  if (!(detachstate == PTHREAD_CREATE_JOINABLE
        || detachstate == PTHREAD_CREATE_DETACHED))
    return EINVAL;
  *attr ^= (*attr ^ detachstate)
           & (PTHREAD_CREATE_JOINABLE | PTHREAD_CREATE_DETACHED);
  return 0;
}

int
pthread_attr_destroy (_GL_UNUSED pthread_attr_t *attr)
{
  return 0;
}

#endif

#if (defined _WIN32 && ! defined __CYGWIN__) && USE_WINDOWS_THREADS
 

int
pthread_create (pthread_t *threadp, const pthread_attr_t *attr,
                pthread_main_function_t mainfunc, void *arg)
{
  unsigned int glwthread_attr =
    (attr != NULL
     && (*attr & (PTHREAD_CREATE_JOINABLE | PTHREAD_CREATE_DETACHED))
        != PTHREAD_CREATE_JOINABLE
     ? GLWTHREAD_ATTR_DETACHED
     : 0);
  return glwthread_thread_create (threadp, glwthread_attr, mainfunc, arg);
}

pthread_t
pthread_self (void)
{
  return glwthread_thread_self ();
}

int
pthread_equal (pthread_t thread1, pthread_t thread2)
{
  return thread1 == thread2;
}

int
pthread_detach (pthread_t thread)
{
  return glwthread_thread_detach (thread);
}

int
pthread_join (pthread_t thread, void **valuep)
{
  return glwthread_thread_join (thread, valuep);
}

void
pthread_exit (void *value)
{
  glwthread_thread_exit (value);
}

#elif HAVE_PTHREAD_H
 

# if PTHREAD_CREATE_IS_INLINE
int
pthread_create (pthread_t *threadp, const pthread_attr_t *attr,
                pthread_main_function_t mainfunc, void *arg)
#  undef pthread_create
{
  return pthread_create (threadp, attr, mainfunc, arg);
}

int
pthread_attr_init (pthread_attr_t *attr)
#  undef pthread_attr_init
{
  return pthread_attr_init (attr);
}

# endif

#else
 

int
pthread_create (pthread_t *threadp, const pthread_attr_t *attr,
                pthread_main_function_t mainfunc, void *arg)
{
   
  return EAGAIN;
}

pthread_t
pthread_self (void)
{
  return 42;
}

int
pthread_equal (pthread_t thread1, pthread_t thread2)
{
  return thread1 == thread2;
}

int
pthread_detach (pthread_t thread)
{
   
  return EINVAL;
}

int
pthread_join (pthread_t thread, void **valuep)
{
   
  return EINVAL;
}

void
pthread_exit (void *value)
{
   
  exit (0);
}

#endif
