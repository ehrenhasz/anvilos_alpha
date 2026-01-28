

#ifndef MICROPY_INCLUDED_MIMXRT_LED_H
#define MICROPY_INCLUDED_MIMXRT_LED_H

#include "pin.h"

typedef enum {
    #if defined(MICROPY_HW_LED1_PIN)
    MACHINE_BOARD_LED1 = 1,
    #if defined(MICROPY_HW_LED2_PIN)
    MACHINE_BOARD_LED2 = 2,
    #if defined(MICROPY_HW_LED3_PIN)
    MACHINE_BOARD_LED3 = 3,
    #if defined(MICROPY_HW_LED4_PIN)
    MACHINE_BOARD_LED4 = 4,
    #endif
    #endif
    #endif
    #endif
    MICROPY_HW_LED_MAX
} machine_led_t;

typedef struct _machine_led_obj_t {
    mp_obj_base_t base;
    mp_uint_t led_id;
    const machine_pin_obj_t *led_pin;
} machine_led_obj_t;

void led_init(void);
void led_state(machine_led_t led, int state);
void led_toggle(machine_led_t led);
void led_debug(int value, int delay);

extern const mp_obj_type_t machine_led_type;
extern const machine_led_obj_t machine_led_obj[];

#endif 
