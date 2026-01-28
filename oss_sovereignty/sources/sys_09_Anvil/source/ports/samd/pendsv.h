
#ifndef MICROPY_INCLUDED_SAMD_PENDSV_H
#define MICROPY_INCLUDED_SAMD_PENDSV_H

enum {
    PENDSV_DISPATCH_SOFT_TIMER,  
    MICROPY_BOARD_PENDSV_ENTRIES
    PENDSV_DISPATCH_MAX
};

#define PENDSV_DISPATCH_NUM_SLOTS PENDSV_DISPATCH_MAX

typedef void (*pendsv_dispatch_t)(void);

void pendsv_init(void);
void pendsv_schedule_dispatch(size_t slot, pendsv_dispatch_t f);

#endif 
