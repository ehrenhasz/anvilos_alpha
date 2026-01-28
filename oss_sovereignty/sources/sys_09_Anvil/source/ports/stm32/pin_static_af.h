
#ifndef MICROPY_INCLUDED_STM32_PIN_STATIC_AF_H
#define MICROPY_INCLUDED_STM32_PIN_STATIC_AF_H


#include <string.h>

#include "py/mphal.h"
#include "genhdr/pins.h"
#include "genhdr/pins_af_defs.h"

#if 0 
#define mp_hal_pin_config_alt_static(pin_obj, mode, pull, fn_type) \
    mp_hal_pin_config(pin_obj, mode, pull, fn_type(pin_obj)); \
    _Static_assert(fn_type(pin_obj) != -1, ""); \
    _Static_assert(__builtin_constant_p(fn_type(pin_obj)) == 1, "")

#else

#define mp_hal_pin_config_alt_static(pin_obj, mode, pull, fn_type) \
    mp_hal_pin_config(pin_obj, mode, pull, fn_type(pin_obj))     

#define mp_hal_pin_config_alt_static_speed(pin_obj, mode, pull, speed, fn_type) \
    mp_hal_pin_config(pin_obj, mode, pull, fn_type(pin_obj));      \
    mp_hal_pin_config_speed(pin_obj, speed)

#endif

#endif 
