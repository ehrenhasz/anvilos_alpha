 

#ifndef _WINDOWS_INITGUARD_H
#define _WINDOWS_INITGUARD_H

#define WIN32_LEAN_AND_MEAN   
#include <windows.h>

typedef struct
        {
          volatile int done;
          volatile LONG started;
        }
        glwthread_initguard_t;

#define GLWTHREAD_INITGUARD_INIT { 0, -1 }

#endif  
