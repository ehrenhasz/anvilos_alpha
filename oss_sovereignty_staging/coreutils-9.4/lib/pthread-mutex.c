 

#include <config.h>

 
#include <pthread.h>

#if (defined _WIN32 && ! defined __CYGWIN__) && USE_WINDOWS_THREADS
# include "windows-timedmutex.h"
# include "windows-timedrecmutex.h"
#else
# include <stdlib.h>
#endif

#if ((defined _WIN32 && ! defined __CYGWIN__) && USE_WINDOWS_THREADS) || !HAVE_PTHREAD_H

int
pthread_mutexattr_init (pthread_mutexattr_t *attr)
{
  *attr = (PTHREAD_MUTEX_STALLED << 2) | PTHREAD_MUTEX_DEFAULT;
  return 0;
}

int
pthread_mutexattr_gettype (const pthread_mutexattr_t *attr, int *typep)
{
  *typep = *attr & (PTHREAD_MUTEX_DEFAULT | PTHREAD_MUTEX_NORMAL
                    | PTHREAD_MUTEX_ERRORCHECK | PTHREAD_MUTEX_RECURSIVE);
  return 0;
}

int
pthread_mutexattr_settype (pthread_mutexattr_t *attr, int type)
{
  if (!(type == PTHREAD_MUTEX_DEFAULT
        || type == PTHREAD_MUTEX_NORMAL
        || type == PTHREAD_MUTEX_ERRORCHECK
        || type == PTHREAD_MUTEX_RECURSIVE))
    return EINVAL;
  *attr ^= (*attr ^ type)
           & (PTHREAD_MUTEX_DEFAULT | PTHREAD_MUTEX_NORMAL
              | PTHREAD_MUTEX_ERRORCHECK | PTHREAD_MUTEX_RECURSIVE);
  return 0;
}

int
pthread_mutexattr_getrobust (const pthread_mutexattr_t *attr, int *robustp)
{
  *robustp = (*attr >> 2) & (PTHREAD_MUTEX_STALLED | PTHREAD_MUTEX_ROBUST);
  return 0;
}

int
pthread_mutexattr_setrobust (pthread_mutexattr_t *attr, int robust)
{
  if (!(robust == PTHREAD_MUTEX_STALLED || robust == PTHREAD_MUTEX_ROBUST))
    return EINVAL;
  *attr ^= (*attr ^ (robust << 2))
           & ((PTHREAD_MUTEX_STALLED | PTHREAD_MUTEX_ROBUST) << 2);
  return 0;
}

int
pthread_mutexattr_destroy (_GL_UNUSED pthread_mutexattr_t *attr)
{
  return 0;
}

#elif PTHREAD_MUTEXATTR_ROBUST_UNIMPLEMENTED

int
pthread_mutexattr_getrobust (const pthread_mutexattr_t *attr, int *robustp)
{
  *robustp = PTHREAD_MUTEX_STALLED;
  return 0;
}

int
pthread_mutexattr_setrobust (pthread_mutexattr_t *attr, int robust)
{
  if (!(robust == PTHREAD_MUTEX_STALLED || robust == PTHREAD_MUTEX_ROBUST))
    return EINVAL;
  if (!(robust == PTHREAD_MUTEX_STALLED))
    return ENOTSUP;
  return 0;
}

#endif

#if (defined _WIN32 && ! defined __CYGWIN__) && USE_WINDOWS_THREADS
 

int
pthread_mutex_init (pthread_mutex_t *mutex, const pthread_mutexattr_t *attr)
{
   
  if (attr != NULL
      && (*attr & (PTHREAD_MUTEX_DEFAULT | PTHREAD_MUTEX_NORMAL
                   | PTHREAD_MUTEX_ERRORCHECK | PTHREAD_MUTEX_RECURSIVE))
         == PTHREAD_MUTEX_RECURSIVE)
    {
      mutex->type = 2;
      return glwthread_timedrecmutex_init (&mutex->u.u_timedrecmutex);
    }
  else
    {
      mutex->type = 1;
      return glwthread_timedmutex_init (&mutex->u.u_timedmutex);
    }
}

int
pthread_mutex_lock (pthread_mutex_t *mutex)
{
  switch (mutex->type)
    {
    case 1:
      return glwthread_timedmutex_lock (&mutex->u.u_timedmutex);
    case 2:
      return glwthread_timedrecmutex_lock (&mutex->u.u_timedrecmutex);
    default:
      abort ();
    }
}

int
pthread_mutex_trylock (pthread_mutex_t *mutex)
{
  switch (mutex->type)
    {
    case 1:
      return glwthread_timedmutex_trylock (&mutex->u.u_timedmutex);
    case 2:
      return glwthread_timedrecmutex_trylock (&mutex->u.u_timedrecmutex);
    default:
      abort ();
    }
}

int
pthread_mutex_timedlock (pthread_mutex_t *mutex, const struct timespec *abstime)
{
  switch (mutex->type)
    {
    case 1:
      return glwthread_timedmutex_timedlock (&mutex->u.u_timedmutex, abstime);
    case 2:
      return glwthread_timedrecmutex_timedlock (&mutex->u.u_timedrecmutex,
                                                abstime);
    default:
      abort ();
    }
}

int
pthread_mutex_unlock (pthread_mutex_t *mutex)
{
  switch (mutex->type)
    {
    case 1:
      return glwthread_timedmutex_unlock (&mutex->u.u_timedmutex);
    case 2:
      return glwthread_timedrecmutex_unlock (&mutex->u.u_timedrecmutex);
    default:
      abort ();
    }
}

int
pthread_mutex_destroy (pthread_mutex_t *mutex)
{
  switch (mutex->type)
    {
    case 1:
      return glwthread_timedmutex_destroy (&mutex->u.u_timedmutex);
    case 2:
      return glwthread_timedrecmutex_destroy (&mutex->u.u_timedrecmutex);
    default:
      abort ();
    }
}

#elif HAVE_PTHREAD_H
 

 

#else
 

int
pthread_mutex_init (_GL_UNUSED pthread_mutex_t *mutex,
                    _GL_UNUSED const pthread_mutexattr_t *attr)
{
   
  return 0;
}

int
pthread_mutex_lock (_GL_UNUSED pthread_mutex_t *mutex)
{
   
  return 0;
}

int
pthread_mutex_trylock (_GL_UNUSED pthread_mutex_t *mutex)
{
   
  return 0;
}

int
pthread_mutex_timedlock (_GL_UNUSED pthread_mutex_t *mutex,
                         _GL_UNUSED const struct timespec *abstime)
{
   
  return 0;
}

int
pthread_mutex_unlock (_GL_UNUSED pthread_mutex_t *mutex)
{
   
  return 0;
}

int
pthread_mutex_destroy (_GL_UNUSED pthread_mutex_t *mutex)
{
   
  return 0;
}

#endif
