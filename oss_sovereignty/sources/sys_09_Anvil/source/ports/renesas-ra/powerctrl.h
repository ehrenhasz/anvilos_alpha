
#ifndef MICROPY_INCLUDED_RA_POWERCTRL_H
#define MICROPY_INCLUDED_RA_POWERCTRL_H

#include <stdbool.h>
#include <stdint.h>

void SystemClock_Config(void);

NORETURN void powerctrl_mcu_reset(void);
NORETURN void powerctrl_enter_bootloader(uint32_t r0, uint32_t bl_addr);
void powerctrl_check_enter_bootloader(void);

void powerctrl_enter_stop_mode(void);
NORETURN void powerctrl_enter_standby_mode(void);

#endif 
