
#ifndef MICROPY_INCLUDED_SHARED_RUNTIME_SOFTTIMER_H
#define MICROPY_INCLUDED_SHARED_RUNTIME_SOFTTIMER_H

#include "py/pairheap.h"

#define SOFT_TIMER_FLAG_PY_CALLBACK (1)
#define SOFT_TIMER_FLAG_GC_ALLOCATED (2)

#define SOFT_TIMER_MODE_ONE_SHOT (1)
#define SOFT_TIMER_MODE_PERIODIC (2)

typedef struct _soft_timer_entry_t {
    mp_pairheap_t pairheap;
    uint16_t flags;
    uint16_t mode;
    uint32_t expiry_ms;
    uint32_t delta_ms; 
    union {
        void (*c_callback)(struct _soft_timer_entry_t *);
        mp_obj_t py_callback;
    };
} soft_timer_entry_t;

extern volatile uint32_t soft_timer_next;

static inline int32_t soft_timer_ticks_diff(uint32_t t1, uint32_t t0) {
    
    
    
    return t1 - t0;
}

void soft_timer_deinit(void);
void soft_timer_handler(void);
void soft_timer_gc_mark_all(void);

void soft_timer_static_init(soft_timer_entry_t *entry, uint16_t mode, uint32_t delta_ms, void (*cb)(soft_timer_entry_t *));
void soft_timer_insert(soft_timer_entry_t *entry, uint32_t initial_delta_ms);
void soft_timer_remove(soft_timer_entry_t *entry);



static inline void soft_timer_reinsert(soft_timer_entry_t *entry, uint32_t initial_delta_ms) {
    soft_timer_remove(entry);
    soft_timer_insert(entry, initial_delta_ms);
}

#if !defined(MICROPY_SOFT_TIMER_TICKS_MS)








uint32_t soft_timer_get_ms(void);
void soft_timer_schedule_at_ms(uint32_t ticks_ms);
#endif

#endif 
