
#ifndef MICROPY_INCLUDED_STM32_MODMACHINE_H
#define MICROPY_INCLUDED_STM32_MODMACHINE_H

#include "py/obj.h"

void machine_init(void);
void machine_deinit(void);
void machine_i2s_init0();

MP_DECLARE_CONST_FUN_OBJ_VAR_BETWEEN(machine_info_obj);

MP_DECLARE_CONST_FUN_OBJ_0(machine_disable_irq_obj);
MP_DECLARE_CONST_FUN_OBJ_VAR_BETWEEN(machine_enable_irq_obj);

MP_DECLARE_CONST_FUN_OBJ_0(pyb_irq_stats_obj);

#endif 
