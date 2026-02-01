 

#include <config.h>

 
#include "windows-timedrecmutex.h"

#include <errno.h>
#include <stdlib.h>
#include <sys/time.h>

 
#undef CreateEvent
#define CreateEvent CreateEventA

int
glwthread_timedrecmutex_init (glwthread_timedrecmutex_t *mutex)
{
  mutex->owner = 0;
  mutex->depth = 0;
   
   
          int err = glwthread_timedrecmutex_init (mutex);
          if (err != 0)
            {
               
              InterlockedDecrement (&mutex->guard.started);
              return err;
            }
        }
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
glwthread_timedrecmutex_trylock (glwthread_timedrecmutex_t *mutex)
{
  if (!mutex->guard.done)
    {
      if (InterlockedIncrement (&mutex->guard.started) == 0)
        {
           
          int err = glwthread_timedrecmutex_init (mutex);
          if (err != 0)
            {
               
              InterlockedDecrement (&mutex->guard.started);
              return err;
            }
        }
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
glwthread_timedrecmutex_timedlock (glwthread_timedrecmutex_t *mutex,
                                   const struct timespec *abstime)
{
  if (!mutex->guard.done)
    {
      if (InterlockedIncrement (&mutex->guard.started) == 0)
        {
           
          int err = glwthread_timedrecmutex_init (mutex);
          if (err != 0)
            {
               
              InterlockedDecrement (&mutex->guard.started);
              return err;
            }
        }
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
         
        for (;;)
          {
            if (TryEnterCriticalSection (&mutex->lock))
              break;

            {
              struct timeval currtime;
              DWORD timeout;
              DWORD result;

              gettimeofday (&currtime, NULL);

               
              if (currtime.tv_sec > abstime->tv_sec)
                timeout = 0;
              else
                {
                  unsigned long seconds = abstime->tv_sec - currtime.tv_sec;
                  timeout = seconds * 1000;
                  if (timeout / 1000 != seconds)  
                    timeout = INFINITE;
                  else
                    {
                      long milliseconds =
                        abstime->tv_nsec / 1000000 - currtime.tv_usec / 1000;
                      if (milliseconds >= 0)
                        {
                          timeout += milliseconds;
                          if (timeout < milliseconds)  
                            timeout = INFINITE;
                        }
                      else
                        {
                          if (timeout >= - milliseconds)
                            timeout -= (- milliseconds);
                          else
                            timeout = 0;
                        }
                    }
                }
              if (timeout == 0)
                return ETIMEDOUT;

               
            }
          }
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
glwthread_timedrecmutex_unlock (glwthread_timedrecmutex_t *mutex)
{
  if (mutex->owner != GetCurrentThreadId ())
    return EPERM;
  if (mutex->depth == 0)
    return EINVAL;
  if (--(mutex->depth) == 0)
    {
      mutex->owner = 0;
      LeaveCriticalSection (&mutex->lock);
       
      /* SetEvent
         <https:
      SetEvent (mutex->event);
    }
  return 0;
}

int
glwthread_timedrecmutex_destroy (glwthread_timedrecmutex_t *mutex)
{
  if (mutex->owner != 0)
    return EBUSY;
  DeleteCriticalSection (&mutex->lock);
  /* CloseHandle
     <https:
  CloseHandle (mutex->event);
  mutex->guard.done = 0;
  return 0;
}
