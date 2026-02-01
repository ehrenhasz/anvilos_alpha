 
#ifndef MICROPY_INCLUDED_PY_MPTHREAD_H
#define MICROPY_INCLUDED_PY_MPTHREAD_H

#include "py/mpconfig.h"

#if MICROPY_PY_THREAD

struct _mp_state_thread_t;

#ifdef MICROPY_MPTHREADPORT_H
#include MICROPY_MPTHREADPORT_H
#else
#include <mpthreadport.h>
#endif

struct _mp_state_thread_t *mp_thread_get_state(void);
void mp_thread_set_state(struct _mp_state_thread_t *state);
mp_uint_t mp_thread_create(void *(*entry)(void *), void *arg, size_t *stack_size);
mp_uint_t mp_thread_get_id(void);
void mp_thread_start(void);
void mp_thread_finish(void);
void mp_thread_mutex_init(mp_thread_mutex_t *mutex);
int mp_thread_mutex_lock(mp_thread_mutex_t *mutex, int wait);
void mp_thread_mutex_unlock(mp_thread_mutex_t *mutex);

#endif 

#if MICROPY_PY_THREAD && MICROPY_PY_THREAD_GIL
#include "py/mpstate.h"
#define MP_THREAD_GIL_ENTER() mp_thread_mutex_lock(&MP_STATE_VM(gil_mutex), 1)
#define MP_THREAD_GIL_EXIT() mp_thread_mutex_unlock(&MP_STATE_VM(gil_mutex))
#else
#define MP_THREAD_GIL_ENTER()
#define MP_THREAD_GIL_EXIT()
#endif

#endif 
