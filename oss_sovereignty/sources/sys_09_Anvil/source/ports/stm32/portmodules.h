
#ifndef MICROPY_INCLUDED_STM32_PORTMODULES_H
#define MICROPY_INCLUDED_STM32_PORTMODULES_H

extern const mp_obj_module_t pyb_module;
extern const mp_obj_module_t stm_module;
extern const mp_obj_module_t mp_module_socket;



MP_DECLARE_CONST_FUN_OBJ_1(time_sleep_ms_obj);
MP_DECLARE_CONST_FUN_OBJ_1(time_sleep_us_obj);

MP_DECLARE_CONST_FUN_OBJ_0(mp_os_sync_obj);

#endif 
