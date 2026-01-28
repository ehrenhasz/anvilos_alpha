
#ifndef MICROPY_INCLUDED_RA_RTC_H
#define MICROPY_INCLUDED_RA_RTC_H

#include "py/obj.h"

typedef struct
{
    uint8_t Hours;
    uint8_t Minutes;
    uint8_t Seconds;
    uint8_t TimeFormat;
    uint32_t SubSeconds;
    uint32_t SecondFraction;
    uint32_t DayLightSaving;
    uint32_t StoreOperation;
} RTC_TimeTypeDef;


typedef struct
{
    uint8_t WeekDay;
    uint8_t Month;
    uint8_t Date;
    uint8_t Year;
} RTC_DateTypeDef;

#define RTC_FORMAT_BIN                      0x000000000U
#define RTC_FORMAT_BCD                      0x000000001U

#define DUMMY_DATE {0, 11, 4, 18}
#define DUMMY_TIME {12, 0, 0, 0, 0, 0, 0, 0}

void rtc_get_time(RTC_TimeTypeDef *time);
void rtc_get_date(RTC_DateTypeDef *date);
void rtc_init_start(bool force_init);
void rtc_init_finalise(void);

mp_obj_t machine_rtc_wakeup(size_t n_args, const mp_obj_t *args);

#endif 
