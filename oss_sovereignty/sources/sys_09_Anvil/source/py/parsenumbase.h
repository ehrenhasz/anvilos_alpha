
#ifndef MICROPY_INCLUDED_PY_PARSENUMBASE_H
#define MICROPY_INCLUDED_PY_PARSENUMBASE_H

#include "py/mpconfig.h"

size_t mp_parse_num_base(const char *str, size_t len, int *base);

#endif 
