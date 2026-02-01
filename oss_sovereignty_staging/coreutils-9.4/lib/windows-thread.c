 

#include <config.h>

 
#include "windows-thread.h"

#include <errno.h>
#include <process.h>
#include <stdlib.h>

#include "windows-once.h"
#include "windows-tls.h"

 
static DWORD self_key = (DWORD)-1;

 
static void
do_init_self_key (void)
{
  self_key = TlsAlloc ();
   
  if (self_key == (DWORD)-1)
    abort ();
}

 
static void
init_self_key (void)
{
  static glwthread_once_t once = GLWTHREAD_ONCE_INIT;
  glwthread_once (&once, do_init_self_key);
}

 
struct glwthread_thread_struct
{
   
  HANDLE volatile handle;
  CRITICAL_SECTION handle_lock;
   
  BOOL volatile detached;
  void * volatile result;
   
  void * (*func) (void *);
  void *arg;
};

 
static HANDLE
get_current_thread_handle (void)
{
  HANDLE this_handle;

   
  if (!DuplicateHandle (GetCurrentProcess (), GetCurrentThread (),
                        GetCurrentProcess (), &this_handle,
                        0, FALSE, DUPLICATE_SAME_ACCESS))
    abort ();
  return this_handle;
}

glwthread_thread_t
glwthread_thread_self (void)
{
  glwthread_thread_t thread;

  if (self_key == (DWORD)-1)
    init_self_key ();
  thread = TlsGetValue (self_key);
  if (thread == NULL)
    {
       
      for (;;)
        {
          thread =
            (struct glwthread_thread_struct *)
            malloc (sizeof (struct glwthread_thread_struct));
          if (thread != NULL)
            break;
           
          Sleep (1);
        }

      thread->handle = get_current_thread_handle ();
      InitializeCriticalSection (&thread->handle_lock);
      thread->detached = FALSE;  
      thread->result = NULL;  
      TlsSetValue (self_key, thread);
    }
  return thread;
}

 
static unsigned int WINAPI
wrapper_func (void *varg)
{
  struct glwthread_thread_struct *thread =
    (struct glwthread_thread_struct *) varg;

  EnterCriticalSection (&thread->handle_lock);
   
  if (thread->handle == NULL)
    thread->handle = get_current_thread_handle ();
  LeaveCriticalSection (&thread->handle_lock);

  if (self_key == (DWORD)-1)
    init_self_key ();
  TlsSetValue (self_key, thread);

   
  thread->result = thread->func (thread->arg);

   
  glwthread_tls_process_destructors ();

  if (thread->detached)
    {
       
      DeleteCriticalSection (&thread->handle_lock);
      CloseHandle (thread->handle);
      free (thread);
    }

  return 0;
}

int
glwthread_thread_create (glwthread_thread_t *threadp, unsigned int attr,
                         void * (*func) (void *), void *arg)
{
  struct glwthread_thread_struct *thread =
    (struct glwthread_thread_struct *)
    malloc (sizeof (struct glwthread_thread_struct));
  if (thread == NULL)
    return ENOMEM;
  thread->handle = NULL;
  InitializeCriticalSection (&thread->handle_lock);
  thread->detached = (attr & GLWTHREAD_ATTR_DETACHED ? TRUE : FALSE);
  thread->result = NULL;  
  thread->func = func;
  thread->arg = arg;

  {
    unsigned int thread_id;
    HANDLE thread_handle;

    thread_handle = (HANDLE)
      _beginthreadex (NULL, 100000, wrapper_func, thread, 0, &thread_id);
       
    if (thread_handle == NULL)
      {
        DeleteCriticalSection (&thread->handle_lock);
        free (thread);
        return EAGAIN;
      }

    EnterCriticalSection (&thread->handle_lock);
    if (thread->handle == NULL)
      thread->handle = thread_handle;
    else
       
      CloseHandle (thread_handle);
    LeaveCriticalSection (&thread->handle_lock);

    *threadp = thread;
    return 0;
  }
}

int
glwthread_thread_join (glwthread_thread_t thread, void **retvalp)
{
  if (thread == NULL)
    return EINVAL;

  if (thread == glwthread_thread_self ())
    return EDEADLK;

  if (thread->detached)
    return EINVAL;

  if (WaitForSingleObject (thread->handle, INFINITE) == WAIT_FAILED)
    return EINVAL;

  if (retvalp != NULL)
    *retvalp = thread->result;

  DeleteCriticalSection (&thread->handle_lock);
  CloseHandle (thread->handle);
  free (thread);

  return 0;
}

int
glwthread_thread_detach (glwthread_thread_t thread)
{
  if (thread == NULL)
    return EINVAL;

  if (thread->detached)
    return EINVAL;

  thread->detached = TRUE;
  return 0;
}

void
glwthread_thread_exit (void *retval)
{
  glwthread_thread_t thread = glwthread_thread_self ();
  thread->result = retval;
  glwthread_tls_process_destructors ();
  _endthreadex (0);  
  abort ();
}
