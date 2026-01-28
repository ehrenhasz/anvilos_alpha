
#ifndef MICROPY_INCLUDED_STM32_LED_H
#define MICROPY_INCLUDED_STM32_LED_H

typedef enum {
    PYB_LED_RED = 1,
    PYB_LED_GREEN = 2,
    PYB_LED_YELLOW = 3,
    PYB_LED_BLUE = 4,
} pyb_led_t;

void led_init(void);
void led_state(pyb_led_t led, int state);
void led_toggle(pyb_led_t led);
void led_debug(int value, int delay);

extern const mp_obj_type_t pyb_led_type;

#endif 
