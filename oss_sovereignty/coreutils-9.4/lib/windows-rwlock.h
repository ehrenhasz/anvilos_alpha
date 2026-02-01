 

#ifndef _WINDOWS_RWLOCK_H
#define _WINDOWS_RWLOCK_H

#define WIN32_LEAN_AND_MEAN   
#include <windows.h>

#include "windows-initguard.h"

 

typedef struct
        {
          HANDLE *array;  
          unsigned int count;  
          unsigned int alloc;  
          unsigned int offset;  
        }
        glwthread_carray_waitqueue_t;
typedef struct
        {
          glwthread_initguard_t guard;  
          CRITICAL_SECTION lock;  
          glwthread_carray_waitqueue_t waiting_readers;  
          glwthread_carray_waitqueue_t waiting_writers;  
          int runcount;  
        }
        glwthread_rwlock_t;

#define GLWTHREAD_RWLOCK_INIT { GLWTHREAD_INITGUARD_INIT }

#ifdef __cplusplus
extern "C" {
#endif

extern void glwthread_rwlock_init (glwthread_rwlock_t *lock);
extern int glwthread_rwlock_rdlock (glwthread_rwlock_t *lock);
extern int glwthread_rwlock_wrlock (glwthread_rwlock_t *lock);
extern int glwthread_rwlock_tryrdlock (glwthread_rwlock_t *lock);
extern int glwthread_rwlock_trywrlock (glwthread_rwlock_t *lock);
extern int glwthread_rwlock_unlock (glwthread_rwlock_t *lock);
extern int glwthread_rwlock_destroy (glwthread_rwlock_t *lock);

#ifdef __cplusplus
}
#endif

#endif  
