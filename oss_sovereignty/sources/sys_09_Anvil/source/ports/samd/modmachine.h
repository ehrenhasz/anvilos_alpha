
#ifndef MICROPY_INCLUDED_SAMD_MODMACHINE_H
#define MICROPY_INCLUDED_SAMD_MODMACHINE_H

#include "py/obj.h"
#include "shared/timeutils/timeutils.h"

#if MICROPY_PY_MACHINE_DAC
extern const mp_obj_type_t machine_dac_type;
#endif

void rtc_gettime(timeutils_struct_time_t *tm);

#endif 
