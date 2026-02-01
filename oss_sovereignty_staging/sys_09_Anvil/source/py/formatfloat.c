 

#include "py/mpconfig.h"
#include "py/misc.h"
#if MICROPY_FLOAT_IMPL != MICROPY_FLOAT_IMPL_NONE

#include <assert.h>
#include <stdlib.h>
#include <stdint.h>
#include <math.h>
#include "py/formatfloat.h"

 

#if MICROPY_FLOAT_IMPL == MICROPY_FLOAT_IMPL_FLOAT





#define FPTYPE float
#define FPCONST(x) x##F
#define FPROUND_TO_ONE 0.9999995F
#define FPDECEXP 32
#define FPMIN_BUF_SIZE 6 

#define FLT_SIGN_MASK   0x80000000

static inline int fp_signbit(float x) {
    mp_float_union_t fb = {x};
    return fb.i & FLT_SIGN_MASK;
}
#define fp_isnan(x) isnan(x)
#define fp_isinf(x) isinf(x)
static inline int fp_iszero(float x) {
    mp_float_union_t fb = {x};
    return fb.i == 0;
}
static inline int fp_isless1(float x) {
    mp_float_union_t fb = {x};
    return fb.i < 0x3f800000;
}

#elif MICROPY_FLOAT_IMPL == MICROPY_FLOAT_IMPL_DOUBLE

#define FPTYPE double
#define FPCONST(x) x
#define FPROUND_TO_ONE 0.999999999995
#define FPDECEXP 256
#define FPMIN_BUF_SIZE 7 
#define fp_signbit(x) signbit(x)
#define fp_isnan(x) isnan(x)
#define fp_isinf(x) isinf(x)
#define fp_iszero(x) (x == 0)
#define fp_isless1(x) (x < 1.0)

#endif 

static inline int fp_expval(FPTYPE x) {
    mp_float_union_t fb = {x};
    return (int)((fb.i >> MP_FLOAT_FRAC_BITS) & (~(0xFFFFFFFF << MP_FLOAT_EXP_BITS))) - MP_FLOAT_EXP_OFFSET;
}

int mp_format_float(FPTYPE f, char *buf, size_t buf_size, char fmt, int prec, char sign) {

    char *s = buf;

    if (buf_size <= FPMIN_BUF_SIZE) {
        
        
        

        if (buf_size >= 2) {
            *s++ = '?';
        }
        if (buf_size >= 1) {
            *s = '\0';
        }
        return buf_size >= 2;
    }
    if (fp_signbit(f) && !fp_isnan(f)) {
        *s++ = '-';
        f = -f;
    } else {
        if (sign) {
            *s++ = sign;
        }
    }

    
    
    int buf_remaining = buf_size - 1 - (s - buf);

    {
        char uc = fmt & 0x20;
        if (fp_isinf(f)) {
            *s++ = 'I' ^ uc;
            *s++ = 'N' ^ uc;
            *s++ = 'F' ^ uc;
            goto ret;
        } else if (fp_isnan(f)) {
            *s++ = 'N' ^ uc;
            *s++ = 'A' ^ uc;
            *s++ = 'N' ^ uc;
        ret:
            *s = '\0';
            return s - buf;
        }
    }

    if (prec < 0) {
        prec = 6;
    }
    char e_char = 'E' | (fmt & 0x20);   
    fmt |= 0x20; 
    char org_fmt = fmt;
    if (fmt == 'g' && prec == 0) {
        prec = 1;
    }
    int e;
    int dec = 0;
    char e_sign = '\0';
    int num_digits = 0;
    int signed_e = 0;

    
    
    int e_guess = (int)(fp_expval(f) * FPCONST(0.3010299956639812));  
    if (fp_iszero(f)) {
        e = 0;
        if (fmt == 'f') {
            
            if (prec + 2 > buf_remaining) {
                prec = buf_remaining - 2;
            }
            num_digits = prec + 1;
        } else {
            
            if (prec + 6 > buf_remaining) {
                prec = buf_remaining - 6;
            }
            if (fmt == 'e') {
                e_sign = '+';
            }
        }
    } else if (fp_isless1(f)) {
        FPTYPE f_entry = f;  
        
        e = -e_guess;
        FPTYPE u_base = MICROPY_FLOAT_C_FUN(pow)(10, -e);
        while (u_base > f) {
            ++e;
            u_base = MICROPY_FLOAT_C_FUN(pow)(10, -e);
        }
        
        
        
        f /= u_base;

        
        

        if (fmt == 'f' || (fmt == 'g' && e <= 4)) {
            fmt = 'f';
            dec = 0;

            if (org_fmt == 'g') {
                prec += (e - 1);
            }

            
            if (prec + 2 > buf_remaining) {
                prec = buf_remaining - 2;
            }

            num_digits = prec;
            signed_e = 0;
            f = f_entry;
            ++num_digits;
        } else {
            
            
            e_sign = '-';
            dec = 0;

            if (prec > (buf_remaining - FPMIN_BUF_SIZE)) {
                prec = buf_remaining - FPMIN_BUF_SIZE;
                if (fmt == 'g') {
                    prec++;
                }
            }
            signed_e = -e;
        }
    } else {
        
        
        
        
        
        e = e_guess;
        FPTYPE next_u = MICROPY_FLOAT_C_FUN(pow)(10, e + 1);
        while (f >= next_u) {
            ++e;
            next_u = MICROPY_FLOAT_C_FUN(pow)(10, e + 1);
        }

        
        
        

        if (fmt == 'f') {
            if (e >= buf_remaining) {
                fmt = 'e';
            } else if ((e + prec + 2) > buf_remaining) {
                prec = buf_remaining - e - 2;
                if (prec < 0) {
                    
                    
                    prec++;
                }
            }
        }
        if (fmt == 'e' && prec > (buf_remaining - FPMIN_BUF_SIZE)) {
            prec = buf_remaining - FPMIN_BUF_SIZE;
        }
        if (fmt == 'g') {
            
            if (prec + (FPMIN_BUF_SIZE - 1) > buf_remaining) {
                prec = buf_remaining - (FPMIN_BUF_SIZE - 1);
            }
        }
        
        

        if (fmt == 'g' && e < prec) {
            fmt = 'f';
            prec -= (e + 1);
        }
        if (fmt == 'f') {
            dec = e;
            num_digits = prec + e + 1;
        } else {
            e_sign = '+';
        }
        signed_e = e;
    }
    if (prec < 0) {
        
        prec = 0;
    }

    
    

    
    
    
    
    
    

    if (fmt == 'e') {
        num_digits = prec + 1;
    } else if (fmt == 'g') {
        if (prec == 0) {
            prec = 1;
        }
        num_digits = prec;
    }

    int d = 0;
    for (int digit_index = signed_e; num_digits >= 0; --digit_index) {
        FPTYPE u_base = FPCONST(1.0);
        if (digit_index > 0) {
            
            u_base = MICROPY_FLOAT_C_FUN(pow)(10, digit_index);
        }
        for (d = 0; d < 9; ++d) {
            if (f < u_base) {
                break;
            }
            f -= u_base;
        }
        
        
        if (num_digits > 0) {
            
            *s++ = '0' + d;
            if (dec == 0 && prec > 0) {
                *s++ = '.';
            }
        }
        --dec;
        --num_digits;
        if (digit_index <= 0) {
            
            
            
            f *= FPCONST(10.0);
        }
    }
    
    if (d >= 5) {
        char *rs = s;
        rs--;
        while (1) {
            if (*rs == '.') {
                rs--;
                continue;
            }
            if (*rs < '0' || *rs > '9') {
                
                rs++; 
                break;
            }
            if (*rs < '9') {
                (*rs)++;
                break;
            }
            *rs = '0';
            if (rs == buf) {
                break;
            }
            rs--;
        }
        if (*rs == '0') {
            
            if (rs[1] == '.' && fmt != 'f') {
                
                
                rs[0] = '.';
                rs[1] = '0';
                if (e_sign == '-') {
                    e--;
                    if (e == 0) {
                        e_sign = '+';
                    }
                } else {
                    e++;
                }
            } else {
                
                
                if ((size_t)(s + 1 - buf) < buf_size) {
                    s++;
                }
            }
            char *ss = s;
            while (ss > rs) {
                *ss = ss[-1];
                ss--;
            }
            *rs = '1';
        }
    }

    
    assert((size_t)(s + 1 - buf) <= buf_size);

    if (org_fmt == 'g' && prec > 0) {
        
        while (s[-1] == '0') {
            s--;
        }
        if (s[-1] == '.') {
            s--;
        }
    }
    
    if (e_sign) {
        *s++ = e_char;
        *s++ = e_sign;
        if (FPMIN_BUF_SIZE == 7 && e >= 100) {
            *s++ = '0' + (e / 100);
        }
        *s++ = '0' + ((e / 10) % 10);
        *s++ = '0' + (e % 10);
    }
    *s = '\0';

    
    assert((size_t)(s + 1 - buf) <= buf_size);

    return s - buf;
}

#endif 
