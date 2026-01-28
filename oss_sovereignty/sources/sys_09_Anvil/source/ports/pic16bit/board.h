
#ifndef MICROPY_INCLUDED_PIC16BIT_BOARD_H
#define MICROPY_INCLUDED_PIC16BIT_BOARD_H

void cpu_init(void);

void led_init(void);
void led_state(int led, int state);
void led_toggle(int led);

void switch_init(void);
int switch_get(int sw);

void uart_init(void);
int uart_rx_any(void);
int uart_rx_char(void);
void uart_tx_char(int chr);

#endif 
