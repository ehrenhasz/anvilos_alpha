

#ifndef MICROPY_INCLUDED_ESP32_DOUBLE_TAP_H
#define MICROPY_INCLUDED_ESP32_DOUBLE_TAP_H

#include <stdint.h>

void double_tap_init(void);
void double_tap_mark(void);
void double_tap_invalidate(void);
bool double_tap_check_match(void);

#endif 
