
#ifndef MICROPY_INCLUDED_RENESAS_RA_PYBTHREAD_H
#define MICROPY_INCLUDED_RENESAS_RA_PYBTHREAD_H

typedef struct _pyb_thread_t {
    void *sp;
    uint32_t local_state;
    void *arg;                  
    void *stack;                
    size_t stack_len;           
    uint32_t timeslice;
    struct _pyb_thread_t *all_next;
    struct _pyb_thread_t *run_prev;
    struct _pyb_thread_t *run_next;
    struct _pyb_thread_t *queue_next;
} pyb_thread_t;

typedef pyb_thread_t *pyb_mutex_t;

extern volatile int pyb_thread_enabled;
extern pyb_thread_t *volatile pyb_thread_all;
extern pyb_thread_t *volatile pyb_thread_cur;

void pyb_thread_init(pyb_thread_t *th);
void pyb_thread_deinit();
uint32_t pyb_thread_new(pyb_thread_t *th, void *stack, size_t stack_len, void *entry, void *arg);
void pyb_thread_dump(void);

static inline uint32_t pyb_thread_get_id(void) {
    return (uint32_t)pyb_thread_cur;
}

static inline void pyb_thread_set_local(void *value) {
    pyb_thread_cur->local_state = (uint32_t)value;
}

static inline void *pyb_thread_get_local(void) {
    return (void *)pyb_thread_cur->local_state;
}

static inline void pyb_thread_yield(void) {
    if (pyb_thread_cur->run_next == pyb_thread_cur) {
        __WFI();
    } else {
        SCB->ICSR = SCB_ICSR_PENDSVSET_Msk;
    }
}

void pyb_mutex_init(pyb_mutex_t *m);
int pyb_mutex_lock(pyb_mutex_t *m, int wait);
void pyb_mutex_unlock(pyb_mutex_t *m);

#endif 
