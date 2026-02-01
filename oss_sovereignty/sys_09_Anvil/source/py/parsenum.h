 
#ifndef MICROPY_INCLUDED_PY_PARSENUM_H
#define MICROPY_INCLUDED_PY_PARSENUM_H

#include "py/mpconfig.h"
#include "py/lexer.h"
#include "py/obj.h"



mp_obj_t mp_parse_num_integer(const char *restrict str, size_t len, int base, mp_lexer_t *lex);

#if MICROPY_PY_BUILTINS_COMPLEX
mp_obj_t mp_parse_num_decimal(const char *str, size_t len, bool allow_imag, bool force_complex, mp_lexer_t *lex);

static inline mp_obj_t mp_parse_num_float(const char *str, size_t len, bool allow_imag, mp_lexer_t *lex) {
    return mp_parse_num_decimal(str, len, allow_imag, false, lex);
}

static inline mp_obj_t mp_parse_num_complex(const char *str, size_t len, mp_lexer_t *lex) {
    return mp_parse_num_decimal(str, len, true, true, lex);
}
#else
mp_obj_t mp_parse_num_float(const char *str, size_t len, bool allow_imag, mp_lexer_t *lex);
#endif

#endif 
