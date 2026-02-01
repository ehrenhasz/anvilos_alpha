 

#ifndef _WINDOWS_TIMEDMUTEX_H
#define _WINDOWS_TIMEDMUTEX_H

#define WIN32_LEAN_AND_MEAN   
#include <windows.h>

#include <time.h>

#include "windows-initguard.h"

typedef struct
        {
          glwthread_initguard_t guard;  
          HANDLE event;
          CRITICAL_SECTION lock;
        }
        glwthread_timedmutex_t;

#define GLWTHREAD_TIMEDMUTEX_INIT { GLWTHREAD_INITGUARD_INIT }

#ifdef __cplusplus
extern "C" {
#endif

extern int glwthread_timedmutex_init (glwthread_timedmutex_t *mutex);
extern int glwthread_timedmutex_lock (glwthread_timedmutex_t *mutex);
extern int glwthread_timedmutex_trylock (glwthread_timedmutex_t *mutex);
extern int glwthread_timedmutex_timedlock (glwthread_timedmutex_t *mutex,
                                           const struct timespec *abstime);
extern int glwthread_timedmutex_unlock (glwthread_timedmutex_t *mutex);
extern int glwthread_timedmutex_destroy (glwthread_timedmutex_t *mutex);

#ifdef __cplusplus
}
#endif

#endif  
