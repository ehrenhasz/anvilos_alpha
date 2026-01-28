
#ifndef MICROPY_INCLUDED_STM32_RNG_H
#define MICROPY_INCLUDED_STM32_RNG_H

#include "py/obj.h"

uint32_t rng_get(void);

MP_DECLARE_CONST_FUN_OBJ_0(pyb_rng_get_obj);

#endif 
