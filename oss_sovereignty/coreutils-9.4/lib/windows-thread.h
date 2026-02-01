 

#ifndef _WINDOWS_THREAD_H
#define _WINDOWS_THREAD_H

 
#if !_GL_CONFIG_H_INCLUDED
 #error "Please include config.h first."
#endif

#define WIN32_LEAN_AND_MEAN   
#include <windows.h>

 
typedef struct glwthread_thread_struct *glwthread_thread_t;

#ifdef __cplusplus
extern "C" {
#endif

 
#define GLWTHREAD_ATTR_DETACHED 1
extern int glwthread_thread_create (glwthread_thread_t *threadp,
                                    unsigned int attr,
                                    void * (*func) (void *), void *arg);
extern int glwthread_thread_join (glwthread_thread_t thread, void **retvalp);
extern int glwthread_thread_detach (glwthread_thread_t thread);
extern glwthread_thread_t glwthread_thread_self (void);
extern _Noreturn void glwthread_thread_exit (void *retval);

#ifdef __cplusplus
}
#endif

#endif  
