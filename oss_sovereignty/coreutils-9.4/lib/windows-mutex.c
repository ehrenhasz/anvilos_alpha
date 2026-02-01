 

#include <config.h>

 
#include "windows-mutex.h"

#include <errno.h>

void
glwthread_mutex_init (glwthread_mutex_t *mutex)
{
  InitializeCriticalSection (&mutex->lock);
  mutex->guard.done = 1;
}

int
glwthread_mutex_lock (glwthread_mutex_t *mutex)
{
  if (!mutex->guard.done)
    {
      if (InterlockedIncrement (&mutex->guard.started) == 0)
         
        glwthread_mutex_init (mutex);
      else
        {
           
          InterlockedDecrement (&mutex->guard.started);
           
          while (!mutex->guard.done)
            Sleep (0);
        }
    }
  EnterCriticalSection (&mutex->lock);
  return 0;
}

int
glwthread_mutex_trylock (glwthread_mutex_t *mutex)
{
  if (!mutex->guard.done)
    {
      if (InterlockedIncrement (&mutex->guard.started) == 0)
         
        glwthread_mutex_init (mutex);
      else
        {
           
          InterlockedDecrement (&mutex->guard.started);
           
          return EBUSY;
        }
    }
  if (!TryEnterCriticalSection (&mutex->lock))
    return EBUSY;
  return 0;
}

int
glwthread_mutex_unlock (glwthread_mutex_t *mutex)
{
  if (!mutex->guard.done)
    return EINVAL;
  LeaveCriticalSection (&mutex->lock);
  return 0;
}

int
glwthread_mutex_destroy (glwthread_mutex_t *mutex)
{
  if (!mutex->guard.done)
    return EINVAL;
  DeleteCriticalSection (&mutex->lock);
  mutex->guard.done = 0;
  return 0;
}
