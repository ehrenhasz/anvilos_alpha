 

#include <config.h>

 
#include "windows-cond.h"

#include <errno.h>
#include <stdlib.h>
#include <sys/time.h>

 
#undef CreateEvent
#define CreateEvent CreateEventA

 
#define glwthread_waitqueue_t glwthread_linked_waitqueue_t

 
struct glwthread_waitqueue_element
{
  struct glwthread_waitqueue_link link;  
  HANDLE event;  
};

static void
glwthread_waitqueue_init (glwthread_waitqueue_t *wq)
{
  wq->wq_list.wql_next = &wq->wq_list;
  wq->wq_list.wql_prev = &wq->wq_list;
}

 
static struct glwthread_waitqueue_element *
glwthread_waitqueue_add (glwthread_waitqueue_t *wq)
{
  struct glwthread_waitqueue_element *elt;
  HANDLE event;

   
  elt =
    (struct glwthread_waitqueue_element *)
    malloc (sizeof (struct glwthread_waitqueue_element));
  if (elt == NULL)
     
    return NULL;

   
  event = CreateEvent (NULL, TRUE, FALSE, NULL);
  if (event == INVALID_HANDLE_VALUE)
    {
       
      free (elt);
      return NULL;
    }
  elt->event = event;
   
  (elt->link.wql_prev = wq->wq_list.wql_prev)->wql_next = &elt->link;
  (elt->link.wql_next = &wq->wq_list)->wql_prev = &elt->link;
  return elt;
}

 
static bool
glwthread_waitqueue_remove (glwthread_waitqueue_t *wq,
                            struct glwthread_waitqueue_element *elt)
{
  if (elt->link.wql_next != NULL && elt->link.wql_prev != NULL)
    {
       
      struct glwthread_waitqueue_link *prev = elt->link.wql_prev;
      struct glwthread_waitqueue_link *next = elt->link.wql_next;
      prev->wql_next = next;
      next->wql_prev = prev;
      elt->link.wql_next = NULL;
      elt->link.wql_prev = NULL;
      return true;
    }
  else
    return false;
}

 
static void
glwthread_waitqueue_notify_first (glwthread_waitqueue_t *wq)
{
  if (wq->wq_list.wql_next != &wq->wq_list)
    {
      struct glwthread_waitqueue_element *elt =
        (struct glwthread_waitqueue_element *) wq->wq_list.wql_next;
      struct glwthread_waitqueue_link *prev;
      struct glwthread_waitqueue_link *next;

       
      prev = &wq->wq_list;  
      next = elt->link.wql_next;
      prev->wql_next = next;
      next->wql_prev = prev;
      elt->link.wql_next = NULL;
      elt->link.wql_prev = NULL;

      SetEvent (elt->event);
       
    }
}

 
static void
glwthread_waitqueue_notify_all (glwthread_waitqueue_t *wq)
{
  struct glwthread_waitqueue_link *l;

  for (l = wq->wq_list.wql_next; l != &wq->wq_list; )
    {
      struct glwthread_waitqueue_element *elt =
        (struct glwthread_waitqueue_element *) l;
      struct glwthread_waitqueue_link *prev;
      struct glwthread_waitqueue_link *next;

       
      prev = &wq->wq_list;  
      next = elt->link.wql_next;
      prev->wql_next = next;
      next->wql_prev = prev;
      elt->link.wql_next = NULL;
      elt->link.wql_prev = NULL;

      SetEvent (elt->event);
       

      l = next;
    }
  if (!(wq->wq_list.wql_next == &wq->wq_list
        && wq->wq_list.wql_prev == &wq->wq_list))
    abort ();
}

int
glwthread_cond_init (glwthread_cond_t *cond)
{
  InitializeCriticalSection (&cond->lock);
  glwthread_waitqueue_init (&cond->waiters);

  cond->guard.done = 1;
  return 0;
}

int
glwthread_cond_wait (glwthread_cond_t *cond,
                     void *mutex, int (*mutex_lock) (void *), int (*mutex_unlock) (void *))
{
  if (!cond->guard.done)
    {
      if (InterlockedIncrement (&cond->guard.started) == 0)
         
        glwthread_cond_init (cond);
      else
        {
           
          InterlockedDecrement (&cond->guard.started);
           
          while (!cond->guard.done)
            Sleep (0);
        }
    }

  EnterCriticalSection (&cond->lock);
  {
    struct glwthread_waitqueue_element *elt =
      glwthread_waitqueue_add (&cond->waiters);
    LeaveCriticalSection (&cond->lock);
    if (elt == NULL)
      {
         
        return EAGAIN;
      }
    else
      {
        HANDLE event = elt->event;
        int err;
        DWORD result;

         
        err = mutex_unlock (mutex);
        if (err != 0)
          {
            EnterCriticalSection (&cond->lock);
            glwthread_waitqueue_remove (&cond->waiters, elt);
            LeaveCriticalSection (&cond->lock);
            CloseHandle (event);
            free (elt);
            return err;
          }
         
         
        result = WaitForSingleObject (event, INFINITE);
        if (result == WAIT_FAILED || result == WAIT_TIMEOUT)
          abort ();
        CloseHandle (event);
        free (elt);
         
        return mutex_lock (mutex);
      }
  }
}

int
glwthread_cond_timedwait (glwthread_cond_t *cond,
                          void *mutex, int (*mutex_lock) (void *), int (*mutex_unlock) (void *),
                          const struct timespec *abstime)
{
  if (!cond->guard.done)
    {
      if (InterlockedIncrement (&cond->guard.started) == 0)
         
        glwthread_cond_init (cond);
      else
        {
           
          InterlockedDecrement (&cond->guard.started);
           
          while (!cond->guard.done)
            Sleep (0);
        }
    }

  {
    struct timeval currtime;

    gettimeofday (&currtime, NULL);
    if (currtime.tv_sec > abstime->tv_sec
        || (currtime.tv_sec == abstime->tv_sec
            && currtime.tv_usec * 1000 >= abstime->tv_nsec))
      return ETIMEDOUT;

    EnterCriticalSection (&cond->lock);
    {
      struct glwthread_waitqueue_element *elt =
        glwthread_waitqueue_add (&cond->waiters);
      LeaveCriticalSection (&cond->lock);
      if (elt == NULL)
        {
           
          return EAGAIN;
        }
      else
        {
          HANDLE event = elt->event;
          int err;
          DWORD timeout;
          DWORD result;

           
          err = mutex_unlock (mutex);
          if (err != 0)
            {
              EnterCriticalSection (&cond->lock);
              glwthread_waitqueue_remove (&cond->waiters, elt);
              LeaveCriticalSection (&cond->lock);
              CloseHandle (event);
              free (elt);
              return err;
            }
           
           
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
          result = WaitForSingleObject (event, timeout);
          if (result == WAIT_FAILED)
            abort ();
          if (result == WAIT_TIMEOUT)
            {
              EnterCriticalSection (&cond->lock);
              if (glwthread_waitqueue_remove (&cond->waiters, elt))
                {
                   
                  if (!(WaitForSingleObject (event, 0) == WAIT_TIMEOUT))
                    abort ();
                }
              else
                {
                   
                  if (!(WaitForSingleObject (event, 0) == WAIT_OBJECT_0))
                    abort ();
                   
                  result = WAIT_OBJECT_0;
                }
              LeaveCriticalSection (&cond->lock);
            }
          else
            {
               
            }
          CloseHandle (event);
          free (elt);
           
          err = mutex_lock (mutex);
          return (err ? err :
                  result == WAIT_OBJECT_0 ? 0 :
                  result == WAIT_TIMEOUT ? ETIMEDOUT :
                    EAGAIN);
        }
    }
  }
}

int
glwthread_cond_signal (glwthread_cond_t *cond)
{
  if (!cond->guard.done)
    return EINVAL;

  EnterCriticalSection (&cond->lock);
   
  if (cond->waiters.wq_list.wql_next != &cond->waiters.wq_list)
    glwthread_waitqueue_notify_first (&cond->waiters);
  LeaveCriticalSection (&cond->lock);

  return 0;
}

int
glwthread_cond_broadcast (glwthread_cond_t *cond)
{
  if (!cond->guard.done)
    return EINVAL;

  EnterCriticalSection (&cond->lock);
   
  glwthread_waitqueue_notify_all (&cond->waiters);
  LeaveCriticalSection (&cond->lock);

  return 0;
}

int
glwthread_cond_destroy (glwthread_cond_t *cond)
{
  if (!cond->guard.done)
    return EINVAL;
  if (cond->waiters.wq_list.wql_next != &cond->waiters.wq_list)
    return EBUSY;
  DeleteCriticalSection (&cond->lock);
  cond->guard.done = 0;
  return 0;
}
