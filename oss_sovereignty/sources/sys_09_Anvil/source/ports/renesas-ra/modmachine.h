
#ifndef MICROPY_INCLUDED_RENESAS_RA_MODMACHINE_H
#define MICROPY_INCLUDED_RENESAS_RA_MODMACHINE_H

#include "py/obj.h"

extern const mp_obj_type_t machine_touchpad_type;
extern const mp_obj_type_t machine_dac_type;
extern const mp_obj_type_t machine_sdcard_type;

void machine_init(void);
void machine_deinit(void);
void machine_pin_init(void);
void machine_pin_deinit(void);
void machine_i2s_init0(void);

MP_DECLARE_CONST_FUN_OBJ_0(machine_disable_irq_obj);
MP_DECLARE_CONST_FUN_OBJ_VAR_BETWEEN(machine_enable_irq_obj);

#endif 
