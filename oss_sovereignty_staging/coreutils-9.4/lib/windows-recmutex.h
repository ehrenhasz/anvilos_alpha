 

#ifndef _WINDOWS_RECMUTEX_H
#define _WINDOWS_RECMUTEX_H

#define WIN32_LEAN_AND_MEAN   
#include <windows.h>

#include "windows-initguard.h"

 

typedef struct
        {
          glwthread_initguard_t guard;  
          DWORD owner;
          unsigned long depth;
          CRITICAL_SECTION lock;
        }
        glwthread_recmutex_t;

#define GLWTHREAD_RECMUTEX_INIT { GLWTHREAD_INITGUARD_INIT, 0, 0 }

#ifdef __cplusplus
extern "C" {
#endif

extern void glwthread_recmutex_init (glwthread_recmutex_t *mutex);
extern int glwthread_recmutex_lock (glwthread_recmutex_t *mutex);
extern int glwthread_recmutex_trylock (glwthread_recmutex_t *mutex);
extern int glwthread_recmutex_unlock (glwthread_recmutex_t *mutex);
extern int glwthread_recmutex_destroy (glwthread_recmutex_t *mutex);

#ifdef __cplusplus
}
#endif

#endif  
