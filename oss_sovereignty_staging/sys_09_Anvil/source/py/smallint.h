 
#ifndef MICROPY_INCLUDED_PY_SMALLINT_H
#define MICROPY_INCLUDED_PY_SMALLINT_H

#include "py/mpconfig.h"
#include "py/misc.h"



#ifndef MP_SMALL_INT_MIN


#if MICROPY_OBJ_REPR == MICROPY_OBJ_REPR_A || MICROPY_OBJ_REPR == MICROPY_OBJ_REPR_C

#define MP_SMALL_INT_MIN ((mp_int_t)(((mp_int_t)MP_OBJ_WORD_MSBIT_HIGH) >> 1))
#define MP_SMALL_INT_FITS(n) ((((n) ^ ((mp_uint_t)(n) << 1)) & MP_OBJ_WORD_MSBIT_HIGH) == 0)

#define MP_SMALL_INT_POSITIVE_MASK ~(MP_OBJ_WORD_MSBIT_HIGH | (MP_OBJ_WORD_MSBIT_HIGH >> 1))

#elif MICROPY_OBJ_REPR == MICROPY_OBJ_REPR_B

#define MP_SMALL_INT_MIN ((mp_int_t)(((mp_int_t)MP_OBJ_WORD_MSBIT_HIGH) >> 2))
#define MP_SMALL_INT_FITS(n) ((((n) & MP_SMALL_INT_MIN) == 0) || (((n) & MP_SMALL_INT_MIN) == MP_SMALL_INT_MIN))

#define MP_SMALL_INT_POSITIVE_MASK ~(MP_OBJ_WORD_MSBIT_HIGH | (MP_OBJ_WORD_MSBIT_HIGH >> 1) | (MP_OBJ_WORD_MSBIT_HIGH >> 2))

#elif MICROPY_OBJ_REPR == MICROPY_OBJ_REPR_D

#define MP_SMALL_INT_MIN ((mp_int_t)(((mp_int_t)0xffff800000000000) >> 1))
#define MP_SMALL_INT_FITS(n) ((((n) ^ ((n) << 1)) & 0xffff800000000000) == 0)

#define MP_SMALL_INT_POSITIVE_MASK ~(0xffff800000000000 | (0xffff800000000000 >> 1))

#endif

#endif

#define MP_SMALL_INT_MAX ((mp_int_t)(~(MP_SMALL_INT_MIN)))



#define MP_IMAX_BITS(m) ((m) / ((m) % 255 + 1) / 255 % 255 * 8 + 7 - 86 / ((m) % 255 + 12))


#define MP_SMALL_INT_BITS (MP_IMAX_BITS(MP_SMALL_INT_MAX) + 1)

bool mp_small_int_mul_overflow(mp_int_t x, mp_int_t y);
mp_int_t mp_small_int_modulo(mp_int_t dividend, mp_int_t divisor);
mp_int_t mp_small_int_floor_divide(mp_int_t num, mp_int_t denom);

#endif 
