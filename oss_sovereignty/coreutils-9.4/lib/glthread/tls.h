 

 


#ifndef _TLS_H
#define _TLS_H

 
#if !_GL_CONFIG_H_INCLUDED
 #error "Please include config.h first."
#endif

#include <errno.h>
#include <stdlib.h>

#if !defined c11_threads_in_use
# if HAVE_THREADS_H && USE_POSIX_THREADS_FROM_LIBC
#  define c11_threads_in_use() 1
# elif HAVE_THREADS_H && USE_POSIX_THREADS_WEAK
#  include <threads.h>
#  pragma weak thrd_exit
#  define c11_threads_in_use() (thrd_exit != NULL)
# else
#  define c11_threads_in_use() 0
# endif
#endif

 

#if USE_ISOC_THREADS || USE_ISOC_AND_POSIX_THREADS

 

# include <threads.h>

 

typedef tss_t gl_tls_key_t;
# define glthread_tls_key_init(KEY, DESTRUCTOR) \
    (tss_create (KEY, DESTRUCTOR) != thrd_success ? EAGAIN : 0)
# define gl_tls_get(NAME) \
    tss_get (NAME)
# define glthread_tls_set(KEY, POINTER) \
    (tss_set (*(KEY), (POINTER)) != thrd_success ? ENOMEM : 0)
# define glthread_tls_key_destroy(KEY) \
    (tss_delete (*(KEY)), 0)

#endif

 

#if USE_POSIX_THREADS

 

# include <pthread.h>

# if PTHREAD_IN_USE_DETECTION_HARD

 
#  define pthread_in_use() \
     glthread_in_use ()
extern int glthread_in_use (void);

# endif

# if USE_POSIX_THREADS_WEAK

 

#  pragma weak pthread_key_create
#  pragma weak pthread_getspecific
#  pragma weak pthread_setspecific
#  pragma weak pthread_key_delete
#  ifndef pthread_self
#   pragma weak pthread_self
#  endif

#  if !PTHREAD_IN_USE_DETECTION_HARD
#   pragma weak pthread_mutexattr_gettype
#   define pthread_in_use() \
      (pthread_mutexattr_gettype != NULL || c11_threads_in_use ())
#  endif

# else

#  if !PTHREAD_IN_USE_DETECTION_HARD
#   define pthread_in_use() 1
#  endif

# endif

 

typedef union
        {
          void *singlethread_value;
          pthread_key_t key;
        }
        gl_tls_key_t;
# define glthread_tls_key_init(KEY, DESTRUCTOR) \
    (pthread_in_use ()                              \
     ? pthread_key_create (&(KEY)->key, DESTRUCTOR) \
     : ((KEY)->singlethread_value = NULL, 0))
# define gl_tls_get(NAME) \
    (pthread_in_use ()                  \
     ? pthread_getspecific ((NAME).key) \
     : (NAME).singlethread_value)
# define glthread_tls_set(KEY, POINTER) \
    (pthread_in_use ()                             \
     ? pthread_setspecific ((KEY)->key, (POINTER)) \
     : ((KEY)->singlethread_value = (POINTER), 0))
# define glthread_tls_key_destroy(KEY) \
    (pthread_in_use () ? pthread_key_delete ((KEY)->key) : 0)

#endif

 

#if USE_WINDOWS_THREADS

# define WIN32_LEAN_AND_MEAN   
# include <windows.h>

# include "windows-tls.h"

 

typedef glwthread_tls_key_t gl_tls_key_t;
# define glthread_tls_key_init(KEY, DESTRUCTOR) \
    glwthread_tls_key_create (KEY, DESTRUCTOR)
# define gl_tls_get(NAME) \
    TlsGetValue (NAME)
# define glthread_tls_set(KEY, POINTER) \
    (!TlsSetValue (*(KEY), POINTER) ? EINVAL : 0)
# define glthread_tls_key_destroy(KEY) \
    glwthread_tls_key_delete (*(KEY))

#endif

 

#if !(USE_ISOC_THREADS || USE_POSIX_THREADS || USE_ISOC_AND_POSIX_THREADS || USE_WINDOWS_THREADS)

 

 

typedef struct
        {
          void *singlethread_value;
        }
        gl_tls_key_t;
# define glthread_tls_key_init(KEY, DESTRUCTOR) \
    ((KEY)->singlethread_value = NULL, \
     (void) (DESTRUCTOR),              \
     0)
# define gl_tls_get(NAME) \
    (NAME).singlethread_value
# define glthread_tls_set(KEY, POINTER) \
    ((KEY)->singlethread_value = (POINTER), 0)
# define glthread_tls_key_destroy(KEY) \
    0

#endif

 

 

 

#define gl_tls_key_init(NAME, DESTRUCTOR) \
   do                                                 \
     {                                                \
       if (glthread_tls_key_init (&NAME, DESTRUCTOR)) \
         abort ();                                    \
     }                                                \
   while (0)
#define gl_tls_set(NAME, POINTER) \
   do                                         \
     {                                        \
       if (glthread_tls_set (&NAME, POINTER)) \
         abort ();                            \
     }                                        \
   while (0)
#define gl_tls_key_destroy(NAME) \
   do                                        \
     {                                       \
       if (glthread_tls_key_destroy (&NAME)) \
         abort ();                           \
     }                                       \
   while (0)

 

#endif  
