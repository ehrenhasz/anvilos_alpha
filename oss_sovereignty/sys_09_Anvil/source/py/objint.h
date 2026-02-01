 
#ifndef MICROPY_INCLUDED_PY_OBJINT_H
#define MICROPY_INCLUDED_PY_OBJINT_H

#include "py/mpz.h"
#include "py/obj.h"

typedef struct _mp_obj_int_t {
    mp_obj_base_t base;
    #if MICROPY_LONGINT_IMPL == MICROPY_LONGINT_IMPL_LONGLONG
    mp_longint_impl_t val;
    #elif MICROPY_LONGINT_IMPL == MICROPY_LONGINT_IMPL_MPZ
    mpz_t mpz;
    #endif
} mp_obj_int_t;

extern const mp_obj_int_t mp_sys_maxsize_obj;

#if MICROPY_PY_BUILTINS_FLOAT
mp_float_t mp_obj_int_as_float_impl(mp_obj_t self_in);
#endif

size_t mp_int_format_size(size_t num_bits, int base, const char *prefix, char comma);

mp_obj_int_t *mp_obj_int_new_mpz(void);

void mp_obj_int_print(const mp_print_t *print, mp_obj_t self_in, mp_print_kind_t kind);
char *mp_obj_int_formatted(char **buf, size_t *buf_size, size_t *fmt_size, mp_const_obj_t self_in,
    int base, const char *prefix, char base_char, char comma);
char *mp_obj_int_formatted_impl(char **buf, size_t *buf_size, size_t *fmt_size, mp_const_obj_t self_in,
    int base, const char *prefix, char base_char, char comma);
mp_int_t mp_obj_int_hash(mp_obj_t self_in);
mp_obj_t mp_obj_int_from_bytes_impl(bool big_endian, size_t len, const byte *buf);
void mp_obj_int_to_bytes_impl(mp_obj_t self_in, bool big_endian, size_t len, byte *buf);
int mp_obj_int_sign(mp_obj_t self_in);
mp_obj_t mp_obj_int_unary_op(mp_unary_op_t op, mp_obj_t o_in);
mp_obj_t mp_obj_int_binary_op(mp_binary_op_t op, mp_obj_t lhs_in, mp_obj_t rhs_in);
mp_obj_t mp_obj_int_binary_op_extra_cases(mp_binary_op_t op, mp_obj_t lhs_in, mp_obj_t rhs_in);
mp_obj_t mp_obj_int_pow3(mp_obj_t base, mp_obj_t exponent,  mp_obj_t modulus);

#endif 
