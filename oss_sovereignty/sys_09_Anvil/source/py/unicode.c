 

#include <stdint.h>

#include "py/unicode.h"


#define FL_PRINT (0x01)
#define FL_SPACE (0x02)
#define FL_DIGIT (0x04)
#define FL_ALPHA (0x08)
#define FL_UPPER (0x10)
#define FL_LOWER (0x20)
#define FL_XDIGIT (0x40)


#define AT_PR (FL_PRINT)
#define AT_SP (FL_SPACE | FL_PRINT)
#define AT_DI (FL_DIGIT | FL_PRINT | FL_XDIGIT)
#define AT_AL (FL_ALPHA | FL_PRINT)
#define AT_UP (FL_UPPER | FL_ALPHA | FL_PRINT)
#define AT_LO (FL_LOWER | FL_ALPHA | FL_PRINT)
#define AT_UX (FL_UPPER | FL_ALPHA | FL_PRINT | FL_XDIGIT)
#define AT_LX (FL_LOWER | FL_ALPHA | FL_PRINT | FL_XDIGIT)


static const uint8_t attr[] = {
    0, 0, 0, 0, 0, 0, 0, 0,
    0, AT_SP, AT_SP, AT_SP, AT_SP, AT_SP, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    AT_SP, AT_PR, AT_PR, AT_PR, AT_PR, AT_PR, AT_PR, AT_PR,
    AT_PR, AT_PR, AT_PR, AT_PR, AT_PR, AT_PR, AT_PR, AT_PR,
    AT_DI, AT_DI, AT_DI, AT_DI, AT_DI, AT_DI, AT_DI, AT_DI,
    AT_DI, AT_DI, AT_PR, AT_PR, AT_PR, AT_PR, AT_PR, AT_PR,
    AT_PR, AT_UX, AT_UX, AT_UX, AT_UX, AT_UX, AT_UX, AT_UP,
    AT_UP, AT_UP, AT_UP, AT_UP, AT_UP, AT_UP, AT_UP, AT_UP,
    AT_UP, AT_UP, AT_UP, AT_UP, AT_UP, AT_UP, AT_UP, AT_UP,
    AT_UP, AT_UP, AT_UP, AT_PR, AT_PR, AT_PR, AT_PR, AT_PR,
    AT_PR, AT_LX, AT_LX, AT_LX, AT_LX, AT_LX, AT_LX, AT_LO,
    AT_LO, AT_LO, AT_LO, AT_LO, AT_LO, AT_LO, AT_LO, AT_LO,
    AT_LO, AT_LO, AT_LO, AT_LO, AT_LO, AT_LO, AT_LO, AT_LO,
    AT_LO, AT_LO, AT_LO, AT_PR, AT_PR, AT_PR, AT_PR, 0
};

#if MICROPY_PY_BUILTINS_STR_UNICODE

unichar utf8_get_char(const byte *s) {
    unichar ord = *s++;
    if (!UTF8_IS_NONASCII(ord)) {
        return ord;
    }
    ord &= 0x7F;
    for (unichar mask = 0x40; ord & mask; mask >>= 1) {
        ord &= ~mask;
    }
    while (UTF8_IS_CONT(*s)) {
        ord = (ord << 6) | (*s++ & 0x3F);
    }
    return ord;
}

const byte *utf8_next_char(const byte *s) {
    ++s;
    while (UTF8_IS_CONT(*s)) {
        ++s;
    }
    return s;
}

mp_uint_t utf8_ptr_to_index(const byte *s, const byte *ptr) {
    mp_uint_t i = 0;
    while (ptr > s) {
        if (!UTF8_IS_CONT(*--ptr)) {
            i++;
        }
    }

    return i;
}

size_t utf8_charlen(const byte *str, size_t len) {
    size_t charlen = 0;
    for (const byte *top = str + len; str < top; ++str) {
        if (!UTF8_IS_CONT(*str)) {
            ++charlen;
        }
    }
    return charlen;
}

#endif


bool unichar_isspace(unichar c) {
    return c < 128 && (attr[c] & FL_SPACE) != 0;
}

bool unichar_isalpha(unichar c) {
    return c < 128 && (attr[c] & FL_ALPHA) != 0;
}

 

bool unichar_isdigit(unichar c) {
    return c < 128 && (attr[c] & FL_DIGIT) != 0;
}

bool unichar_isxdigit(unichar c) {
    return c < 128 && (attr[c] & FL_XDIGIT) != 0;
}

bool unichar_isident(unichar c) {
    return c < 128 && ((attr[c] & (FL_ALPHA | FL_DIGIT)) != 0 || c == '_');
}

bool unichar_isalnum(unichar c) {
    return c < 128 && ((attr[c] & (FL_ALPHA | FL_DIGIT)) != 0);
}

bool unichar_isupper(unichar c) {
    return c < 128 && (attr[c] & FL_UPPER) != 0;
}

bool unichar_islower(unichar c) {
    return c < 128 && (attr[c] & FL_LOWER) != 0;
}

unichar unichar_tolower(unichar c) {
    if (unichar_isupper(c)) {
        return c + 0x20;
    }
    return c;
}

unichar unichar_toupper(unichar c) {
    if (unichar_islower(c)) {
        return c - 0x20;
    }
    return c;
}

mp_uint_t unichar_xdigit_value(unichar c) {
    
    mp_uint_t n = c - '0';
    if (n > 9) {
        n &= ~('a' - 'A');
        n -= ('A' - ('9' + 1));
    }
    return n;
}

#if MICROPY_PY_BUILTINS_STR_UNICODE

bool utf8_check(const byte *p, size_t len) {
    uint8_t need = 0;
    const byte *end = p + len;
    for (; p < end; p++) {
        byte c = *p;
        if (need) {
            if (UTF8_IS_CONT(c)) {
                need--;
            } else {
                
                return 0;
            }
        } else {
            if (c >= 0xc0) {
                if (c >= 0xf8) {
                    
                    return 0;
                }
                need = (0xe5 >> ((c >> 3) & 0x6)) & 3;
            } else if (c >= 0x80) {
                
                return 0;
            }
        }
    }
    return need == 0; 
}

#endif
