
#ifndef MICROPY_INCLUDED_RP2_MPTHREADPORT_H
#define MICROPY_INCLUDED_RP2_MPTHREADPORT_H

#include "pico/mutex.h"

typedef struct mutex mp_thread_mutex_t;

extern void *core_state[2];

void mp_thread_init(void);
void mp_thread_deinit(void);
void mp_thread_gc_others(void);

static inline void mp_thread_set_state(struct _mp_state_thread_t *state) {
    core_state[get_core_num()] = state;
}

static inline struct _mp_state_thread_t *mp_thread_get_state(void) {
    return (struct _mp_state_thread_t *)core_state[get_core_num()];
}

static inline void mp_thread_mutex_init(mp_thread_mutex_t *m) {
    mutex_init(m);
}

static inline int mp_thread_mutex_lock(mp_thread_mutex_t *m, int wait) {
    if (wait) {
        mutex_enter_blocking(m);
        return 1;
    } else {
        return mutex_try_enter(m, NULL);
    }
}

static inline void mp_thread_mutex_unlock(mp_thread_mutex_t *m) {
    mutex_exit(m);
}

#endif 
