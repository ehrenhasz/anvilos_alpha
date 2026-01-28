
#ifndef MICROPY_INCLUDED_STM32_RTC_H
#define MICROPY_INCLUDED_STM32_RTC_H

#include "py/obj.h"

extern RTC_HandleTypeDef RTCHandle;
extern const mp_obj_type_t pyb_rtc_type;

void rtc_init_start(bool force_init);
void rtc_init_finalise(void);

mp_obj_t pyb_rtc_wakeup(size_t n_args, const mp_obj_t *args);

#endif 
