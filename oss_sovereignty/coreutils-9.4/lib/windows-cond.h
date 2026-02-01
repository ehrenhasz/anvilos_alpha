 

#ifndef _WINDOWS_COND_H
#define _WINDOWS_COND_H

#define WIN32_LEAN_AND_MEAN   
#include <windows.h>

#include <time.h>

#include "windows-initguard.h"

#ifndef _glwthread_linked_waitqueue_link_defined
#define _glwthread_linked_waitqueue_link_defined
struct glwthread_waitqueue_link
{
  struct glwthread_waitqueue_link *wql_next;
  struct glwthread_waitqueue_link *wql_prev;
};
#endif  

typedef struct
        {
          struct glwthread_waitqueue_link wq_list;  
        }
        glwthread_linked_waitqueue_t;

typedef struct
        {
          glwthread_initguard_t guard;  
          CRITICAL_SECTION lock;  
          glwthread_linked_waitqueue_t waiters;  
        }
        glwthread_cond_t;

#define GLWTHREAD_COND_INIT { GLWTHREAD_INITGUARD_INIT }

#ifdef __cplusplus
extern "C" {
#endif

extern int glwthread_cond_init (glwthread_cond_t *cond);
 
extern int glwthread_cond_wait (glwthread_cond_t *cond,
                                void *mutex,
                                int (*mutex_lock) (void *),
                                int (*mutex_unlock) (void *));
extern int glwthread_cond_timedwait (glwthread_cond_t *cond,
                                     void *mutex,
                                     int (*mutex_lock) (void *),
                                     int (*mutex_unlock) (void *),
                                     const struct timespec *abstime);
extern int glwthread_cond_signal (glwthread_cond_t *cond);
extern int glwthread_cond_broadcast (glwthread_cond_t *cond);
extern int glwthread_cond_destroy (glwthread_cond_t *cond);

#ifdef __cplusplus
}
#endif

#endif  
