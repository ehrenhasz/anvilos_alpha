 

#include "py/obj.h"
#include "py/mpstate.h"

#if MICROPY_KBD_EXCEPTION

int mp_interrupt_char = -1;

void mp_hal_set_interrupt_char(int c) {
    mp_interrupt_char = c;
}

#endif
