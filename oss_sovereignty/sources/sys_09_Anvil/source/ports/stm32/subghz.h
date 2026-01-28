
#ifndef MICROPY_INCLUDED_STM32_SUBGHZ_H
#define MICROPY_INCLUDED_STM32_SUBGHZ_H

#include "py/obj.h"



void subghz_init(void);
void subghz_deinit(void);

MP_DECLARE_CONST_FUN_OBJ_1(subghz_cs_obj);
MP_DECLARE_CONST_FUN_OBJ_1(subghz_irq_obj);
MP_DECLARE_CONST_FUN_OBJ_0(subghz_is_busy_obj);

#endif
