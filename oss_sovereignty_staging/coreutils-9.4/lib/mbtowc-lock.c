 

#include <config.h>

 
#if OMIT_MBTOWC_LOCK

 
typedef int dummy;

#else

 

 
# undef gl_get_mbtowc_lock

 
# ifndef DLL_EXPORTED
#  if HAVE_VISIBILITY
   
#   define DLL_EXPORTED __attribute__((__visibility__("default")))
#  elif defined _WIN32 || defined __CYGWIN__
#   define DLL_EXPORTED __declspec(dllexport)
#  else
#   define DLL_EXPORTED
#  endif
# endif

# if defined _WIN32 && !defined __CYGWIN__

#  define WIN32_LEAN_AND_MEAN   
#  include <windows.h>

#  include "windows-initguard.h"

 

 
DLL_EXPORTED CRITICAL_SECTION *gl_get_mbtowc_lock (void);

static glwthread_initguard_t guard = GLWTHREAD_INITGUARD_INIT;
static CRITICAL_SECTION lock;

 
CRITICAL_SECTION *
gl_get_mbtowc_lock (void)
{
  if (!guard.done)
    {
      if (InterlockedIncrement (&guard.started) == 0)
        {
           
          InitializeCriticalSection (&lock);
          guard.done = 1;
        }
      else
        {
           
          InterlockedDecrement (&guard.started);
           
          while (!guard.done)
            Sleep (0);
        }
    }
  return &lock;
}

# elif HAVE_PTHREAD_API

#  include <pthread.h>

static pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;

 
DLL_EXPORTED pthread_mutex_t *gl_get_mbtowc_lock (void);

 
pthread_mutex_t *
gl_get_mbtowc_lock (void)
{
  return &mutex;
}

# elif HAVE_THREADS_H

#  include <threads.h>
#  include <stdlib.h>

static int volatile init_needed = 1;
static once_flag init_once = ONCE_FLAG_INIT;
static mtx_t mutex;

static void
atomic_init (void)
{
  if (mtx_init (&mutex, mtx_plain) != thrd_success)
    abort ();
  init_needed = 0;
}

 
DLL_EXPORTED mtx_t *gl_get_mbtowc_lock (void);

 
mtx_t *
gl_get_mbtowc_lock (void)
{
  if (init_needed)
    call_once (&init_once, atomic_init);
  return &mutex;
}

# endif

# if (defined _WIN32 || defined __CYGWIN__) && !defined _MSC_VER
 
#  if defined _WIN64 || defined _LP64
#   define IMP(x) __imp_##x
#  else
#   define IMP(x) _imp__##x
#  endif
void * IMP(gl_get_mbtowc_lock) = &gl_get_mbtowc_lock;
# endif

#endif
