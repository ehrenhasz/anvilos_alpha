 

#ifndef _WINDOWS_TLS_H
#define _WINDOWS_TLS_H

#define WIN32_LEAN_AND_MEAN   
#include <windows.h>

typedef DWORD glwthread_tls_key_t;

#ifdef __cplusplus
extern "C" {
#endif

extern int glwthread_tls_key_create (glwthread_tls_key_t *keyp, void (*destructor) (void *));
extern void *glwthread_tls_get (glwthread_tls_key_t key);
extern int glwthread_tls_set (glwthread_tls_key_t key, void *value);
extern int glwthread_tls_key_delete (glwthread_tls_key_t key);
extern void glwthread_tls_process_destructors (void);
#define GLWTHREAD_DESTRUCTOR_ITERATIONS 4

#ifdef __cplusplus
}
#endif

#endif  
