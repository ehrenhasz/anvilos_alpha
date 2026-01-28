
#ifndef MICROPY_INCLUDED_WEBASSEMBLY_LEXER_DEDENT_H
#define MICROPY_INCLUDED_WEBASSEMBLY_LEXER_DEDENT_H

#include "py/lexer.h"




mp_lexer_t *mp_lexer_new_from_str_len_dedent(qstr src_name, const char *str, size_t len, size_t free_len);

#endif 
