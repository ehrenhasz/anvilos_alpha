 

#include <config.h>

 
#include "windows-recmutex.h"

#include <errno.h>

void
glwthread_recmutex_init (glwthread_recmutex_t *mutex)
{
  mutex->owner = 0;
  mutex->depth = 0;
  InitializeCriticalSection (&mutex->lock);
  mutex->guard.done = 1;
}

int
glwthread_recmutex_lock (glwthread_recmutex_t *mutex)
{
  if (!mutex->guard.done)
    {
      if (InterlockedIncrement (&mutex->guard.started) == 0)
         
        glwthread_recmutex_init (mutex);
      else
        {
           
          InterlockedDecrement (&mutex->guard.started);
           
          while (!mutex->guard.done)
            Sleep (0);
        }
    }
  {
    DWORD self = GetCurrentThreadId ();
    if (mutex->owner != self)
      {
        EnterCriticalSection (&mutex->lock);
        mutex->owner = self;
      }
    if (++(mutex->depth) == 0)  
      {
        mutex->depth--;
        return EAGAIN;
      }
  }
  return 0;
}

int
glwthread_recmutex_trylock (glwthread_recmutex_t *mutex)
{
  if (!mutex->guard.done)
    {
      if (InterlockedIncrement (&mutex->guard.started) == 0)
         
        glwthread_recmutex_init (mutex);
      else
        {
           
          InterlockedDecrement (&mutex->guard.started);
           
          return EBUSY;
        }
    }
  {
    DWORD self = GetCurrentThreadId ();
    if (mutex->owner != self)
      {
        if (!TryEnterCriticalSection (&mutex->lock))
          return EBUSY;
        mutex->owner = self;
      }
    if (++(mutex->depth) == 0)  
      {
        mutex->depth--;
        return EAGAIN;
      }
  }
  return 0;
}

int
glwthread_recmutex_unlock (glwthread_recmutex_t *mutex)
{
  if (mutex->owner != GetCurrentThreadId ())
    return EPERM;
  if (mutex->depth == 0)
    return EINVAL;
  if (--(mutex->depth) == 0)
    {
      mutex->owner = 0;
      LeaveCriticalSection (&mutex->lock);
    }
  return 0;
}

int
glwthread_recmutex_destroy (glwthread_recmutex_t *mutex)
{
  if (mutex->owner != 0)
    return EBUSY;
  DeleteCriticalSection (&mutex->lock);
  mutex->guard.done = 0;
  return 0;
}
