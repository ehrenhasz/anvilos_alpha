
#ifndef MICROPY_INCLUDED_STM32_POWERCTRL_H
#define MICROPY_INCLUDED_STM32_POWERCTRL_H

#include <stdbool.h>
#include <stdint.h>

#if defined(STM32WB)
void stm32_system_init(void);
#else
static inline void stm32_system_init(void) {
    SystemInit();
}
#endif

void SystemClock_Config(void);

NORETURN void powerctrl_mcu_reset(void);
NORETURN void powerctrl_enter_bootloader(uint32_t r0, uint32_t bl_addr);
void powerctrl_check_enter_bootloader(void);

void powerctrl_config_systick(void);
int powerctrl_rcc_clock_config_pll(RCC_ClkInitTypeDef *rcc_init, uint32_t sysclk_mhz, bool need_pllsai);
int powerctrl_set_sysclk(uint32_t sysclk, uint32_t ahb, uint32_t apb1, uint32_t apb2);
void powerctrl_enter_stop_mode(void);
NORETURN void powerctrl_enter_standby_mode(void);

#endif 
