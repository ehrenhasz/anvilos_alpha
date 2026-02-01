 

#include <stdlib.h>
#include <assert.h>
#include <string.h>

#include "py/parsenum.h"
#include "py/smallint.h"
#include "py/objint.h"
#include "py/objstr.h"
#include "py/runtime.h"
#include "py/binary.h"

#if MICROPY_PY_BUILTINS_FLOAT
#include <math.h>
#endif


static mp_obj_t mp_obj_int_make_new(const mp_obj_type_t *type_in, size_t n_args, size_t n_kw, const mp_obj_t *args) {
    (void)type_in;
    mp_arg_check_num(n_args, n_kw, 0, 2, false);

    switch (n_args) {
        case 0:
            return MP_OBJ_NEW_SMALL_INT(0);

        case 1: {
            mp_buffer_info_t bufinfo;
            mp_obj_t o = mp_unary_op(MP_UNARY_OP_INT_MAYBE, args[0]);
            if (o != MP_OBJ_NULL) {
                return o;
            } else if (mp_get_buffer(args[0], &bufinfo, MP_BUFFER_READ)) {
                
                return mp_parse_num_integer(bufinfo.buf, bufinfo.len, 0, NULL);
            #if MICROPY_PY_BUILTINS_FLOAT
            } else if (mp_obj_is_float(args[0])) {
                return mp_obj_new_int_from_float(mp_obj_float_get(args[0]));
            #endif
            } else {
                mp_raise_TypeError_int_conversion(args[0]);
            }
        }

        case 2:
        default: {
            
            size_t l;
            const char *s = mp_obj_str_get_data(args[0], &l);
            return mp_parse_num_integer(s, l, mp_obj_get_int(args[1]), NULL);
        }
    }
}

#if MICROPY_PY_BUILTINS_FLOAT

typedef enum {
    MP_FP_CLASS_FIT_SMALLINT,
    MP_FP_CLASS_FIT_LONGINT,
    MP_FP_CLASS_OVERFLOW
} mp_fp_as_int_class_t;

static mp_fp_as_int_class_t mp_classify_fp_as_int(mp_float_t val) {
    union {
        mp_float_t f;
        #if MICROPY_FLOAT_IMPL == MICROPY_FLOAT_IMPL_FLOAT
        uint32_t i;
        #elif MICROPY_FLOAT_IMPL == MICROPY_FLOAT_IMPL_DOUBLE
        uint32_t i[2];
        #endif
    } u = {val};

    uint32_t e;
    #if MICROPY_FLOAT_IMPL == MICROPY_FLOAT_IMPL_FLOAT
    e = u.i;
    #elif MICROPY_FLOAT_IMPL == MICROPY_FLOAT_IMPL_DOUBLE
    e = u.i[MP_ENDIANNESS_LITTLE];
    #endif
#define MP_FLOAT_SIGN_SHIFT_I32 ((MP_FLOAT_FRAC_BITS + MP_FLOAT_EXP_BITS) % 32)
#define MP_FLOAT_EXP_SHIFT_I32 (MP_FLOAT_FRAC_BITS % 32)

    if (e & (1U << MP_FLOAT_SIGN_SHIFT_I32)) {
        #if MICROPY_FLOAT_IMPL == MICROPY_FLOAT_IMPL_DOUBLE
        e |= u.i[MP_ENDIANNESS_BIG] != 0;
        #endif
        if ((e & ~(1U << MP_FLOAT_SIGN_SHIFT_I32)) == 0) {
            
            e = 0;
        } else {
            e += ((1U << MP_FLOAT_EXP_BITS) - 1) << MP_FLOAT_EXP_SHIFT_I32;
        }
    } else {
        e &= ~((1U << MP_FLOAT_EXP_SHIFT_I32) - 1);
    }
    
    
    if (e <= ((8 * sizeof(uintptr_t) + MP_FLOAT_EXP_BIAS - 3) << MP_FLOAT_EXP_SHIFT_I32)) {
        return MP_FP_CLASS_FIT_SMALLINT;
    }
    #if MICROPY_LONGINT_IMPL == MICROPY_LONGINT_IMPL_LONGLONG
    if (e <= (((sizeof(long long) * MP_BITS_PER_BYTE) + MP_FLOAT_EXP_BIAS - 2) << MP_FLOAT_EXP_SHIFT_I32)) {
        return MP_FP_CLASS_FIT_LONGINT;
    }
    #endif
    #if MICROPY_LONGINT_IMPL == MICROPY_LONGINT_IMPL_MPZ
    return MP_FP_CLASS_FIT_LONGINT;
    #else
    return MP_FP_CLASS_OVERFLOW;
    #endif
}
#undef MP_FLOAT_SIGN_SHIFT_I32
#undef MP_FLOAT_EXP_SHIFT_I32

mp_obj_t mp_obj_new_int_from_float(mp_float_t val) {
    mp_float_union_t u = {val};
    
    if (u.p.exp == ((1 << MP_FLOAT_EXP_BITS) - 1)) {
        
        if (u.p.frc == 0) {
            mp_raise_msg(&mp_type_OverflowError, MP_ERROR_TEXT("can't convert inf to int"));
        } else {
            mp_raise_ValueError(MP_ERROR_TEXT("can't convert NaN to int"));
        }
    } else {
        mp_fp_as_int_class_t icl = mp_classify_fp_as_int(val);
        if (icl == MP_FP_CLASS_FIT_SMALLINT) {
            return MP_OBJ_NEW_SMALL_INT((mp_int_t)val);
        #if MICROPY_LONGINT_IMPL == MICROPY_LONGINT_IMPL_MPZ
        } else {
            mp_obj_int_t *o = mp_obj_int_new_mpz();
            mpz_set_from_float(&o->mpz, val);
            return MP_OBJ_FROM_PTR(o);
        }
        #else
        #if MICROPY_LONGINT_IMPL == MICROPY_LONGINT_IMPL_LONGLONG
        } else if (icl == MP_FP_CLASS_FIT_LONGINT) {
            return mp_obj_new_int_from_ll((long long)val);
        #endif
        } else {
            mp_raise_ValueError(MP_ERROR_TEXT("float too big"));
        }
        #endif
    }
}

#endif

#if MICROPY_LONGINT_IMPL == MICROPY_LONGINT_IMPL_LONGLONG
typedef mp_longint_impl_t fmt_int_t;
typedef unsigned long long fmt_uint_t;
#else
typedef mp_int_t fmt_int_t;
typedef mp_uint_t fmt_uint_t;
#endif

void mp_obj_int_print(const mp_print_t *print, mp_obj_t self_in, mp_print_kind_t kind) {
    (void)kind;
    
    
    char stack_buf[sizeof(fmt_int_t) * 4];
    char *buf = stack_buf;
    size_t buf_size = sizeof(stack_buf);
    size_t fmt_size;

    char *str = mp_obj_int_formatted(&buf, &buf_size, &fmt_size, self_in, 10, NULL, '\0', '\0');
    mp_print_str(print, str);

    if (buf != stack_buf) {
        m_del(char, buf, buf_size);
    }
}

static const uint8_t log_base2_floor[] = {
    0, 1, 1, 2,
    2, 2, 2, 3,
    3, 3, 3, 3,
    3, 3, 3, 4,
     
};

size_t mp_int_format_size(size_t num_bits, int base, const char *prefix, char comma) {
    assert(2 <= base && base <= 16);
    size_t num_digits = num_bits / log_base2_floor[base - 1] + 1;
    size_t num_commas = comma ? num_digits / 3 : 0;
    size_t prefix_len = prefix ? strlen(prefix) : 0;
    return num_digits + num_commas + prefix_len + 2; 
}








char *mp_obj_int_formatted(char **buf, size_t *buf_size, size_t *fmt_size, mp_const_obj_t self_in,
    int base, const char *prefix, char base_char, char comma) {
    fmt_int_t num;
    #if MICROPY_LONGINT_IMPL == MICROPY_LONGINT_IMPL_NONE
    
    num = MP_OBJ_SMALL_INT_VALUE(self_in);
    #else
    if (mp_obj_is_small_int(self_in)) {
        
        num = MP_OBJ_SMALL_INT_VALUE(self_in);
    } else {
        assert(mp_obj_is_exact_type(self_in, &mp_type_int));
        
        #if MICROPY_LONGINT_IMPL == MICROPY_LONGINT_IMPL_LONGLONG
        const mp_obj_int_t *self = self_in;
        
        num = self->val;
        #else
        
        return mp_obj_int_formatted_impl(buf, buf_size, fmt_size, self_in, base, prefix, base_char, comma);
        #endif
    }
    #endif

    char sign = '\0';
    if (num < 0) {
        num = -num;
        sign = '-';
    }

    size_t needed_size = mp_int_format_size(sizeof(fmt_int_t) * 8, base, prefix, comma);
    if (needed_size > *buf_size) {
        *buf = m_new(char, needed_size);
        *buf_size = needed_size;
    }
    char *str = *buf;

    char *b = str + needed_size;
    *(--b) = '\0';
    char *last_comma = b;

    if (num == 0) {
        *(--b) = '0';
    } else {
        do {
            
            int c = (fmt_uint_t)num % base;
            num = (fmt_uint_t)num / base;
            if (c >= 10) {
                c += base_char - 10;
            } else {
                c += '0';
            }
            *(--b) = c;
            if (comma && num != 0 && b > str && (last_comma - b) == 3) {
                *(--b) = comma;
                last_comma = b;
            }
        }
        while (b > str && num != 0);
    }
    if (prefix) {
        size_t prefix_len = strlen(prefix);
        char *p = b - prefix_len;
        if (p > str) {
            b = p;
            while (*prefix) {
                *p++ = *prefix++;
            }
        }
    }
    if (sign && b > str) {
        *(--b) = sign;
    }
    *fmt_size = *buf + needed_size - b - 1;

    return b;
}

#if MICROPY_LONGINT_IMPL == MICROPY_LONGINT_IMPL_NONE

int mp_obj_int_sign(mp_obj_t self_in) {
    mp_int_t val = mp_obj_get_int(self_in);
    if (val < 0) {
        return -1;
    } else if (val > 0) {
        return 1;
    } else {
        return 0;
    }
}


mp_obj_t mp_obj_int_unary_op(mp_unary_op_t op, mp_obj_t o_in) {
    return MP_OBJ_NULL; 
}


mp_obj_t mp_obj_int_binary_op(mp_binary_op_t op, mp_obj_t lhs_in, mp_obj_t rhs_in) {
    return mp_obj_int_binary_op_extra_cases(op, lhs_in, rhs_in);
}


mp_obj_t mp_obj_new_int_from_str_len(const char **str, size_t len, bool neg, unsigned int base) {
    mp_raise_msg(&mp_type_OverflowError, MP_ERROR_TEXT("long int not supported in this build"));
    return mp_const_none;
}


mp_obj_t mp_obj_new_int_from_ll(long long val) {
    mp_raise_msg(&mp_type_OverflowError, MP_ERROR_TEXT("small int overflow"));
    return mp_const_none;
}


mp_obj_t mp_obj_new_int_from_ull(unsigned long long val) {
    mp_raise_msg(&mp_type_OverflowError, MP_ERROR_TEXT("small int overflow"));
    return mp_const_none;
}

mp_obj_t mp_obj_new_int_from_uint(mp_uint_t value) {
    
    
    if ((value & ~MP_SMALL_INT_POSITIVE_MASK) == 0) {
        return MP_OBJ_NEW_SMALL_INT(value);
    }
    mp_raise_msg(&mp_type_OverflowError, MP_ERROR_TEXT("small int overflow"));
    return mp_const_none;
}

mp_obj_t mp_obj_new_int(mp_int_t value) {
    if (MP_SMALL_INT_FITS(value)) {
        return MP_OBJ_NEW_SMALL_INT(value);
    }
    mp_raise_msg(&mp_type_OverflowError, MP_ERROR_TEXT("small int overflow"));
    return mp_const_none;
}

mp_int_t mp_obj_int_get_truncated(mp_const_obj_t self_in) {
    return MP_OBJ_SMALL_INT_VALUE(self_in);
}

mp_int_t mp_obj_int_get_checked(mp_const_obj_t self_in) {
    return MP_OBJ_SMALL_INT_VALUE(self_in);
}

#endif 



mp_obj_t mp_obj_int_binary_op_extra_cases(mp_binary_op_t op, mp_obj_t lhs_in, mp_obj_t rhs_in) {
    if (rhs_in == mp_const_false) {
        
        return mp_binary_op(op, lhs_in, MP_OBJ_NEW_SMALL_INT(0));
    } else if (rhs_in == mp_const_true) {
        
        return mp_binary_op(op, lhs_in, MP_OBJ_NEW_SMALL_INT(1));
    } else if (op == MP_BINARY_OP_MULTIPLY) {
        if (mp_obj_is_str_or_bytes(rhs_in) || mp_obj_is_type(rhs_in, &mp_type_tuple) || mp_obj_is_type(rhs_in, &mp_type_list)) {
            
            return mp_binary_op(op, rhs_in, lhs_in);
        }
    }
    return MP_OBJ_NULL; 
}


static mp_obj_t int_from_bytes(size_t n_args, const mp_obj_t *args) {
    
    (void)n_args;

    
    mp_buffer_info_t bufinfo;
    mp_get_buffer_raise(args[1], &bufinfo, MP_BUFFER_READ);

    const byte *buf = (const byte *)bufinfo.buf;
    int delta = 1;
    if (args[2] == MP_OBJ_NEW_QSTR(MP_QSTR_little)) {
        buf += bufinfo.len - 1;
        delta = -1;
    }

    mp_uint_t value = 0;
    size_t len = bufinfo.len;
    for (; len--; buf += delta) {
        #if MICROPY_LONGINT_IMPL != MICROPY_LONGINT_IMPL_NONE
        if (value > (MP_SMALL_INT_MAX >> 8)) {
            
            return mp_obj_int_from_bytes_impl(args[2] != MP_OBJ_NEW_QSTR(MP_QSTR_little), bufinfo.len, bufinfo.buf);
        }
        #endif
        value = (value << 8) | *buf;
    }
    return mp_obj_new_int_from_uint(value);
}

static MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(int_from_bytes_fun_obj, 3, 4, int_from_bytes);
static MP_DEFINE_CONST_CLASSMETHOD_OBJ(int_from_bytes_obj, MP_ROM_PTR(&int_from_bytes_fun_obj));

static mp_obj_t int_to_bytes(size_t n_args, const mp_obj_t *args) {
    
    (void)n_args;

    mp_int_t len = mp_obj_get_int(args[1]);
    if (len < 0) {
        mp_raise_ValueError(NULL);
    }
    bool big_endian = args[2] != MP_OBJ_NEW_QSTR(MP_QSTR_little);

    vstr_t vstr;
    vstr_init_len(&vstr, len);
    byte *data = (byte *)vstr.buf;
    memset(data, 0, len);

    #if MICROPY_LONGINT_IMPL != MICROPY_LONGINT_IMPL_NONE
    if (!mp_obj_is_small_int(args[0])) {
        mp_obj_int_to_bytes_impl(args[0], big_endian, len, data);
    } else
    #endif
    {
        mp_int_t val = MP_OBJ_SMALL_INT_VALUE(args[0]);
        size_t l = MIN((size_t)len, sizeof(val));
        mp_binary_set_int(l, big_endian, data + (big_endian ? (len - l) : 0), val);
    }

    return mp_obj_new_bytes_from_vstr(&vstr);
}
static MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(int_to_bytes_obj, 3, 4, int_to_bytes);

static const mp_rom_map_elem_t int_locals_dict_table[] = {
    { MP_ROM_QSTR(MP_QSTR_from_bytes), MP_ROM_PTR(&int_from_bytes_obj) },
    { MP_ROM_QSTR(MP_QSTR_to_bytes), MP_ROM_PTR(&int_to_bytes_obj) },
};

static MP_DEFINE_CONST_DICT(int_locals_dict, int_locals_dict_table);

MP_DEFINE_CONST_OBJ_TYPE(
    mp_type_int,
    MP_QSTR_int,
    MP_TYPE_FLAG_NONE,
    make_new, mp_obj_int_make_new,
    print, mp_obj_int_print,
    unary_op, mp_obj_int_unary_op,
    binary_op, mp_obj_int_binary_op,
    locals_dict, &int_locals_dict
    );
