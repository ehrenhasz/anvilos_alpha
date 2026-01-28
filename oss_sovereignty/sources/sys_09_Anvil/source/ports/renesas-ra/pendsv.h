
#ifndef MICROPY_INCLUDED_RENESAS_RA_PENDSV_H
#define MICROPY_INCLUDED_RENESAS_RA_PENDSV_H

#include "boardctrl.h"

enum {
    PENDSV_DISPATCH_SOFT_TIMER,
    #if MICROPY_PY_NETWORK && MICROPY_PY_LWIP
    PENDSV_DISPATCH_LWIP,
    #if MICROPY_PY_NETWORK_CYW43
    PENDSV_DISPATCH_CYW43,
    #endif
    #endif
    #if MICROPY_PY_BLUETOOTH && !MICROPY_PY_BLUETOOTH_USE_SYNC_EVENTS
    PENDSV_DISPATCH_BLUETOOTH_HCI,
    #endif
    MICROPY_BOARD_PENDSV_ENTRIES
    PENDSV_DISPATCH_MAX
};

#define PENDSV_DISPATCH_NUM_SLOTS PENDSV_DISPATCH_MAX

typedef void (*pendsv_dispatch_t)(void);

void pendsv_init(void);
void pendsv_kbd_intr(void);
void pendsv_schedule_dispatch(size_t slot, pendsv_dispatch_t f);

#endif 
