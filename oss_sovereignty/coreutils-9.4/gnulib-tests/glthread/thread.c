 

#include <config.h>

 
#include "glthread/thread.h"

#include <stdlib.h>
#include "glthread/lock.h"

 

#if USE_ISOC_THREADS

struct thrd_with_exitvalue
{
  thrd_t volatile tid;
  void * volatile exitvalue;
};

 
static tss_t thrd_with_exitvalue_key;

 
static void
do_init_thrd_with_exitvalue_key (void)
{
  if (tss_create (&thrd_with_exitvalue_key, NULL) != thrd_success)
    abort ();
}

 
static void
init_thrd_with_exitvalue_key (void)
{
  static once_flag once = ONCE_FLAG_INIT;
  call_once (&once, do_init_thrd_with_exitvalue_key);
}

typedef union
        {
          struct thrd_with_exitvalue t;
          struct
          {
            thrd_t tid;  
            void *(*mainfunc) (void *);
            void *arg;
          } a;
        }
        main_arg_t;

static int
thrd_main_func (void *pmarg)
{
   
  main_arg_t *main_arg = (main_arg_t *) pmarg;
  void *(*mainfunc) (void *) = main_arg->a.mainfunc;
  void *arg = main_arg->a.arg;

  if (tss_set (thrd_with_exitvalue_key, &main_arg->t) != thrd_success)
    abort ();

   
  {
    void *exitvalue = mainfunc (arg);
     
    main_arg->t.exitvalue = exitvalue;
    return 0;
  }
}

int
glthread_create (gl_thread_t *threadp, void *(*mainfunc) (void *), void *arg)
{
  init_thrd_with_exitvalue_key ();
  {
     
    main_arg_t *main_arg = (main_arg_t *) malloc (sizeof (main_arg_t));
    if (main_arg == NULL)
      return ENOMEM;
    main_arg->a.mainfunc = mainfunc;
    main_arg->a.arg = arg;
    switch (thrd_create ((thrd_t *) &main_arg->t.tid, thrd_main_func, main_arg))
      {
      case thrd_success:
        break;
      case thrd_nomem:
        free (main_arg);
        return ENOMEM;
      default:
        free (main_arg);
        return EAGAIN;
      }
    *threadp = &main_arg->t;
    return 0;
  }
}

gl_thread_t
gl_thread_self (void)
{
  init_thrd_with_exitvalue_key ();
  {
    gl_thread_t thread =
      (struct thrd_with_exitvalue *) tss_get (thrd_with_exitvalue_key);
    if (thread == NULL)
      {
         
        for (;;)
          {
            thread =
              (struct thrd_with_exitvalue *)
              malloc (sizeof (struct thrd_with_exitvalue));
            if (thread != NULL)
              break;
             
            {
              struct timespec ts =
                {
                  .tv_sec = 1,
                  .tv_nsec = 0
                };
              thrd_sleep (&ts, NULL);
            }
          }
        thread->tid = thrd_current ();
        thread->exitvalue = NULL;  
        if (tss_set (thrd_with_exitvalue_key, thread) != thrd_success)
          abort ();
      }
    return thread;
  }
}

int
glthread_join (gl_thread_t thread, void **return_value_ptr)
{
   
  int dummy;

  if (thread == gl_thread_self ())
    return EINVAL;
  if (thrd_join (thread->tid, &dummy) != thrd_success)
    return EINVAL;
  if (return_value_ptr != NULL)
    *return_value_ptr = thread->exitvalue;
  free (thread);
  return 0;
}

_Noreturn void
gl_thread_exit (void *return_value)
{
  gl_thread_t thread = gl_thread_self ();
  thread->exitvalue = return_value;
  thrd_exit (0);
}

#endif

 

#if USE_POSIX_THREADS || USE_ISOC_AND_POSIX_THREADS

#include <pthread.h>

#if defined PTW32_VERSION || defined __MVS__

const gl_thread_t gl_null_thread  ;

#endif

#endif

 

#if USE_WINDOWS_THREADS

#endif

 

gl_thread_t
gl_thread_create (void *(*func) (void *arg), void *arg)
{
  gl_thread_t thread;
  int ret;

  ret = glthread_create (&thread, func, arg);
  if (ret != 0)
    abort ();
  return thread;
}
