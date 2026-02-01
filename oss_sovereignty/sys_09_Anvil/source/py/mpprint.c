 

#include <assert.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "py/mphal.h"
#include "py/mpprint.h"
#include "py/obj.h"
#include "py/objint.h"
#include "py/runtime.h"

#if MICROPY_PY_BUILTINS_FLOAT
#include "py/formatfloat.h"
#endif

static const char pad_spaces[] = "                ";
static const char pad_zeroes[] = "0000000000000000";

static void plat_print_strn(void *env, const char *str, size_t len) {
    (void)env;
    MP_PLAT_PRINT_STRN(str, len);
}

const mp_print_t mp_plat_print = {NULL, plat_print_strn};

int mp_print_str(const mp_print_t *print, const char *str) {
    size_t len = strlen(str);
    if (len) {
        print->print_strn(print->data, str, len);
    }
    return len;
}

int mp_print_strn(const mp_print_t *print, const char *str, size_t len, int flags, char fill, int width) {
    int left_pad = 0;
    int right_pad = 0;
    int pad = width - len;
    int pad_size;
    int total_chars_printed = 0;
    const char *pad_chars;

    if (!fill || fill == ' ') {
        pad_chars = pad_spaces;
        pad_size = sizeof(pad_spaces) - 1;
    } else if (fill == '0') {
        pad_chars = pad_zeroes;
        pad_size = sizeof(pad_zeroes) - 1;
    } else {
        
        
        pad_chars = &fill;
        pad_size = 1;
    }

    if (flags & PF_FLAG_CENTER_ADJUST) {
        left_pad = pad / 2;
        right_pad = pad - left_pad;
    } else if (flags & PF_FLAG_LEFT_ADJUST) {
        right_pad = pad;
    } else {
        left_pad = pad;
    }

    if (left_pad > 0) {
        total_chars_printed += left_pad;
        while (left_pad > 0) {
            int p = left_pad;
            if (p > pad_size) {
                p = pad_size;
            }
            print->print_strn(print->data, pad_chars, p);
            left_pad -= p;
        }
    }
    if (len) {
        print->print_strn(print->data, str, len);
        total_chars_printed += len;
    }
    if (right_pad > 0) {
        total_chars_printed += right_pad;
        while (right_pad > 0) {
            int p = right_pad;
            if (p > pad_size) {
                p = pad_size;
            }
            print->print_strn(print->data, pad_chars, p);
            right_pad -= p;
        }
    }
    return total_chars_printed;
}



#define INT_BUF_SIZE (sizeof(mp_int_t) * 4)



#define SUPPORT_INT_BASE_PREFIX (0)



static int mp_print_int(const mp_print_t *print, mp_uint_t x, int sgn, int base, int base_char, int flags, char fill, int width) {
    char sign = 0;
    if (sgn) {
        if ((mp_int_t)x < 0) {
            sign = '-';
            x = -x;
        } else if (flags & PF_FLAG_SHOW_SIGN) {
            sign = '+';
        } else if (flags & PF_FLAG_SPACE_SIGN) {
            sign = ' ';
        }
    }

    char buf[INT_BUF_SIZE];
    char *b = buf + INT_BUF_SIZE;

    if (x == 0) {
        *(--b) = '0';
    } else {
        do {
            int c = x % base;
            x /= base;
            if (c >= 10) {
                c += base_char - 10;
            } else {
                c += '0';
            }
            *(--b) = c;
        } while (b > buf && x != 0);
    }

    #if SUPPORT_INT_BASE_PREFIX
    char prefix_char = '\0';

    if (flags & PF_FLAG_SHOW_PREFIX) {
        if (base == 2) {
            prefix_char = base_char + 'b' - 'a';
        } else if (base == 8) {
            prefix_char = base_char + 'o' - 'a';
        } else if (base == 16) {
            prefix_char = base_char + 'x' - 'a';
        }
    }
    #endif

    int len = 0;
    if (flags & PF_FLAG_PAD_AFTER_SIGN) {
        if (sign) {
            len += mp_print_strn(print, &sign, 1, flags, fill, 1);
            width--;
        }
        #if SUPPORT_INT_BASE_PREFIX
        if (prefix_char) {
            len += mp_print_strn(print, "0", 1, flags, fill, 1);
            len += mp_print_strn(print, &prefix_char, 1, flags, fill, 1);
            width -= 2;
        }
        #endif
    } else {
        #if SUPPORT_INT_BASE_PREFIX
        if (prefix_char && b > &buf[1]) {
            *(--b) = prefix_char;
            *(--b) = '0';
        }
        #endif
        if (sign && b > buf) {
            *(--b) = sign;
        }
    }

    len += mp_print_strn(print, b, buf + INT_BUF_SIZE - b, flags, fill, width);
    return len;
}

int mp_print_mp_int(const mp_print_t *print, mp_obj_t x, int base, int base_char, int flags, char fill, int width, int prec) {
    
    
    
    assert(base == 2 || base == 8 || base == 10 || base == 16);

    if (!mp_obj_is_int(x)) {
        
        
        x = MP_OBJ_NEW_SMALL_INT(mp_obj_get_int(x));
    }

    if ((flags & (PF_FLAG_LEFT_ADJUST | PF_FLAG_CENTER_ADJUST)) == 0 && fill == '0') {
        if (prec > width) {
            width = prec;
        }
        prec = 0;
    }
    char prefix_buf[4];
    char *prefix = prefix_buf;

    if (mp_obj_int_sign(x) >= 0) {
        if (flags & PF_FLAG_SHOW_SIGN) {
            *prefix++ = '+';
        } else if (flags & PF_FLAG_SPACE_SIGN) {
            *prefix++ = ' ';
        }
    }

    if (flags & PF_FLAG_SHOW_PREFIX) {
        if (base == 2) {
            *prefix++ = '0';
            *prefix++ = base_char + 'b' - 'a';
        } else if (base == 8) {
            *prefix++ = '0';
            if (flags & PF_FLAG_SHOW_OCTAL_LETTER) {
                *prefix++ = base_char + 'o' - 'a';
            }
        } else if (base == 16) {
            *prefix++ = '0';
            *prefix++ = base_char + 'x' - 'a';
        }
    }
    *prefix = '\0';
    int prefix_len = prefix - prefix_buf;
    prefix = prefix_buf;

    char comma = '\0';
    if (flags & PF_FLAG_SHOW_COMMA) {
        comma = ',';
    }

    
    
    char stack_buf[sizeof(mp_int_t) * 4];
    char *buf = stack_buf;
    size_t buf_size = sizeof(stack_buf);
    size_t fmt_size = 0;
    char *str;

    if (prec > 1) {
        flags |= PF_FLAG_PAD_AFTER_SIGN;
    }
    char sign = '\0';
    if (flags & PF_FLAG_PAD_AFTER_SIGN) {
        
        
        str = mp_obj_int_formatted(&buf, &buf_size, &fmt_size,
            x, base, NULL, base_char, comma);
        if (*str == '-') {
            sign = *str++;
            fmt_size--;
        }
    } else {
        str = mp_obj_int_formatted(&buf, &buf_size, &fmt_size,
            x, base, prefix, base_char, comma);
    }

    int spaces_before = 0;
    int spaces_after = 0;

    if (prec > 1) {
        
        
        

        int prec_width = fmt_size;  
        if (prec_width < prec) {
            prec_width = prec;
        }
        if (flags & PF_FLAG_PAD_AFTER_SIGN) {
            if (sign) {
                prec_width++;
            }
            prec_width += prefix_len;
        }
        if (prec_width < width) {
            if (flags & PF_FLAG_LEFT_ADJUST) {
                spaces_after = width - prec_width;
            } else {
                spaces_before = width - prec_width;
            }
        }
        fill = '0';
        flags &= ~PF_FLAG_LEFT_ADJUST;
    }

    int len = 0;
    if (spaces_before) {
        len += mp_print_strn(print, "", 0, 0, ' ', spaces_before);
    }
    if (flags & PF_FLAG_PAD_AFTER_SIGN) {
        
        if (sign) {
            len += mp_print_strn(print, &sign, 1, 0, 0, 1);
            width--;
        }
        if (prefix_len) {
            len += mp_print_strn(print, prefix, prefix_len, 0, 0, 1);
            width -= prefix_len;
        }
    }
    if (prec > 1) {
        width = prec;
    }

    len += mp_print_strn(print, str, fmt_size, flags, fill, width);

    if (spaces_after) {
        len += mp_print_strn(print, "", 0, 0, ' ', spaces_after);
    }

    if (buf != stack_buf) {
        m_del(char, buf, buf_size);
    }
    return len;
}

#if MICROPY_PY_BUILTINS_FLOAT
int mp_print_float(const mp_print_t *print, mp_float_t f, char fmt, int flags, char fill, int width, int prec) {
    char buf[32];
    char sign = '\0';
    int chrs = 0;

    if (flags & PF_FLAG_SHOW_SIGN) {
        sign = '+';
    } else
    if (flags & PF_FLAG_SPACE_SIGN) {
        sign = ' ';
    }

    int len = mp_format_float(f, buf, sizeof(buf), fmt, prec, sign);

    char *s = buf;

    if ((flags & PF_FLAG_ADD_PERCENT) && (size_t)(len + 1) < sizeof(buf)) {
        buf[len++] = '%';
        buf[len] = '\0';
    }

    
    if ((flags & PF_FLAG_PAD_AFTER_SIGN) && buf[0] < '0') {
        
        s++;
        chrs += mp_print_strn(print, &buf[0], 1, 0, 0, 1);
        width--;
        len--;
    }

    chrs += mp_print_strn(print, s, len, flags, fill, width);

    return chrs;
}
#endif

int mp_printf(const mp_print_t *print, const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    int ret = mp_vprintf(print, fmt, ap);
    va_end(ap);
    return ret;
}

int mp_vprintf(const mp_print_t *print, const char *fmt, va_list args) {
    int chrs = 0;
    for (;;) {
        {
            const char *f = fmt;
            while (*f != '\0' && *f != '%') {
                ++f; 
            }
            if (f > fmt) {
                print->print_strn(print->data, fmt, f - fmt);
                chrs += f - fmt;
                fmt = f;
            }
        }

        if (*fmt == '\0') {
            break;
        }

        
        ++fmt;

        
        int flags = 0;
        char fill = ' ';
        while (*fmt != '\0') {
            if (*fmt == '-') {
                flags |= PF_FLAG_LEFT_ADJUST;
            } else if (*fmt == '+') {
                flags |= PF_FLAG_SHOW_SIGN;
            } else if (*fmt == ' ') {
                flags |= PF_FLAG_SPACE_SIGN;
            } else if (*fmt == '!') {
                flags |= PF_FLAG_NO_TRAILZ;
            } else if (*fmt == '0') {
                flags |= PF_FLAG_PAD_AFTER_SIGN;
                fill = '0';
            } else {
                break;
            }
            ++fmt;
        }

        
        int width = 0;
        for (; '0' <= *fmt && *fmt <= '9'; ++fmt) {
            width = width * 10 + *fmt - '0';
        }

        
        int prec = -1;
        if (*fmt == '.') {
            ++fmt;
            if (*fmt == '*') {
                ++fmt;
                prec = va_arg(args, int);
            } else {
                prec = 0;
                for (; '0' <= *fmt && *fmt <= '9'; ++fmt) {
                    prec = prec * 10 + *fmt - '0';
                }
            }
            if (prec < 0) {
                prec = 0;
            }
        }

        
        #ifndef __LP64__
        const
        #endif
        bool long_arg = false;
        if (*fmt == 'l') {
            ++fmt;
            #ifdef __LP64__
            long_arg = true;
            #endif
        }

        if (*fmt == '\0') {
            break;
        }

        switch (*fmt) {
            case 'b':
                if (va_arg(args, int)) {
                    chrs += mp_print_strn(print, "true", 4, flags, fill, width);
                } else {
                    chrs += mp_print_strn(print, "false", 5, flags, fill, width);
                }
                break;
            case 'c': {
                char str = va_arg(args, int);
                chrs += mp_print_strn(print, &str, 1, flags, fill, width);
                break;
            }
            case 'q': {
                qstr qst = va_arg(args, qstr);
                size_t len;
                const char *str = (const char *)qstr_data(qst, &len);
                if (prec >= 0 && (size_t)prec < len) {
                    len = prec;
                }
                chrs += mp_print_strn(print, str, len, flags, fill, width);
                break;
            }
            case 's': {
                const char *str = va_arg(args, const char *);
                #ifndef NDEBUG
                
                if (prec != 0 && str == NULL) {
                    chrs += mp_print_strn(print, "(null)", 6, flags, fill, width);
                    break;
                }
                #endif
                size_t len = strlen(str);
                if (prec >= 0 && (size_t)prec < len) {
                    len = prec;
                }
                chrs += mp_print_strn(print, str, len, flags, fill, width);
                break;
            }
            case 'd': {
                mp_int_t val;
                if (long_arg) {
                    val = va_arg(args, long int);
                } else {
                    val = va_arg(args, int);
                }
                chrs += mp_print_int(print, val, 1, 10, 'a', flags, fill, width);
                break;
            }
            case 'u':
            case 'x':
            case 'X': {
                int base = 16 - ((*fmt + 1) & 6); 
                char fmt_c = (*fmt & 0xf0) - 'P' + 'A'; 
                mp_uint_t val;
                if (long_arg) {
                    val = va_arg(args, unsigned long int);
                } else {
                    val = va_arg(args, unsigned int);
                }
                chrs += mp_print_int(print, val, 0, base, fmt_c, flags, fill, width);
                break;
            }
            case 'p':
            case 'P': 
                
                chrs += mp_print_int(print, va_arg(args, unsigned long int), 0, 16, 'a', flags, fill, width);
                break;
            #if MICROPY_PY_BUILTINS_FLOAT
            case 'e':
            case 'E':
            case 'f':
            case 'F':
            case 'g':
            case 'G': {
                #if ((MICROPY_FLOAT_IMPL == MICROPY_FLOAT_IMPL_FLOAT) || (MICROPY_FLOAT_IMPL == MICROPY_FLOAT_IMPL_DOUBLE))
                mp_float_t f = (mp_float_t)va_arg(args, double);
                chrs += mp_print_float(print, f, *fmt, flags, fill, width, prec);
                #else
                #error Unknown MICROPY FLOAT IMPL
                #endif
                break;
            }
            #endif
                
                
                
            #if (MICROPY_OBJ_REPR == MICROPY_OBJ_REPR_D) || defined(_WIN64)
            case 'l': {
                unsigned long long int arg_value = va_arg(args, unsigned long long int);
                ++fmt;
                assert(*fmt == 'u' || *fmt == 'd' || !"unsupported fmt char");
                chrs += mp_print_int(print, arg_value, *fmt == 'd', 10, 'a', flags, fill, width);
                break;
            }
            #endif
            default:
                
                assert(*fmt == '%' || !"unsupported fmt char");
                print->print_strn(print->data, fmt, 1);
                chrs += 1;
                break;
        }
        ++fmt;
    }
    return chrs;
}
