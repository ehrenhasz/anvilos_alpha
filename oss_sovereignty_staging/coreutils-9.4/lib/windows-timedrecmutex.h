 

#ifndef _WINDOWS_TIMEDRECMUTEX_H
#define _WINDOWS_TIMEDRECMUTEX_H

#define WIN32_LEAN_AND_MEAN   
#include <windows.h>

#include <time.h>

#include "windows-initguard.h"

 

typedef struct
        {
          glwthread_initguard_t guard;  
          DWORD owner;
          unsigned long depth;
          HANDLE event;
          CRITICAL_SECTION lock;
        }
        glwthread_timedrecmutex_t;

#define GLWTHREAD_TIMEDRECMUTEX_INIT { GLWTHREAD_INITGUARD_INIT, 0, 0 }

#ifdef __cplusplus
extern "C" {
#endif

extern int glwthread_timedrecmutex_init (glwthread_timedrecmutex_t *mutex);
extern int glwthread_timedrecmutex_lock (glwthread_timedrecmutex_t *mutex);
extern int glwthread_timedrecmutex_trylock (glwthread_timedrecmutex_t *mutex);
extern int glwthread_timedrecmutex_timedlock (glwthread_timedrecmutex_t *mutex,
                                              const struct timespec *abstime);
extern int glwthread_timedrecmutex_unlock (glwthread_timedrecmutex_t *mutex);
extern int glwthread_timedrecmutex_destroy (glwthread_timedrecmutex_t *mutex);

#ifdef __cplusplus
}
#endif

#endif  
