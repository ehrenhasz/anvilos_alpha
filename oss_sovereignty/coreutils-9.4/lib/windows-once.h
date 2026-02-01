 

#ifndef _WINDOWS_ONCE_H
#define _WINDOWS_ONCE_H

#define WIN32_LEAN_AND_MEAN   
#include <windows.h>

typedef struct
        {
          volatile int inited;
          volatile LONG started;
          CRITICAL_SECTION lock;
        }
        glwthread_once_t;

#define GLWTHREAD_ONCE_INIT { -1, -1 }

#ifdef __cplusplus
extern "C" {
#endif

extern void glwthread_once (glwthread_once_t *once_control,
                            void (*initfunction) (void));

#ifdef __cplusplus
}
#endif

#endif  
