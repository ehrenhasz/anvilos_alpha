
#ifndef MICROPY_INCLUDED_RENESAS_RA_SYSTICK_H
#define MICROPY_INCLUDED_RENESAS_RA_SYSTICK_H


#define POW2_CEIL(x) ((((x) - 1) | ((x) - 1) >> 1 | ((x) - 1) >> 2 | ((x) - 1) >> 3) + 1)

enum {
    SYSTICK_DISPATCH_DMA = 0,
    #if MICROPY_HW_ENABLE_STORAGE
    SYSTICK_DISPATCH_STORAGE,
    #endif
    #if MICROPY_PY_NETWORK && MICROPY_PY_LWIP
    SYSTICK_DISPATCH_LWIP,
    #endif
    SYSTICK_DISPATCH_MAX
};

#define SYSTICK_DISPATCH_NUM_SLOTS POW2_CEIL(SYSTICK_DISPATCH_MAX)

typedef void (*systick_dispatch_t)(uint32_t);

extern systick_dispatch_t systick_dispatch_table[SYSTICK_DISPATCH_NUM_SLOTS];

static inline void systick_enable_dispatch(size_t slot, systick_dispatch_t f) {
    systick_dispatch_table[slot] = f;
}

static inline void systick_disable_dispatch(size_t slot) {
    systick_dispatch_table[slot] = NULL;
}

void systick_wait_at_least(uint32_t stc, uint32_t delay_ms);
bool systick_has_passed(uint32_t stc, uint32_t delay_ms);

#endif 
