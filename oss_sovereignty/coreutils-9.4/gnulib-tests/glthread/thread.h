 

 


#ifndef _GLTHREAD_THREAD_H
#define _GLTHREAD_THREAD_H

 
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

 

#if USE_ISOC_THREADS

 

# include <threads.h>

# ifdef __cplusplus
extern "C" {
# endif

 

typedef struct thrd_with_exitvalue *gl_thread_t;
extern int glthread_create (gl_thread_t *threadp,
                            void *(*func) (void *), void *arg);
# define glthread_sigmask(HOW, SET, OSET) \
    pthread_sigmask (HOW, SET, OSET)
extern int glthread_join (gl_thread_t thread, void **return_value_ptr);
extern gl_thread_t gl_thread_self (void);
# define gl_thread_self_pointer() \
    (void *) gl_thread_self ()
extern _Noreturn void gl_thread_exit (void *return_value);
# define glthread_atfork(PREPARE_FUNC, PARENT_FUNC, CHILD_FUNC) 0

# ifdef __cplusplus
}
# endif

#endif

 

#if USE_POSIX_THREADS || USE_ISOC_AND_POSIX_THREADS

 

# include <pthread.h>

 
# include <stdint.h>

 
# if defined __sgi
#  include <unistd.h>
# endif

# if USE_POSIX_THREADS_WEAK
 
#  include <signal.h>
# endif

# ifdef __cplusplus
extern "C" {
# endif

# if PTHREAD_IN_USE_DETECTION_HARD

 
#  define pthread_in_use() \
     glthread_in_use ()
extern int glthread_in_use (void);

# endif

# if USE_POSIX_THREADS_WEAK

 

 

 

#  ifndef pthread_sigmask  
#   pragma weak pthread_sigmask
#  endif

#  pragma weak pthread_join
#  ifndef pthread_self
#   pragma weak pthread_self
#  endif
#  pragma weak pthread_exit
#  if HAVE_PTHREAD_ATFORK
#   pragma weak pthread_atfork
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

 

 
typedef pthread_t gl_thread_t;
# define glthread_create(THREADP, FUNC, ARG) \
    (pthread_in_use () ? pthread_create (THREADP, NULL, FUNC, ARG) : ENOSYS)
# define glthread_sigmask(HOW, SET, OSET) \
    (pthread_in_use () ? pthread_sigmask (HOW, SET, OSET) : 0)
# define glthread_join(THREAD, RETVALP) \
    (pthread_in_use () ? pthread_join (THREAD, RETVALP) : 0)
# ifdef PTW32_VERSION
    
#  define gl_thread_self() \
     (pthread_in_use () ? pthread_self () : gl_null_thread)
#  define gl_thread_self_pointer() \
     (pthread_in_use () ? pthread_self ().p : NULL)
extern const gl_thread_t gl_null_thread;
# elif defined __MVS__
    
#  define gl_thread_self() \
     (pthread_in_use () ? pthread_self () : gl_null_thread)
#  define gl_thread_self_pointer() \
     (pthread_in_use () ? *((void **) pthread_self ().__) : NULL)
extern const gl_thread_t gl_null_thread;
# else
#  define gl_thread_self() \
     (pthread_in_use () ? pthread_self () : (pthread_t) 0)
#  define gl_thread_self_pointer() \
     (pthread_in_use () ? (void *) (intptr_t) (pthread_t) pthread_self () : NULL)
# endif
# define gl_thread_exit(RETVAL) \
    (void) (pthread_in_use () ? (pthread_exit (RETVAL), 0) : 0)

# if HAVE_PTHREAD_ATFORK
#  define glthread_atfork(PREPARE_FUNC, PARENT_FUNC, CHILD_FUNC) \
     (pthread_in_use () ? pthread_atfork (PREPARE_FUNC, PARENT_FUNC, CHILD_FUNC) : 0)
# else
#  define glthread_atfork(PREPARE_FUNC, PARENT_FUNC, CHILD_FUNC) 0
# endif

# ifdef __cplusplus
}
# endif

#endif

 

#if USE_WINDOWS_THREADS

# define WIN32_LEAN_AND_MEAN   
# include <windows.h>

# include "windows-thread.h"

# ifdef __cplusplus
extern "C" {
# endif

 

typedef glwthread_thread_t gl_thread_t;
# define glthread_create(THREADP, FUNC, ARG) \
    glwthread_thread_create (THREADP, 0, FUNC, ARG)
# define glthread_sigmask(HOW, SET, OSET) \
      0
# define glthread_join(THREAD, RETVALP) \
    glwthread_thread_join (THREAD, RETVALP)
# define gl_thread_self() \
    glwthread_thread_self ()
# define gl_thread_self_pointer() \
    gl_thread_self ()
# define gl_thread_exit(RETVAL) \
    glwthread_thread_exit (RETVAL)
# define glthread_atfork(PREPARE_FUNC, PARENT_FUNC, CHILD_FUNC) 0

# ifdef __cplusplus
}
# endif

#endif

 

#if !(USE_ISOC_THREADS || USE_POSIX_THREADS || USE_ISOC_AND_POSIX_THREADS || USE_WINDOWS_THREADS)

 

typedef int gl_thread_t;
# define glthread_create(THREADP, FUNC, ARG) ENOSYS
# define glthread_sigmask(HOW, SET, OSET) 0
# define glthread_join(THREAD, RETVALP) 0
# define gl_thread_self() 0
# define gl_thread_self_pointer() \
    ((void *) gl_thread_self ())
# define gl_thread_exit(RETVAL) (void)0
# define glthread_atfork(PREPARE_FUNC, PARENT_FUNC, CHILD_FUNC) 0

#endif

 

 

#ifdef __cplusplus
extern "C" {
#endif

extern gl_thread_t gl_thread_create (void *(*func) (void *arg), void *arg);
#define gl_thread_sigmask(HOW, SET, OSET)     \
   do                                         \
     {                                        \
       if (glthread_sigmask (HOW, SET, OSET)) \
         abort ();                            \
     }                                        \
   while (0)
#define gl_thread_join(THREAD, RETVAL)     \
   do                                      \
     {                                     \
       if (glthread_join (THREAD, RETVAL)) \
         abort ();                         \
     }                                     \
   while (0)
#define gl_thread_atfork(PREPARE, PARENT, CHILD)     \
   do                                                \
     {                                               \
       if (glthread_atfork (PREPARE, PARENT, CHILD)) \
         abort ();                                   \
     }                                               \
   while (0)

#ifdef __cplusplus
}
#endif

#endif  
