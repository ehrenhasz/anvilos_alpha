 

#include <stdbool.h>
#include <stdlib.h>

#include "py/runtime.h"
#include "py/parsenumbase.h"
#include "py/parsenum.h"
#include "py/smallint.h"

#if MICROPY_PY_BUILTINS_FLOAT
#include <math.h>
#endif

static NORETURN void raise_exc(mp_obj_t exc, mp_lexer_t *lex) {
    
    
    if (lex != NULL) {
        ((mp_obj_base_t *)MP_OBJ_TO_PTR(exc))->type = &mp_type_SyntaxError;
        mp_obj_exception_add_traceback(exc, lex->source_name, lex->tok_line, MP_QSTRnull);
    }
    nlr_raise(exc);
}

mp_obj_t mp_parse_num_integer(const char *restrict str_, size_t len, int base, mp_lexer_t *lex) {
    const byte *restrict str = (const byte *)str_;
    const byte *restrict top = str + len;
    bool neg = false;
    mp_obj_t ret_val;

    
    if ((base != 0 && base < 2) || base > 36) {
        
        mp_raise_ValueError(MP_ERROR_TEXT("int() arg 2 must be >= 2 and <= 36"));
    }

    
    for (; str < top && unichar_isspace(*str); str++) {
    }

    
    if (str < top) {
        if (*str == '+') {
            str++;
        } else if (*str == '-') {
            str++;
            neg = true;
        }
    }

    
    str += mp_parse_num_base((const char *)str, top - str, &base);

    
    mp_int_t int_val = 0;
    const byte *restrict str_val_start = str;
    for (; str < top; str++) {
        
        mp_uint_t dig = *str;
        if ('0' <= dig && dig <= '9') {
            dig -= '0';
        } else if (dig == '_') {
            continue;
        } else {
            dig |= 0x20; 
            if ('a' <= dig && dig <= 'z') {
                dig -= 'a' - 10;
            } else {
                
                break;
            }
        }
        if (dig >= (mp_uint_t)base) {
            break;
        }

        
        if (mp_small_int_mul_overflow(int_val, base)) {
            goto overflow;
        }
        int_val = int_val * base + dig;
        if (!MP_SMALL_INT_FITS(int_val)) {
            goto overflow;
        }
    }

    
    if (neg) {
        int_val = -int_val;
    }

    
    ret_val = MP_OBJ_NEW_SMALL_INT(int_val);

have_ret_val:
    
    if (str == str_val_start) {
        goto value_error;
    }

    
    for (; str < top && unichar_isspace(*str); str++) {
    }

    
    if (str != top) {
        goto value_error;
    }

    
    return ret_val;

overflow:
    
    {
        const char *s2 = (const char *)str_val_start;
        ret_val = mp_obj_new_int_from_str_len(&s2, top - str_val_start, neg, base);
        str = (const byte *)s2;
        goto have_ret_val;
    }

value_error:
    {
        #if MICROPY_ERROR_REPORTING <= MICROPY_ERROR_REPORTING_TERSE
        mp_obj_t exc = mp_obj_new_exception_msg(&mp_type_ValueError,
            MP_ERROR_TEXT("invalid syntax for integer"));
        raise_exc(exc, lex);
        #elif MICROPY_ERROR_REPORTING == MICROPY_ERROR_REPORTING_NORMAL
        mp_obj_t exc = mp_obj_new_exception_msg_varg(&mp_type_ValueError,
            MP_ERROR_TEXT("invalid syntax for integer with base %d"), base);
        raise_exc(exc, lex);
        #else
        vstr_t vstr;
        mp_print_t print;
        vstr_init_print(&vstr, 50, &print);
        mp_printf(&print, "invalid syntax for integer with base %d: ", base);
        mp_str_print_quoted(&print, str_val_start, top - str_val_start, true);
        mp_obj_t exc = mp_obj_new_exception_arg1(&mp_type_ValueError,
            mp_obj_new_str_from_utf8_vstr(&vstr));
        raise_exc(exc, lex);
        #endif
    }
}

enum {
    REAL_IMAG_STATE_START = 0,
    REAL_IMAG_STATE_HAVE_REAL = 1,
    REAL_IMAG_STATE_HAVE_IMAG = 2,
};

typedef enum {
    PARSE_DEC_IN_INTG,
    PARSE_DEC_IN_FRAC,
    PARSE_DEC_IN_EXP,
} parse_dec_in_t;

#if MICROPY_PY_BUILTINS_FLOAT






#if MICROPY_FLOAT_IMPL == MICROPY_FLOAT_IMPL_FLOAT
#define DEC_VAL_MAX 1e20F
#define SMALL_NORMAL_VAL (1e-37F)
#define SMALL_NORMAL_EXP (-37)
#define EXACT_POWER_OF_10 (9)
#elif MICROPY_FLOAT_IMPL == MICROPY_FLOAT_IMPL_DOUBLE
#define DEC_VAL_MAX 1e200
#define SMALL_NORMAL_VAL (1e-307)
#define SMALL_NORMAL_EXP (-307)
#define EXACT_POWER_OF_10 (22)
#endif


static void accept_digit(mp_float_t *p_dec_val, int dig, int *p_exp_extra, int in) {
    
    if (*p_dec_val < DEC_VAL_MAX) {
        
        *p_dec_val = 10 * *p_dec_val + dig;
        if (in == PARSE_DEC_IN_FRAC) {
            --(*p_exp_extra);
        }
    } else {
        
        
        if (in == PARSE_DEC_IN_INTG) {
            ++(*p_exp_extra);
        }
    }
}
#endif 

#if MICROPY_PY_BUILTINS_COMPLEX
mp_obj_t mp_parse_num_decimal(const char *str, size_t len, bool allow_imag, bool force_complex, mp_lexer_t *lex)
#else
mp_obj_t mp_parse_num_float(const char *str, size_t len, bool allow_imag, mp_lexer_t *lex)
#endif
{
    #if MICROPY_PY_BUILTINS_FLOAT

    const char *top = str + len;
    mp_float_t dec_val = 0;
    bool dec_neg = false;

    #if MICROPY_PY_BUILTINS_COMPLEX
    unsigned int real_imag_state = REAL_IMAG_STATE_START;
    mp_float_t dec_real = 0;
parse_start:
    #endif

    
    for (; str < top && unichar_isspace(*str); str++) {
    }

    
    if (str < top) {
        if (*str == '+') {
            str++;
        } else if (*str == '-') {
            str++;
            dec_neg = true;
        }
    }

    const char *str_val_start = str;

    
    if (str < top && (str[0] | 0x20) == 'i') {
        
        if (str + 2 < top && (str[1] | 0x20) == 'n' && (str[2] | 0x20) == 'f') {
            
            str += 3;
            dec_val = (mp_float_t)INFINITY;
            if (str + 4 < top && (str[0] | 0x20) == 'i' && (str[1] | 0x20) == 'n' && (str[2] | 0x20) == 'i' && (str[3] | 0x20) == 't' && (str[4] | 0x20) == 'y') {
                
                str += 5;
            }
        }
    } else if (str < top && (str[0] | 0x20) == 'n') {
        
        if (str + 2 < top && (str[1] | 0x20) == 'a' && (str[2] | 0x20) == 'n') {
            
            str += 3;
            dec_val = MICROPY_FLOAT_C_FUN(nan)("");
        }
    } else {
        
        parse_dec_in_t in = PARSE_DEC_IN_INTG;
        bool exp_neg = false;
        int exp_val = 0;
        int exp_extra = 0;
        int trailing_zeros_intg = 0, trailing_zeros_frac = 0;
        while (str < top) {
            unsigned int dig = *str++;
            if ('0' <= dig && dig <= '9') {
                dig -= '0';
                if (in == PARSE_DEC_IN_EXP) {
                    
                    
                    
                    if (exp_val < (INT_MAX / 2 - 9) / 10) {
                        exp_val = 10 * exp_val + dig;
                    }
                } else {
                    if (dig == 0 || dec_val >= DEC_VAL_MAX) {
                        
                        
                        if (in == PARSE_DEC_IN_INTG) {
                            ++trailing_zeros_intg;
                        } else {
                            ++trailing_zeros_frac;
                        }
                    } else {
                        
                        while (trailing_zeros_intg) {
                            accept_digit(&dec_val, 0, &exp_extra, PARSE_DEC_IN_INTG);
                            --trailing_zeros_intg;
                        }
                        while (trailing_zeros_frac) {
                            accept_digit(&dec_val, 0, &exp_extra, PARSE_DEC_IN_FRAC);
                            --trailing_zeros_frac;
                        }
                        accept_digit(&dec_val, dig, &exp_extra, in);
                    }
                }
            } else if (in == PARSE_DEC_IN_INTG && dig == '.') {
                in = PARSE_DEC_IN_FRAC;
            } else if (in != PARSE_DEC_IN_EXP && ((dig | 0x20) == 'e')) {
                in = PARSE_DEC_IN_EXP;
                if (str < top) {
                    if (str[0] == '+') {
                        str++;
                    } else if (str[0] == '-') {
                        str++;
                        exp_neg = true;
                    }
                }
                if (str == top) {
                    goto value_error;
                }
            } else if (dig == '_') {
                continue;
            } else {
                
                str--;
                break;
            }
        }

        
        if (exp_neg) {
            exp_val = -exp_val;
        }

        
        exp_val += exp_extra + trailing_zeros_intg;
        if (exp_val < SMALL_NORMAL_EXP) {
            exp_val -= SMALL_NORMAL_EXP;
            dec_val *= SMALL_NORMAL_VAL;
        }

        
        
        
        
        
        if (exp_val < 0 && exp_val >= -EXACT_POWER_OF_10) {
            dec_val /= MICROPY_FLOAT_C_FUN(pow)(10, -exp_val);
        } else {
            dec_val *= MICROPY_FLOAT_C_FUN(pow)(10, exp_val);
        }
    }

    if (allow_imag && str < top && (*str | 0x20) == 'j') {
        #if MICROPY_PY_BUILTINS_COMPLEX
        if (str == str_val_start) {
            
            dec_val = 1;
        }
        ++str;
        real_imag_state |= REAL_IMAG_STATE_HAVE_IMAG;
        #else
        raise_exc(mp_obj_new_exception_msg(&mp_type_ValueError, MP_ERROR_TEXT("complex values not supported")), lex);
        #endif
    }

    
    if (dec_neg) {
        dec_val = -dec_val;
    }

    
    if (str == str_val_start) {
        goto value_error;
    }

    
    for (; str < top && unichar_isspace(*str); str++) {
    }

    
    if (str != top) {
        #if MICROPY_PY_BUILTINS_COMPLEX
        if (force_complex && real_imag_state == REAL_IMAG_STATE_START) {
            
            dec_real = dec_val;
            dec_val = 0;
            real_imag_state |= REAL_IMAG_STATE_HAVE_REAL;
            goto parse_start;
        }
        #endif
        goto value_error;
    }

    #if MICROPY_PY_BUILTINS_COMPLEX
    if (real_imag_state == REAL_IMAG_STATE_HAVE_REAL) {
        
        goto value_error;
    }
    #endif

    

    #if MICROPY_PY_BUILTINS_COMPLEX
    if (real_imag_state != REAL_IMAG_STATE_START) {
        return mp_obj_new_complex(dec_real, dec_val);
    } else if (force_complex) {
        return mp_obj_new_complex(dec_val, 0);
    }
    #endif

    return mp_obj_new_float(dec_val);

value_error:
    raise_exc(mp_obj_new_exception_msg(&mp_type_ValueError, MP_ERROR_TEXT("invalid syntax for number")), lex);

    #else
    raise_exc(mp_obj_new_exception_msg(&mp_type_ValueError, MP_ERROR_TEXT("decimal numbers not supported")), lex);
    #endif
}
