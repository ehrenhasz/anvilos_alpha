 

#ifndef _GLTHREAD_YIELD_H
#define _GLTHREAD_YIELD_H

#include <errno.h>

 

#if USE_ISOC_THREADS || USE_ISOC_AND_POSIX_THREADS

 

# include <threads.h>

# ifdef __cplusplus
extern "C" {
# endif

# define gl_thread_yield() \
    thrd_yield ()

# ifdef __cplusplus
}
# endif

#endif

 

#if USE_POSIX_THREADS

 

# include <sched.h>

# ifdef __cplusplus
extern "C" {
# endif

# define gl_thread_yield() \
    sched_yield ()

# ifdef __cplusplus
}
# endif

#endif

 

#if USE_WINDOWS_THREADS

# define WIN32_LEAN_AND_MEAN   
# include <windows.h>

# ifdef __cplusplus
extern "C" {
# endif

# define gl_thread_yield() \
    Sleep (0)

# ifdef __cplusplus
}
# endif

#endif

 

#if !(USE_ISOC_THREADS || USE_POSIX_THREADS || USE_ISOC_AND_POSIX_THREADS || USE_WINDOWS_THREADS)

 

# define gl_thread_yield() 0

#endif

 

#endif  
