

#ifndef LED_H
#define LED_H

typedef enum {
#if MICROPY_HW_LED_TRICOLOR
    BOARD_LED_RED = 1,
    BOARD_LED_GREEN = 2,
    BOARD_LED_BLUE = 3
#elif (MICROPY_HW_LED_COUNT == 1)
    BOARD_LED1 = 1,
#elif (MICROPY_HW_LED_COUNT == 2)
    BOARD_LED1 = 1,
    BOARD_LED2 = 2,
#elif (MICROPY_HW_LED_COUNT == 3)
    BOARD_LED1 = 1,
    BOARD_LED2 = 2,
    BOARD_LED3 = 3,
#else
    BOARD_LED1 = 1,
    BOARD_LED2 = 2,
    BOARD_LED3 = 3,
    BOARD_LED4 = 4
#endif
} board_led_t;

void led_init(void);
void led_state(board_led_t, int);
void led_toggle(board_led_t);

extern const mp_obj_type_t board_led_type;

#endif 
