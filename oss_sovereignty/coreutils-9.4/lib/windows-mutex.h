 

#ifndef _WINDOWS_MUTEX_H
#define _WINDOWS_MUTEX_H

#define WIN32_LEAN_AND_MEAN   
#include <windows.h>

#include "windows-initguard.h"

typedef struct
        {
          glwthread_initguard_t guard;  
          CRITICAL_SECTION lock;
        }
        glwthread_mutex_t;

#define GLWTHREAD_MUTEX_INIT { GLWTHREAD_INITGUARD_INIT }

#ifdef __cplusplus
extern "C" {
#endif

extern void glwthread_mutex_init (glwthread_mutex_t *mutex);
extern int glwthread_mutex_lock (glwthread_mutex_t *mutex);
extern int glwthread_mutex_trylock (glwthread_mutex_t *mutex);
extern int glwthread_mutex_unlock (glwthread_mutex_t *mutex);
extern int glwthread_mutex_destroy (glwthread_mutex_t *mutex);

#ifdef __cplusplus
}
#endif

#endif  
