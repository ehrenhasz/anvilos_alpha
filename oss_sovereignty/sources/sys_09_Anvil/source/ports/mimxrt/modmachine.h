
#ifndef MICROPY_INCLUDED_MIMXRT_MODMACHINE_H
#define MICROPY_INCLUDED_MIMXRT_MODMACHINE_H

#include "py/obj.h"

extern const mp_obj_type_t machine_sdcard_type;

void machine_adc_init(void);
void machine_pin_irq_deinit(void);
void machine_rtc_irq_deinit(void);
void machine_pwm_deinit_all(void);
void machine_uart_deinit_all(void);
void machine_timer_init_PIT(void);
void machine_sdcard_init0(void);
void mimxrt_sdram_init(void);
void machine_i2s_init0();
void machine_i2s_deinit_all(void);
void machine_rtc_start(void);
void machine_rtc_alarm_helper(int seconds, bool repeat);
void machine_uart_set_baudrate(mp_obj_t uart, uint32_t baudrate);

#endif 
