

#ifndef UART_H__
#define UART_H__

#include "pin.h"
#include "genhdr/pins.h"

typedef struct _machine_uart_obj_t machine_uart_obj_t;

void uart_init0(void);
void uart_deinit(void);
void uart_irq_handler(mp_uint_t uart_id);

bool uart_rx_any(machine_uart_obj_t *uart_obj);
int uart_rx_char(machine_uart_obj_t *uart_obj);
void uart_tx_strn(machine_uart_obj_t *uart_obj, const char *str, uint len);
void uart_tx_strn_cooked(machine_uart_obj_t *uart_obj, const char *str, uint len);

#endif
