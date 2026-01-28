

#ifndef MICROPY_INCLUDED_ESP32_MACHINE_RTC_H
#define MICROPY_INCLUDED_ESP32_MACHINE_RTC_H

#include "modmachine.h"

typedef struct {
    uint64_t ext1_pins; 
    int8_t ext0_pin;   
    bool wake_on_touch : 1;
    bool wake_on_ulp : 1;
    bool ext0_level : 1;
    wake_type_t ext0_wake_types;
    bool ext1_level : 1;
} machine_rtc_config_t;

extern machine_rtc_config_t machine_rtc_config;

#endif
