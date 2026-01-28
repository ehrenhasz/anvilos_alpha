
#ifndef MICROPY_INCLUDED_RENESAS_RA_LED_H
#define MICROPY_INCLUDED_RENESAS_RA_LED_H

typedef enum {
    RA_LED1 = 1,
    RA_LED2 = 2,
    RA_LED3 = 3,
    RA_LED4 = 4,
} ra_led_t;

void led_init(void);
void led_state(ra_led_t led, int state);
void led_toggle(ra_led_t led);
void led_debug(int value, int delay);

extern const mp_obj_type_t ra_led_type;

#endif 
