
#ifndef MICROPY_INCLUDED_PY_UNICODE_H
#define MICROPY_INCLUDED_PY_UNICODE_H

#include "py/mpconfig.h"
#include "py/misc.h"

mp_uint_t utf8_ptr_to_index(const byte *s, const byte *ptr);
bool utf8_check(const byte *p, size_t len);

#endif 
