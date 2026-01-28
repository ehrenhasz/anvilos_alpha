
#ifndef MICROPY_INCLUDED_RA_EXTINT_H
#define MICROPY_INCLUDED_RA_EXTINT_H

#include "py/mphal.h"

#define EXTI_RTC_WAKEUP     (16)
#define EXTI_NUM_VECTORS    (PYB_EXTI_NUM_VECTORS)
extern mp_obj_t pyb_extint_callback_arg[];

void extint_callback(void *param);
void extint_init0(void);
uint extint_register(mp_obj_t pin_obj, uint32_t mode, uint32_t pull, mp_obj_t callback_obj, bool override_callback_obj);
void extint_register_pin(const machine_pin_obj_t *pin, uint32_t mode, bool hard_irq, mp_obj_t callback_obj);
void extint_enable(uint line);
void extint_disable(uint line);
void extint_swint(uint line);
void extint_trigger_mode(uint line, uint32_t mode);

extern const mp_obj_type_t extint_type;

#endif 
