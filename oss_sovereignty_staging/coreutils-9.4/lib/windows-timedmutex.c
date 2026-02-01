 

#include <config.h>

 
#include "windows-timedmutex.h"

#include <errno.h>
#include <stdlib.h>
#include <sys/time.h>

 
#undef CreateEvent
#define CreateEvent CreateEventA

int
glwthread_timedmutex_init (glwthread_timedmutex_t *mutex)
{
   
   
          int err = glwthread_timedmutex_init (mutex);
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
  EnterCriticalSection (&mutex->lock);
  return 0;
}

int
glwthread_timedmutex_trylock (glwthread_timedmutex_t *mutex)
{
  if (!mutex->guard.done)
    {
      if (InterlockedIncrement (&mutex->guard.started) == 0)
        {
           
          int err = glwthread_timedmutex_init (mutex);
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
  if (!TryEnterCriticalSection (&mutex->lock))
    return EBUSY;
  return 0;
}

int
glwthread_timedmutex_timedlock (glwthread_timedmutex_t *mutex,
                                const struct timespec *abstime)
{
  if (!mutex->guard.done)
    {
      if (InterlockedIncrement (&mutex->guard.started) == 0)
        {
           
          int err = glwthread_timedmutex_init (mutex);
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
  return 0;
}

int
glwthread_timedmutex_unlock (glwthread_timedmutex_t *mutex)
{
  if (!mutex->guard.done)
    return EINVAL;
  LeaveCriticalSection (&mutex->lock);
   
  /* SetEvent
     <https:
  SetEvent (mutex->event);
  return 0;
}

int
glwthread_timedmutex_destroy (glwthread_timedmutex_t *mutex)
{
  if (!mutex->guard.done)
    return EINVAL;
  DeleteCriticalSection (&mutex->lock);
  /* CloseHandle
     <https:
  CloseHandle (mutex->event);
  mutex->guard.done = 0;
  return 0;
}
