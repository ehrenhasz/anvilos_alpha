
#ifndef MICROPY_INCLUDED_CC3200_MODS_PYBRTC_H
#define MICROPY_INCLUDED_CC3200_MODS_PYBRTC_H


#define PYB_RTC_ALARM0                      (0x01)

#define RTC_ACCESS_TIME_MSEC                (5)
#define PYB_RTC_MIN_ALARM_TIME_MS           (RTC_ACCESS_TIME_MSEC * 2)

typedef struct _pyb_rtc_obj_t {
    mp_obj_base_t base;
    mp_obj_t irq_obj;
    uint32_t irq_flags;
    uint32_t alarm_ms;
    uint32_t alarm_time_s;
    uint16_t alarm_time_ms;
    byte pwrmode;
    bool alarmset;
    bool repeat;
    bool irq_enabled;
} pyb_rtc_obj_t;

extern const mp_obj_type_t pyb_rtc_type;

extern void pyb_rtc_pre_init(void);
extern void pyb_rtc_get_time (uint32_t *secs, uint16_t *msecs);
extern uint32_t pyb_rtc_get_seconds (void);
extern void pyb_rtc_calc_future_time (uint32_t a_mseconds, uint32_t *f_seconds, uint16_t *f_mseconds);
extern void pyb_rtc_repeat_alarm (pyb_rtc_obj_t *self);
extern void pyb_rtc_disable_alarm (void);

#endif 
