
#ifndef MICROPY_INCLUDED_STM32_SERVO_H
#define MICROPY_INCLUDED_STM32_SERVO_H

void servo_init(void);
void servo_timer_irq_callback(void);

extern const mp_obj_type_t pyb_servo_type;

MP_DECLARE_CONST_FUN_OBJ_2(pyb_servo_set_obj);
MP_DECLARE_CONST_FUN_OBJ_2(pyb_pwm_set_obj);

#endif 
