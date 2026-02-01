 

#include "py/smallint.h"

bool mp_small_int_mul_overflow(mp_int_t x, mp_int_t y) {
    
    if (x > 0) { 
        if (y > 0) { 
            if (x > (MP_SMALL_INT_MAX / y)) {
                return true;
            }
        } else { 
            if (y < (MP_SMALL_INT_MIN / x)) {
                return true;
            }
        } 
    } else { 
        if (y > 0) { 
            if (x < (MP_SMALL_INT_MIN / y)) {
                return true;
            }
        } else { 
            if (x != 0 && y < (MP_SMALL_INT_MAX / x)) {
                return true;
            }
        } 
    } 
    return false;
}

mp_int_t mp_small_int_modulo(mp_int_t dividend, mp_int_t divisor) {
    
    dividend %= divisor;
    if ((dividend < 0 && divisor > 0) || (dividend > 0 && divisor < 0)) {
        dividend += divisor;
    }
    return dividend;
}

mp_int_t mp_small_int_floor_divide(mp_int_t num, mp_int_t denom) {
    if (num >= 0) {
        if (denom < 0) {
            num += -denom - 1;
        }
    } else {
        if (denom >= 0) {
            num += -denom + 1;
        }
    }
    return num / denom;
}
