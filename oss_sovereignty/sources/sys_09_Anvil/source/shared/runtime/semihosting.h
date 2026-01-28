
#ifndef MICROPY_INCLUDED_LIB_UTILS_SEMIHOSTING_H
#define MICROPY_INCLUDED_LIB_UTILS_SEMIHOSTING_H



#include <stddef.h>
#include <stdint.h>

void mp_semihosting_init();
int mp_semihosting_rx_char();
uint32_t mp_semihosting_tx_strn(const char *str, size_t len);
uint32_t mp_semihosting_tx_strn_cooked(const char *str, size_t len);

#endif 
