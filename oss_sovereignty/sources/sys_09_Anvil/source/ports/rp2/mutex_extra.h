
#ifndef MICROPY_INCLUDED_RP2_MUTEX_EXTRA_H
#define MICROPY_INCLUDED_RP2_MUTEX_EXTRA_H

#include "pico/mutex.h"

uint32_t mutex_enter_blocking_and_disable_interrupts(mutex_t *mtx);
void mutex_exit_and_restore_interrupts(mutex_t *mtx, uint32_t save);

#endif 
