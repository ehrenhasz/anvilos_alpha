 

#include <stdlib.h>
#include <string.h>

#include "py/smallint.h"
#include "py/objint.h"
#include "py/runtime.h"

#if MICROPY_PY_BUILTINS_FLOAT
#include <math.h>
#endif

#if MICROPY_LONGINT_IMPL == MICROPY_LONGINT_IMPL_LONGLONG

#if MICROPY_PY_SYS_MAXSIZE

const mp_obj_int_t mp_sys_maxsize_obj = {{&mp_type_int}, MP_SSIZE_MAX};
#endif

mp_obj_t mp_obj_int_from_bytes_impl(bool big_endian, size_t len, const byte *buf) {
    int delta = 1;
    if (!big_endian) {
        buf += len - 1;
        delta = -1;
    }

    mp_longint_impl_t value = 0;
    for (; len--; buf += delta) {
        value = (value << 8) | *buf;
    }
    return mp_obj_new_int_from_ll(value);
}

void mp_obj_int_to_bytes_impl(mp_obj_t self_in, bool big_endian, size_t len, byte *buf) {
    assert(mp_obj_is_exact_type(self_in, &mp_type_int));
    mp_obj_int_t *self = self_in;
    long long val = self->val;
    if (big_endian) {
        byte *b = buf + len;
        while (b > buf) {
            *--b = val;
            val >>= 8;
        }
    } else {
        for (; len > 0; --len) {
            *buf++ = val;
            val >>= 8;
        }
    }
}

int mp_obj_int_sign(mp_obj_t self_in) {
    mp_longint_impl_t val;
    if (mp_obj_is_small_int(self_in)) {
        val = MP_OBJ_SMALL_INT_VALUE(self_in);
    } else {
        mp_obj_int_t *self = self_in;
        val = self->val;
    }
    if (val < 0) {
        return -1;
    } else if (val > 0) {
        return 1;
    } else {
        return 0;
    }
}

mp_obj_t mp_obj_int_unary_op(mp_unary_op_t op, mp_obj_t o_in) {
    mp_obj_int_t *o = o_in;
    switch (op) {
        case MP_UNARY_OP_BOOL:
            return mp_obj_new_bool(o->val != 0);

        
        
        case MP_UNARY_OP_HASH:
            return MP_OBJ_NEW_SMALL_INT((mp_int_t)o->val);

        case MP_UNARY_OP_POSITIVE:
            return o_in;
        case MP_UNARY_OP_NEGATIVE:
            return mp_obj_new_int_from_ll(-o->val);
        case MP_UNARY_OP_INVERT:
            return mp_obj_new_int_from_ll(~o->val);
        case MP_UNARY_OP_ABS: {
            mp_obj_int_t *self = MP_OBJ_TO_PTR(o_in);
            if (self->val >= 0) {
                return o_in;
            }
            self = mp_obj_new_int_from_ll(self->val);
            
            self->val = -self->val;
            return MP_OBJ_FROM_PTR(self);
        }
        case MP_UNARY_OP_INT_MAYBE:
            return o_in;
        default:
            return MP_OBJ_NULL;      
    }
}

mp_obj_t mp_obj_int_binary_op(mp_binary_op_t op, mp_obj_t lhs_in, mp_obj_t rhs_in) {
    long long lhs_val;
    long long rhs_val;

    if (mp_obj_is_small_int(lhs_in)) {
        lhs_val = MP_OBJ_SMALL_INT_VALUE(lhs_in);
    } else {
        assert(mp_obj_is_exact_type(lhs_in, &mp_type_int));
        lhs_val = ((mp_obj_int_t *)lhs_in)->val;
    }

    if (mp_obj_is_small_int(rhs_in)) {
        rhs_val = MP_OBJ_SMALL_INT_VALUE(rhs_in);
    } else if (mp_obj_is_exact_type(rhs_in, &mp_type_int)) {
        rhs_val = ((mp_obj_int_t *)rhs_in)->val;
    } else {
        
        return mp_obj_int_binary_op_extra_cases(op, lhs_in, rhs_in);
    }

    switch (op) {
        case MP_BINARY_OP_ADD:
        case MP_BINARY_OP_INPLACE_ADD:
            return mp_obj_new_int_from_ll(lhs_val + rhs_val);
        case MP_BINARY_OP_SUBTRACT:
        case MP_BINARY_OP_INPLACE_SUBTRACT:
            return mp_obj_new_int_from_ll(lhs_val - rhs_val);
        case MP_BINARY_OP_MULTIPLY:
        case MP_BINARY_OP_INPLACE_MULTIPLY:
            return mp_obj_new_int_from_ll(lhs_val * rhs_val);
        case MP_BINARY_OP_FLOOR_DIVIDE:
        case MP_BINARY_OP_INPLACE_FLOOR_DIVIDE:
            if (rhs_val == 0) {
                goto zero_division;
            }
            return mp_obj_new_int_from_ll(lhs_val / rhs_val);
        case MP_BINARY_OP_MODULO:
        case MP_BINARY_OP_INPLACE_MODULO:
            if (rhs_val == 0) {
                goto zero_division;
            }
            return mp_obj_new_int_from_ll(lhs_val % rhs_val);

        case MP_BINARY_OP_AND:
        case MP_BINARY_OP_INPLACE_AND:
            return mp_obj_new_int_from_ll(lhs_val & rhs_val);
        case MP_BINARY_OP_OR:
        case MP_BINARY_OP_INPLACE_OR:
            return mp_obj_new_int_from_ll(lhs_val | rhs_val);
        case MP_BINARY_OP_XOR:
        case MP_BINARY_OP_INPLACE_XOR:
            return mp_obj_new_int_from_ll(lhs_val ^ rhs_val);

        case MP_BINARY_OP_LSHIFT:
        case MP_BINARY_OP_INPLACE_LSHIFT:
            return mp_obj_new_int_from_ll(lhs_val << (int)rhs_val);
        case MP_BINARY_OP_RSHIFT:
        case MP_BINARY_OP_INPLACE_RSHIFT:
            return mp_obj_new_int_from_ll(lhs_val >> (int)rhs_val);

        case MP_BINARY_OP_POWER:
        case MP_BINARY_OP_INPLACE_POWER: {
            if (rhs_val < 0) {
                #if MICROPY_PY_BUILTINS_FLOAT
                return mp_obj_float_binary_op(op, lhs_val, rhs_in);
                #else
                mp_raise_ValueError(MP_ERROR_TEXT("negative power with no float support"));
                #endif
            }
            long long ans = 1;
            while (rhs_val > 0) {
                if (rhs_val & 1) {
                    ans *= lhs_val;
                }
                if (rhs_val == 1) {
                    break;
                }
                rhs_val /= 2;
                lhs_val *= lhs_val;
            }
            return mp_obj_new_int_from_ll(ans);
        }

        case MP_BINARY_OP_LESS:
            return mp_obj_new_bool(lhs_val < rhs_val);
        case MP_BINARY_OP_MORE:
            return mp_obj_new_bool(lhs_val > rhs_val);
        case MP_BINARY_OP_LESS_EQUAL:
            return mp_obj_new_bool(lhs_val <= rhs_val);
        case MP_BINARY_OP_MORE_EQUAL:
            return mp_obj_new_bool(lhs_val >= rhs_val);
        case MP_BINARY_OP_EQUAL:
            return mp_obj_new_bool(lhs_val == rhs_val);

        default:
            return MP_OBJ_NULL; 
    }

zero_division:
    mp_raise_msg(&mp_type_ZeroDivisionError, MP_ERROR_TEXT("divide by zero"));
}

mp_obj_t mp_obj_new_int(mp_int_t value) {
    if (MP_SMALL_INT_FITS(value)) {
        return MP_OBJ_NEW_SMALL_INT(value);
    }
    return mp_obj_new_int_from_ll(value);
}

mp_obj_t mp_obj_new_int_from_uint(mp_uint_t value) {
    
    
    if ((value & ~MP_SMALL_INT_POSITIVE_MASK) == 0) {
        return MP_OBJ_NEW_SMALL_INT(value);
    }
    return mp_obj_new_int_from_ll(value);
}

mp_obj_t mp_obj_new_int_from_ll(long long val) {
    mp_obj_int_t *o = mp_obj_malloc(mp_obj_int_t, &mp_type_int);
    o->val = val;
    return o;
}

mp_obj_t mp_obj_new_int_from_ull(unsigned long long val) {
    
    if (val >> (sizeof(unsigned long long) * 8 - 1) != 0) {
        mp_raise_msg(&mp_type_OverflowError, MP_ERROR_TEXT("ulonglong too large"));
    }
    mp_obj_int_t *o = mp_obj_malloc(mp_obj_int_t, &mp_type_int);
    o->val = val;
    return o;
}

mp_obj_t mp_obj_new_int_from_str_len(const char **str, size_t len, bool neg, unsigned int base) {
    
    
    mp_obj_int_t *o = mp_obj_malloc(mp_obj_int_t, &mp_type_int);
    char *endptr;
    o->val = strtoll(*str, &endptr, base);
    *str = endptr;
    return o;
}

mp_int_t mp_obj_int_get_truncated(mp_const_obj_t self_in) {
    if (mp_obj_is_small_int(self_in)) {
        return MP_OBJ_SMALL_INT_VALUE(self_in);
    } else {
        const mp_obj_int_t *self = self_in;
        return self->val;
    }
}

mp_int_t mp_obj_int_get_checked(mp_const_obj_t self_in) {
    
    return mp_obj_int_get_truncated(self_in);
}

#if MICROPY_PY_BUILTINS_FLOAT
mp_float_t mp_obj_int_as_float_impl(mp_obj_t self_in) {
    assert(mp_obj_is_exact_type(self_in, &mp_type_int));
    mp_obj_int_t *self = self_in;
    return self->val;
}
#endif

#endif
