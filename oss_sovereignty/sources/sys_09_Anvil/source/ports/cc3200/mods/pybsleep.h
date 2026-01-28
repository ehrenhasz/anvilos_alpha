
#ifndef MICROPY_INCLUDED_CC3200_MODS_PYBSLEEP_H
#define MICROPY_INCLUDED_CC3200_MODS_PYBSLEEP_H


#define PYB_PWR_MODE_ACTIVE                     (0x01)
#define PYB_PWR_MODE_LPDS                       (0x02)
#define PYB_PWR_MODE_HIBERNATE                  (0x04)


typedef enum {
    PYB_SLP_PWRON_RESET = 0,
    PYB_SLP_HARD_RESET,
    PYB_SLP_WDT_RESET,
    PYB_SLP_HIB_RESET,
    PYB_SLP_SOFT_RESET
} pybsleep_reset_cause_t;

typedef enum {
    PYB_SLP_WAKED_PWRON = 0,
    PYB_SLP_WAKED_BY_WLAN,
    PYB_SLP_WAKED_BY_GPIO,
    PYB_SLP_WAKED_BY_RTC
} pybsleep_wake_reason_t;

typedef void (*WakeUpCB_t)(const mp_obj_t self);


void pyb_sleep_pre_init (void);
void pyb_sleep_init0 (void);
void pyb_sleep_signal_soft_reset (void);
void pyb_sleep_add (const mp_obj_t obj, WakeUpCB_t wakeup);
void pyb_sleep_remove (const mp_obj_t obj);
void pyb_sleep_set_gpio_lpds_callback (mp_obj_t cb_obj);
void pyb_sleep_set_wlan_obj (mp_obj_t wlan_obj);
void pyb_sleep_set_rtc_obj (mp_obj_t rtc_obj);
void pyb_sleep_sleep (void);
void pyb_sleep_deepsleep (void);
pybsleep_reset_cause_t pyb_sleep_get_reset_cause (void);
pybsleep_wake_reason_t pyb_sleep_get_wake_reason (void);

#endif 
