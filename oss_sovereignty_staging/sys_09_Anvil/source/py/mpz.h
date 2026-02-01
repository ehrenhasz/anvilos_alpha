 
#ifndef MICROPY_INCLUDED_PY_MPZ_H
#define MICROPY_INCLUDED_PY_MPZ_H

#include <stdint.h>

#include "py/mpconfig.h"
#include "py/misc.h"














#ifndef MPZ_DIG_SIZE
  #if defined(__x86_64__) || defined(_WIN64)

    #define MPZ_DIG_SIZE (32)
  #else

    #define MPZ_DIG_SIZE (16)
  #endif
#endif

#if MPZ_DIG_SIZE > 16
#define MPZ_DBL_DIG_SIZE (64)
typedef uint32_t mpz_dig_t;
typedef uint64_t mpz_dbl_dig_t;
typedef int64_t mpz_dbl_dig_signed_t;
#elif MPZ_DIG_SIZE > 8
#define MPZ_DBL_DIG_SIZE (32)
typedef uint16_t mpz_dig_t;
typedef uint32_t mpz_dbl_dig_t;
typedef int32_t mpz_dbl_dig_signed_t;
#elif MPZ_DIG_SIZE > 4
#define MPZ_DBL_DIG_SIZE (16)
typedef uint8_t mpz_dig_t;
typedef uint16_t mpz_dbl_dig_t;
typedef int16_t mpz_dbl_dig_signed_t;
#else
#define MPZ_DBL_DIG_SIZE (8)
typedef uint8_t mpz_dig_t;
typedef uint8_t mpz_dbl_dig_t;
typedef int8_t mpz_dbl_dig_signed_t;
#endif

#ifdef _WIN64
  #ifdef __MINGW32__
    #define MPZ_LONG_1 1LL
  #else
    #define MPZ_LONG_1 1i64
  #endif
#else
  #define MPZ_LONG_1 1L
#endif


#define MPZ_NUM_DIG_FOR_INT ((sizeof(mp_int_t) * 8 + MPZ_DIG_SIZE - 1) / MPZ_DIG_SIZE)
#define MPZ_NUM_DIG_FOR_LL ((sizeof(long long) * 8 + MPZ_DIG_SIZE - 1) / MPZ_DIG_SIZE)

typedef struct _mpz_t {
    
    size_t neg : 1;
    size_t fixed_dig : 1;
    size_t alloc : (8 * sizeof(size_t) - 2);
    size_t len;
    mpz_dig_t *dig;
} mpz_t;


#define MPZ_CONST_INT(z, val) mpz_t z; mpz_dig_t z##_digits[MPZ_NUM_DIG_FOR_INT]; mpz_init_fixed_from_int(&z, z_digits, MPZ_NUM_DIG_FOR_INT, val);

void mpz_init_zero(mpz_t *z);
void mpz_init_from_int(mpz_t *z, mp_int_t val);
void mpz_init_fixed_from_int(mpz_t *z, mpz_dig_t *dig, size_t dig_alloc, mp_int_t val);
void mpz_deinit(mpz_t *z);

void mpz_set(mpz_t *dest, const mpz_t *src);
void mpz_set_from_int(mpz_t *z, mp_int_t src);
void mpz_set_from_ll(mpz_t *z, long long i, bool is_signed);
#if MICROPY_PY_BUILTINS_FLOAT
void mpz_set_from_float(mpz_t *z, mp_float_t src);
#endif
size_t mpz_set_from_str(mpz_t *z, const char *str, size_t len, bool neg, unsigned int base);
void mpz_set_from_bytes(mpz_t *z, bool big_endian, size_t len, const byte *buf);

static inline bool mpz_is_zero(const mpz_t *z) {
    return z->len == 0;
}
static inline bool mpz_is_neg(const mpz_t *z) {
    return z->neg != 0;
}
int mpz_cmp(const mpz_t *lhs, const mpz_t *rhs);

void mpz_abs_inpl(mpz_t *dest, const mpz_t *z);
void mpz_neg_inpl(mpz_t *dest, const mpz_t *z);
void mpz_not_inpl(mpz_t *dest, const mpz_t *z);
void mpz_shl_inpl(mpz_t *dest, const mpz_t *lhs, mp_uint_t rhs);
void mpz_shr_inpl(mpz_t *dest, const mpz_t *lhs, mp_uint_t rhs);
void mpz_add_inpl(mpz_t *dest, const mpz_t *lhs, const mpz_t *rhs);
void mpz_sub_inpl(mpz_t *dest, const mpz_t *lhs, const mpz_t *rhs);
void mpz_mul_inpl(mpz_t *dest, const mpz_t *lhs, const mpz_t *rhs);
void mpz_pow_inpl(mpz_t *dest, const mpz_t *lhs, const mpz_t *rhs);
void mpz_pow3_inpl(mpz_t *dest, const mpz_t *lhs, const mpz_t *rhs, const mpz_t *mod);
void mpz_and_inpl(mpz_t *dest, const mpz_t *lhs, const mpz_t *rhs);
void mpz_or_inpl(mpz_t *dest, const mpz_t *lhs, const mpz_t *rhs);
void mpz_xor_inpl(mpz_t *dest, const mpz_t *lhs, const mpz_t *rhs);
void mpz_divmod_inpl(mpz_t *dest_quo, mpz_t *dest_rem, const mpz_t *lhs, const mpz_t *rhs);

static inline size_t mpz_max_num_bits(const mpz_t *z) {
    return z->len * MPZ_DIG_SIZE;
}
mp_int_t mpz_hash(const mpz_t *z);
bool mpz_as_int_checked(const mpz_t *z, mp_int_t *value);
bool mpz_as_uint_checked(const mpz_t *z, mp_uint_t *value);
void mpz_as_bytes(const mpz_t *z, bool big_endian, size_t len, byte *buf);
#if MICROPY_PY_BUILTINS_FLOAT
mp_float_t mpz_as_float(const mpz_t *z);
#endif
size_t mpz_as_str_inpl(const mpz_t *z, unsigned int base, const char *prefix, char base_char, char comma, char *str);

#endif 
