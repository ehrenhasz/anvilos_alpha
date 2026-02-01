 

#include <config.h>

 
#include "windows-rwlock.h"

#include <errno.h>
#include <stdlib.h>

 
#undef CreateEvent
#define CreateEvent CreateEventA

 
#define glwthread_waitqueue_t glwthread_carray_waitqueue_t

static void
glwthread_waitqueue_init (glwthread_waitqueue_t *wq)
{
  wq->array = NULL;
  wq->count = 0;
  wq->alloc = 0;
  wq->offset = 0;
}

 
static HANDLE
glwthread_waitqueue_add (glwthread_waitqueue_t *wq)
{
  HANDLE event;
  unsigned int index;

  if (wq->count == wq->alloc)
    {
      unsigned int new_alloc = 2 * wq->alloc + 1;
      HANDLE *new_array =
        (HANDLE *) realloc (wq->array, new_alloc * sizeof (HANDLE));
      if (new_array == NULL)
         
        return INVALID_HANDLE_VALUE;
       
      if (wq->offset > 0)
        {
          unsigned int old_count = wq->count;
          unsigned int old_alloc = wq->alloc;
          unsigned int old_offset = wq->offset;
          unsigned int i;
          if (old_offset + old_count > old_alloc)
            {
              unsigned int limit = old_offset + old_count - old_alloc;
              for (i = 0; i < limit; i++)
                new_array[old_alloc + i] = new_array[i];
            }
          for (i = 0; i < old_count; i++)
            new_array[i] = new_array[old_offset + i];
          wq->offset = 0;
        }
      wq->array = new_array;
      wq->alloc = new_alloc;
    }
   
  event = CreateEvent (NULL, TRUE, FALSE, NULL);
  if (event == INVALID_HANDLE_VALUE)
     
    return INVALID_HANDLE_VALUE;
  index = wq->offset + wq->count;
  if (index >= wq->alloc)
    index -= wq->alloc;
  wq->array[index] = event;
  wq->count++;
  return event;
}

 
static void
glwthread_waitqueue_notify_first (glwthread_waitqueue_t *wq)
{
  SetEvent (wq->array[wq->offset + 0]);
  wq->offset++;
  wq->count--;
  if (wq->count == 0 || wq->offset == wq->alloc)
    wq->offset = 0;
}

 
static void
glwthread_waitqueue_notify_all (glwthread_waitqueue_t *wq)
{
  unsigned int i;

  for (i = 0; i < wq->count; i++)
    {
      unsigned int index = wq->offset + i;
      if (index >= wq->alloc)
        index -= wq->alloc;
      SetEvent (wq->array[index]);
    }
  wq->count = 0;
  wq->offset = 0;
}

void
glwthread_rwlock_init (glwthread_rwlock_t *lock)
{
  InitializeCriticalSection (&lock->lock);
  glwthread_waitqueue_init (&lock->waiting_readers);
  glwthread_waitqueue_init (&lock->waiting_writers);
  lock->runcount = 0;
  lock->guard.done = 1;
}

int
glwthread_rwlock_rdlock (glwthread_rwlock_t *lock)
{
  if (!lock->guard.done)
    {
      if (InterlockedIncrement (&lock->guard.started) == 0)
         
        glwthread_rwlock_init (lock);
      else
        {
           
          InterlockedDecrement (&lock->guard.started);
           
          while (!lock->guard.done)
            Sleep (0);
        }
    }
  EnterCriticalSection (&lock->lock);
   
  if (!(lock->runcount + 1 > 0 && lock->waiting_writers.count == 0))
    {
       
      HANDLE event = glwthread_waitqueue_add (&lock->waiting_readers);
      if (event != INVALID_HANDLE_VALUE)
        {
          DWORD result;
          LeaveCriticalSection (&lock->lock);
           
          result = WaitForSingleObject (event, INFINITE);
          if (result == WAIT_FAILED || result == WAIT_TIMEOUT)
            abort ();
          CloseHandle (event);
           
          if (!(lock->runcount > 0))
            abort ();
          return 0;
        }
      else
        {
           
          do
            {
              LeaveCriticalSection (&lock->lock);
              Sleep (1);
              EnterCriticalSection (&lock->lock);
            }
          while (!(lock->runcount + 1 > 0));
        }
    }
  lock->runcount++;
  LeaveCriticalSection (&lock->lock);
  return 0;
}

int
glwthread_rwlock_wrlock (glwthread_rwlock_t *lock)
{
  if (!lock->guard.done)
    {
      if (InterlockedIncrement (&lock->guard.started) == 0)
         
        glwthread_rwlock_init (lock);
      else
        {
           
          InterlockedDecrement (&lock->guard.started);
           
          while (!lock->guard.done)
            Sleep (0);
        }
    }
  EnterCriticalSection (&lock->lock);
   
  if (!(lock->runcount == 0))
    {
       
      HANDLE event = glwthread_waitqueue_add (&lock->waiting_writers);
      if (event != INVALID_HANDLE_VALUE)
        {
          DWORD result;
          LeaveCriticalSection (&lock->lock);
           
          result = WaitForSingleObject (event, INFINITE);
          if (result == WAIT_FAILED || result == WAIT_TIMEOUT)
            abort ();
          CloseHandle (event);
           
          if (!(lock->runcount == -1))
            abort ();
          return 0;
        }
      else
        {
           
          do
            {
              LeaveCriticalSection (&lock->lock);
              Sleep (1);
              EnterCriticalSection (&lock->lock);
            }
          while (!(lock->runcount == 0));
        }
    }
  lock->runcount--;  
  LeaveCriticalSection (&lock->lock);
  return 0;
}

int
glwthread_rwlock_tryrdlock (glwthread_rwlock_t *lock)
{
  if (!lock->guard.done)
    {
      if (InterlockedIncrement (&lock->guard.started) == 0)
         
        glwthread_rwlock_init (lock);
      else
        {
           
          InterlockedDecrement (&lock->guard.started);
           
          while (!lock->guard.done)
            Sleep (0);
        }
    }
   
  EnterCriticalSection (&lock->lock);
   
  if (!(lock->runcount + 1 > 0 && lock->waiting_writers.count == 0))
    {
       
      LeaveCriticalSection (&lock->lock);
      return EBUSY;
    }
  lock->runcount++;
  LeaveCriticalSection (&lock->lock);
  return 0;
}

int
glwthread_rwlock_trywrlock (glwthread_rwlock_t *lock)
{
  if (!lock->guard.done)
    {
      if (InterlockedIncrement (&lock->guard.started) == 0)
         
        glwthread_rwlock_init (lock);
      else
        {
           
          InterlockedDecrement (&lock->guard.started);
           
          while (!lock->guard.done)
            Sleep (0);
        }
    }
   
  EnterCriticalSection (&lock->lock);
   
  if (!(lock->runcount == 0))
    {
       
      LeaveCriticalSection (&lock->lock);
      return EBUSY;
    }
  lock->runcount--;  
  LeaveCriticalSection (&lock->lock);
  return 0;
}

int
glwthread_rwlock_unlock (glwthread_rwlock_t *lock)
{
  if (!lock->guard.done)
    return EINVAL;
  EnterCriticalSection (&lock->lock);
  if (lock->runcount < 0)
    {
       
      if (!(lock->runcount == -1))
        abort ();
      lock->runcount = 0;
    }
  else
    {
       
      if (!(lock->runcount > 0))
        {
          LeaveCriticalSection (&lock->lock);
          return EPERM;
        }
      lock->runcount--;
    }
  if (lock->runcount == 0)
    {
       
      if (lock->waiting_writers.count > 0)
        {
           
          lock->runcount--;
          glwthread_waitqueue_notify_first (&lock->waiting_writers);
        }
      else
        {
           
          lock->runcount += lock->waiting_readers.count;
          glwthread_waitqueue_notify_all (&lock->waiting_readers);
        }
    }
  LeaveCriticalSection (&lock->lock);
  return 0;
}

int
glwthread_rwlock_destroy (glwthread_rwlock_t *lock)
{
  if (!lock->guard.done)
    return EINVAL;
  if (lock->runcount != 0)
    return EBUSY;
  DeleteCriticalSection (&lock->lock);
  if (lock->waiting_readers.array != NULL)
    free (lock->waiting_readers.array);
  if (lock->waiting_writers.array != NULL)
    free (lock->waiting_writers.array);
  lock->guard.done = 0;
  return 0;
}
