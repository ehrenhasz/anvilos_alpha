

#include "pybthread.h"

typedef pyb_mutex_t mp_thread_mutex_t;

void mp_thread_init(void);
void mp_thread_gc_others(void);

static inline void mp_thread_set_state(struct _mp_state_thread_t *state) {
    pyb_thread_set_local(state);
}

static inline struct _mp_state_thread_t *mp_thread_get_state(void) {
    return pyb_thread_get_local();
}

static inline void mp_thread_mutex_init(mp_thread_mutex_t *m) {
    pyb_mutex_init(m);
}

static inline int mp_thread_mutex_lock(mp_thread_mutex_t *m, int wait) {
    return pyb_mutex_lock(m, wait);
}

static inline void mp_thread_mutex_unlock(mp_thread_mutex_t *m) {
    pyb_mutex_unlock(m);
}
