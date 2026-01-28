
#ifndef MICROPY_INCLUDED_RP2_UART_H
#define MICROPY_INCLUDED_RP2_UART_H

void mp_uart_init(void);
void mp_uart_write_strn(const char *str, size_t len);

#endif 
