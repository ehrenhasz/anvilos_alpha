
#ifndef MICROPY_INCLUDED_RP2_PENDSV_H
#define MICROPY_INCLUDED_RP2_PENDSV_H

#include <stddef.h>

enum {
    PENDSV_DISPATCH_SOFT_TIMER,
    #if MICROPY_PY_NETWORK_CYW43
    PENDSV_DISPATCH_CYW43,
    #endif
    #if MICROPY_PY_NETWORK_WIZNET5K
    PENDSV_DISPATCH_WIZNET,
    #endif
    MICROPY_BOARD_PENDSV_ENTRIES
    PENDSV_DISPATCH_MAX
};

#define PENDSV_DISPATCH_NUM_SLOTS PENDSV_DISPATCH_MAX

typedef void (*pendsv_dispatch_t)(void);

void pendsv_suspend(void);
void pendsv_resume(void);
void pendsv_schedule_dispatch(size_t slot, pendsv_dispatch_t f);

#endif 
