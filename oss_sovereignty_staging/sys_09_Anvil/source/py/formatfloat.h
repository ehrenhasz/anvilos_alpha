 
#ifndef MICROPY_INCLUDED_PY_FORMATFLOAT_H
#define MICROPY_INCLUDED_PY_FORMATFLOAT_H

#include "py/mpconfig.h"

#if MICROPY_PY_BUILTINS_FLOAT
int mp_format_float(mp_float_t f, char *buf, size_t bufSize, char fmt, int prec, char sign);
#endif

#endif 
