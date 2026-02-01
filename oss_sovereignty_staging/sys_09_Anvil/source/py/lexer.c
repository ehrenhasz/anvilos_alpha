 

#include <stdio.h>
#include <string.h>
#include <assert.h>

#include "py/reader.h"
#include "py/lexer.h"
#include "py/runtime.h"

#if MICROPY_ENABLE_COMPILER

#define TAB_SIZE (8)




#define MP_LEXER_EOF ((unichar)MP_READER_EOF)
#define CUR_CHAR(lex) ((lex)->chr0)

static bool is_end(mp_lexer_t *lex) {
    return lex->chr0 == MP_LEXER_EOF;
}

static bool is_physical_newline(mp_lexer_t *lex) {
    return lex->chr0 == '\n';
}

static bool is_char(mp_lexer_t *lex, byte c) {
    return lex->chr0 == c;
}

static bool is_char_or(mp_lexer_t *lex, byte c1, byte c2) {
    return lex->chr0 == c1 || lex->chr0 == c2;
}

static bool is_char_or3(mp_lexer_t *lex, byte c1, byte c2, byte c3) {
    return lex->chr0 == c1 || lex->chr0 == c2 || lex->chr0 == c3;
}

#if MICROPY_PY_FSTRINGS
static bool is_char_or4(mp_lexer_t *lex, byte c1, byte c2, byte c3, byte c4) {
    return lex->chr0 == c1 || lex->chr0 == c2 || lex->chr0 == c3 || lex->chr0 == c4;
}
#endif

static bool is_char_following(mp_lexer_t *lex, byte c) {
    return lex->chr1 == c;
}

static bool is_char_following_or(mp_lexer_t *lex, byte c1, byte c2) {
    return lex->chr1 == c1 || lex->chr1 == c2;
}

static bool is_char_following_following_or(mp_lexer_t *lex, byte c1, byte c2) {
    return lex->chr2 == c1 || lex->chr2 == c2;
}

static bool is_char_and(mp_lexer_t *lex, byte c1, byte c2) {
    return lex->chr0 == c1 && lex->chr1 == c2;
}

static bool is_whitespace(mp_lexer_t *lex) {
    return unichar_isspace(lex->chr0);
}

static bool is_letter(mp_lexer_t *lex) {
    return unichar_isalpha(lex->chr0);
}

static bool is_digit(mp_lexer_t *lex) {
    return unichar_isdigit(lex->chr0);
}

static bool is_following_digit(mp_lexer_t *lex) {
    return unichar_isdigit(lex->chr1);
}

static bool is_following_base_char(mp_lexer_t *lex) {
    const unichar chr1 = lex->chr1 | 0x20;
    return chr1 == 'b' || chr1 == 'o' || chr1 == 'x';
}

static bool is_following_odigit(mp_lexer_t *lex) {
    return lex->chr1 >= '0' && lex->chr1 <= '7';
}

static bool is_string_or_bytes(mp_lexer_t *lex) {
    return is_char_or(lex, '\'', '\"')
           #if MICROPY_PY_FSTRINGS
           || (is_char_or4(lex, 'r', 'u', 'b', 'f') && is_char_following_or(lex, '\'', '\"'))
           || (((is_char_and(lex, 'r', 'f') || is_char_and(lex, 'f', 'r'))
               && is_char_following_following_or(lex, '\'', '\"')))
           #else
           || (is_char_or3(lex, 'r', 'u', 'b') && is_char_following_or(lex, '\'', '\"'))
           #endif
           || ((is_char_and(lex, 'r', 'b') || is_char_and(lex, 'b', 'r'))
               && is_char_following_following_or(lex, '\'', '\"'));
}


static bool is_head_of_identifier(mp_lexer_t *lex) {
    return is_letter(lex) || lex->chr0 == '_' || lex->chr0 >= 0x80;
}

static bool is_tail_of_identifier(mp_lexer_t *lex) {
    return is_head_of_identifier(lex) || is_digit(lex);
}

static void next_char(mp_lexer_t *lex) {
    if (lex->chr0 == '\n') {
        
        ++lex->line;
        lex->column = 1;
    } else if (lex->chr0 == '\t') {
        
        lex->column = (((lex->column - 1 + TAB_SIZE) / TAB_SIZE) * TAB_SIZE) + 1;
    } else {
        
        ++lex->column;
    }

    
    lex->chr0 = lex->chr1;
    lex->chr1 = lex->chr2;

    
    #if MICROPY_PY_FSTRINGS
    if (lex->fstring_args_idx) {
        
        if (lex->fstring_args_idx < lex->fstring_args.len) {
            lex->chr2 = lex->fstring_args.buf[lex->fstring_args_idx++];
        } else {
            
            lex->chr2 = '\0';
        }

        if (lex->chr0 == '\0') {
            
            lex->chr0 = lex->chr0_saved;
            lex->chr1 = lex->chr1_saved;
            lex->chr2 = lex->chr2_saved;
            
            vstr_reset(&lex->fstring_args);
            lex->fstring_args_idx = 0;
        }
    } else
    #endif
    {
        lex->chr2 = lex->reader.readbyte(lex->reader.data);
    }

    if (lex->chr1 == '\r') {
        
        lex->chr1 = '\n';
        if (lex->chr2 == '\n') {
            
            lex->chr2 = lex->reader.readbyte(lex->reader.data);
        }
    }

    
    if (lex->chr2 == MP_LEXER_EOF && lex->chr1 != MP_LEXER_EOF && lex->chr1 != '\n') {
        lex->chr2 = '\n';
    }
}

static void indent_push(mp_lexer_t *lex, size_t indent) {
    if (lex->num_indent_level >= lex->alloc_indent_level) {
        lex->indent_level = m_renew(uint16_t, lex->indent_level, lex->alloc_indent_level, lex->alloc_indent_level + MICROPY_ALLOC_LEXEL_INDENT_INC);
        lex->alloc_indent_level += MICROPY_ALLOC_LEXEL_INDENT_INC;
    }
    lex->indent_level[lex->num_indent_level++] = indent;
}

static size_t indent_top(mp_lexer_t *lex) {
    return lex->indent_level[lex->num_indent_level - 1];
}

static void indent_pop(mp_lexer_t *lex) {
    lex->num_indent_level -= 1;
}







static const char *const tok_enc =
    "()[]{},;~"   
    ":e="         
    "<e=c<e="     
    ">e=c>e="     
    "*e=c*e="     
    "+e="         
    "-e=e>"       
    "&e="         
    "|e="         
    "/e=c/e="     
    "%e="         
    "^e="         
    "@e="         
    "=e="         
    "!.";         


static const uint8_t tok_enc_kind[] = {
    MP_TOKEN_DEL_PAREN_OPEN, MP_TOKEN_DEL_PAREN_CLOSE,
    MP_TOKEN_DEL_BRACKET_OPEN, MP_TOKEN_DEL_BRACKET_CLOSE,
    MP_TOKEN_DEL_BRACE_OPEN, MP_TOKEN_DEL_BRACE_CLOSE,
    MP_TOKEN_DEL_COMMA, MP_TOKEN_DEL_SEMICOLON, MP_TOKEN_OP_TILDE,

    MP_TOKEN_DEL_COLON, MP_TOKEN_OP_ASSIGN,
    MP_TOKEN_OP_LESS, MP_TOKEN_OP_LESS_EQUAL, MP_TOKEN_OP_DBL_LESS, MP_TOKEN_DEL_DBL_LESS_EQUAL,
    MP_TOKEN_OP_MORE, MP_TOKEN_OP_MORE_EQUAL, MP_TOKEN_OP_DBL_MORE, MP_TOKEN_DEL_DBL_MORE_EQUAL,
    MP_TOKEN_OP_STAR, MP_TOKEN_DEL_STAR_EQUAL, MP_TOKEN_OP_DBL_STAR, MP_TOKEN_DEL_DBL_STAR_EQUAL,
    MP_TOKEN_OP_PLUS, MP_TOKEN_DEL_PLUS_EQUAL,
    MP_TOKEN_OP_MINUS, MP_TOKEN_DEL_MINUS_EQUAL, MP_TOKEN_DEL_MINUS_MORE,
    MP_TOKEN_OP_AMPERSAND, MP_TOKEN_DEL_AMPERSAND_EQUAL,
    MP_TOKEN_OP_PIPE, MP_TOKEN_DEL_PIPE_EQUAL,
    MP_TOKEN_OP_SLASH, MP_TOKEN_DEL_SLASH_EQUAL, MP_TOKEN_OP_DBL_SLASH, MP_TOKEN_DEL_DBL_SLASH_EQUAL,
    MP_TOKEN_OP_PERCENT, MP_TOKEN_DEL_PERCENT_EQUAL,
    MP_TOKEN_OP_CARET, MP_TOKEN_DEL_CARET_EQUAL,
    MP_TOKEN_OP_AT, MP_TOKEN_DEL_AT_EQUAL,
    MP_TOKEN_DEL_EQUAL, MP_TOKEN_OP_DBL_EQUAL,
};



static const char *const tok_kw[] = {
    "False",
    "None",
    "True",
    "__debug__",
    "and",
    "as",
    "assert",
    #if MICROPY_PY_ASYNC_AWAIT
    "async",
    "await",
    #endif
    "break",
    "class",
    "continue",
    "def",
    "del",
    "elif",
    "else",
    "except",
    "finally",
    "for",
    "from",
    "global",
    "if",
    "import",
    "in",
    "is",
    "lambda",
    "nonlocal",
    "not",
    "or",
    "pass",
    "raise",
    "return",
    "try",
    "while",
    "with",
    "yield",
};




static bool get_hex(mp_lexer_t *lex, size_t num_digits, mp_uint_t *result) {
    mp_uint_t num = 0;
    while (num_digits-- != 0) {
        next_char(lex);
        unichar c = CUR_CHAR(lex);
        if (!unichar_isxdigit(c)) {
            return false;
        }
        num = (num << 4) + unichar_xdigit_value(c);
    }
    *result = num;
    return true;
}

static void parse_string_literal(mp_lexer_t *lex, bool is_raw, bool is_fstring) {
    
    char quote_char = '\'';
    if (is_char(lex, '\"')) {
        quote_char = '\"';
    }
    next_char(lex);

    
    size_t num_quotes;
    if (is_char_and(lex, quote_char, quote_char)) {
        
        next_char(lex);
        next_char(lex);
        num_quotes = 3;
    } else {
        
        num_quotes = 1;
    }

    size_t n_closing = 0;
    #if MICROPY_PY_FSTRINGS
    if (is_fstring) {
        
        
        
        
        vstr_add_str(&lex->fstring_args, ".format(");
    }
    #endif

    while (!is_end(lex) && (num_quotes > 1 || !is_char(lex, '\n')) && n_closing < num_quotes) {
        if (is_char(lex, quote_char)) {
            n_closing += 1;
            vstr_add_char(&lex->vstr, CUR_CHAR(lex));
        } else {
            n_closing = 0;

            #if MICROPY_PY_FSTRINGS
            while (is_fstring && is_char(lex, '{')) {
                next_char(lex);
                if (is_char(lex, '{')) {
                    
                    vstr_add_byte(&lex->vstr, '{');
                    next_char(lex);
                } else {
                    
                    
                    vstr_add_byte(&lex->fstring_args, '(');
                    
                    size_t i = lex->fstring_args.len;
                    
                    
                    
                    
                    
                    
                    
                    
                    
                    
                    
                    
                    unsigned int nested_bracket_level = 0;
                    while (!is_end(lex) && (nested_bracket_level != 0
                                            || !(is_char_or(lex, ':', '}')
                                                 || (is_char(lex, '!')
                                                     && is_char_following_or(lex, 'r', 's')
                                                     && is_char_following_following_or(lex, ':', '}'))))
                           ) {
                        unichar c = CUR_CHAR(lex);
                        if (c == '[' || c == '{') {
                            nested_bracket_level += 1;
                        } else if (c == ']' || c == '}') {
                            nested_bracket_level -= 1;
                        }
                        
                        vstr_add_byte(&lex->fstring_args, c);
                        next_char(lex);
                    }
                    if (lex->fstring_args.buf[lex->fstring_args.len - 1] == '=') {
                        
                        
                        vstr_add_strn(&lex->vstr, lex->fstring_args.buf + i, lex->fstring_args.len - i);
                        
                        lex->fstring_args.len--;
                    }
                    
                    vstr_add_byte(&lex->fstring_args, ')');
                    
                    vstr_add_byte(&lex->fstring_args, ',');
                }
                vstr_add_byte(&lex->vstr, '{');
            }
            #endif

            if (is_char(lex, '\\')) {
                next_char(lex);
                unichar c = CUR_CHAR(lex);
                if (is_raw) {
                    
                    vstr_add_char(&lex->vstr, '\\');
                } else {
                    switch (c) {
                        
                        
                        case '\n':
                            c = MP_LEXER_EOF;
                            break;                          
                        case '\\':
                            break;
                        case '\'':
                            break;
                        case '"':
                            break;
                        case 'a':
                            c = 0x07;
                            break;
                        case 'b':
                            c = 0x08;
                            break;
                        case 't':
                            c = 0x09;
                            break;
                        case 'n':
                            c = 0x0a;
                            break;
                        case 'v':
                            c = 0x0b;
                            break;
                        case 'f':
                            c = 0x0c;
                            break;
                        case 'r':
                            c = 0x0d;
                            break;
                        case 'u':
                        case 'U':
                            if (lex->tok_kind == MP_TOKEN_BYTES) {
                                
                                vstr_add_char(&lex->vstr, '\\');
                                break;
                            }
                            
                            MP_FALLTHROUGH
                        case 'x': {
                            mp_uint_t num = 0;
                            if (!get_hex(lex, (c == 'x' ? 2 : c == 'u' ? 4 : 8), &num)) {
                                
                                lex->tok_kind = MP_TOKEN_INVALID;
                            }
                            c = num;
                            break;
                        }
                        case 'N':
                            
                            
                            
                            
                            
                            mp_raise_NotImplementedError(MP_ERROR_TEXT("unicode name escapes"));
                            break;
                        default:
                            if (c >= '0' && c <= '7') {
                                
                                size_t digits = 3;
                                mp_uint_t num = c - '0';
                                while (is_following_odigit(lex) && --digits != 0) {
                                    next_char(lex);
                                    num = num * 8 + (CUR_CHAR(lex) - '0');
                                }
                                c = num;
                            } else {
                                
                                vstr_add_char(&lex->vstr, '\\');
                            }
                            break;
                    }
                }
                if (c != MP_LEXER_EOF) {
                    #if MICROPY_PY_BUILTINS_STR_UNICODE
                    if (c < 0x110000 && lex->tok_kind == MP_TOKEN_STRING) {
                        
                        vstr_add_char(&lex->vstr, c);
                    } else if (c < 0x100 && lex->tok_kind == MP_TOKEN_BYTES) {
                        
                        vstr_add_byte(&lex->vstr, c);
                    }
                    #else
                    if (c < 0x100) {
                        
                        vstr_add_byte(&lex->vstr, c);
                    }
                    #endif
                    else {
                        
                        lex->tok_kind = MP_TOKEN_INVALID;
                    }
                }
            } else {
                
                
                vstr_add_byte(&lex->vstr, CUR_CHAR(lex));
            }
        }
        next_char(lex);
    }

    
    if (n_closing < num_quotes) {
        lex->tok_kind = MP_TOKEN_LONELY_STRING_OPEN;
    }

    
    vstr_cut_tail_bytes(&lex->vstr, n_closing);
}



static bool skip_whitespace(mp_lexer_t *lex, bool stop_at_newline) {
    while (!is_end(lex)) {
        if (is_physical_newline(lex)) {
            if (stop_at_newline && lex->nested_bracket_level == 0) {
                return true;
            }
            next_char(lex);
        } else if (is_whitespace(lex)) {
            next_char(lex);
        } else if (is_char(lex, '#')) {
            next_char(lex);
            while (!is_end(lex) && !is_physical_newline(lex)) {
                next_char(lex);
            }
            
        } else if (is_char_and(lex, '\\', '\n')) {
            
            next_char(lex);
            next_char(lex);
        } else {
            break;
        }
    }
    return false;
}

void mp_lexer_to_next(mp_lexer_t *lex) {
    #if MICROPY_PY_FSTRINGS
    if (lex->fstring_args.len && lex->fstring_args_idx == 0) {
        
        
        vstr_add_byte(&lex->fstring_args, ')');
        lex->chr0_saved = lex->chr0;
        lex->chr1_saved = lex->chr1;
        lex->chr2_saved = lex->chr2;
        lex->chr0 = lex->fstring_args.buf[0];
        lex->chr1 = lex->fstring_args.buf[1];
        lex->chr2 = lex->fstring_args.buf[2];
        
        
        lex->fstring_args_idx = 3;
    }
    #endif

    
    vstr_reset(&lex->vstr);

    
    
    
    
    bool had_physical_newline = skip_whitespace(lex, true);

    
    lex->tok_line = lex->line;
    lex->tok_column = lex->column;

    if (lex->emit_dent < 0) {
        lex->tok_kind = MP_TOKEN_DEDENT;
        lex->emit_dent += 1;

    } else if (lex->emit_dent > 0) {
        lex->tok_kind = MP_TOKEN_INDENT;
        lex->emit_dent -= 1;

    } else if (had_physical_newline) {
        
        
        
        skip_whitespace(lex, false);

        lex->tok_kind = MP_TOKEN_NEWLINE;

        size_t num_spaces = lex->column - 1;
        if (num_spaces == indent_top(lex)) {
        } else if (num_spaces > indent_top(lex)) {
            indent_push(lex, num_spaces);
            lex->emit_dent += 1;
        } else {
            while (num_spaces < indent_top(lex)) {
                indent_pop(lex);
                lex->emit_dent -= 1;
            }
            if (num_spaces != indent_top(lex)) {
                lex->tok_kind = MP_TOKEN_DEDENT_MISMATCH;
            }
        }

    } else if (is_end(lex)) {
        lex->tok_kind = MP_TOKEN_END;

    } else if (is_string_or_bytes(lex)) {
        

        
        
        
        
        

        
        lex->tok_kind = MP_TOKEN_END;

        
        do {
            
            bool is_raw = false;
            bool is_fstring = false;
            mp_token_kind_t kind = MP_TOKEN_STRING;
            int n_char = 0;
            if (is_char(lex, 'u')) {
                n_char = 1;
            } else if (is_char(lex, 'b')) {
                kind = MP_TOKEN_BYTES;
                n_char = 1;
                if (is_char_following(lex, 'r')) {
                    is_raw = true;
                    n_char = 2;
                }
            } else if (is_char(lex, 'r')) {
                is_raw = true;
                n_char = 1;
                if (is_char_following(lex, 'b')) {
                    kind = MP_TOKEN_BYTES;
                    n_char = 2;
                }
                #if MICROPY_PY_FSTRINGS
                if (is_char_following(lex, 'f')) {
                    
                    lex->tok_kind = MP_TOKEN_FSTRING_RAW;
                    break;
                }
                #endif
            }
            #if MICROPY_PY_FSTRINGS
            else if (is_char(lex, 'f')) {
                if (is_char_following(lex, 'r')) {
                    
                    lex->tok_kind = MP_TOKEN_FSTRING_RAW;
                    break;
                }
                n_char = 1;
                is_fstring = true;
            }
            #endif

            
            if (lex->tok_kind == MP_TOKEN_END) {
                lex->tok_kind = kind;
            } else if (lex->tok_kind != kind) {
                
                break;
            }

            
            if (n_char != 0) {
                next_char(lex);
                if (n_char == 2) {
                    next_char(lex);
                }
            }

            
            parse_string_literal(lex, is_raw, is_fstring);

            
            skip_whitespace(lex, true);

        } while (is_string_or_bytes(lex));

    } else if (is_head_of_identifier(lex)) {
        lex->tok_kind = MP_TOKEN_NAME;

        
        vstr_add_byte(&lex->vstr, CUR_CHAR(lex));
        next_char(lex);

        
        while (!is_end(lex) && is_tail_of_identifier(lex)) {
            vstr_add_byte(&lex->vstr, CUR_CHAR(lex));
            next_char(lex);
        }

        
        
        
        
        const char *s = vstr_null_terminated_str(&lex->vstr);
        for (size_t i = 0; i < MP_ARRAY_SIZE(tok_kw); i++) {
            int cmp = strcmp(s, tok_kw[i]);
            if (cmp == 0) {
                lex->tok_kind = MP_TOKEN_KW_FALSE + i;
                if (lex->tok_kind == MP_TOKEN_KW___DEBUG__) {
                    lex->tok_kind = (MP_STATE_VM(mp_optimise_value) == 0 ? MP_TOKEN_KW_TRUE : MP_TOKEN_KW_FALSE);
                }
                break;
            } else if (cmp < 0) {
                
                break;
            }
        }

    } else if (is_digit(lex) || (is_char(lex, '.') && is_following_digit(lex))) {
        bool forced_integer = false;
        if (is_char(lex, '.')) {
            lex->tok_kind = MP_TOKEN_FLOAT_OR_IMAG;
        } else {
            lex->tok_kind = MP_TOKEN_INTEGER;
            if (is_char(lex, '0') && is_following_base_char(lex)) {
                forced_integer = true;
            }
        }

        
        vstr_add_char(&lex->vstr, CUR_CHAR(lex));
        next_char(lex);

        
        while (!is_end(lex)) {
            if (!forced_integer && is_char_or(lex, 'e', 'E')) {
                lex->tok_kind = MP_TOKEN_FLOAT_OR_IMAG;
                vstr_add_char(&lex->vstr, 'e');
                next_char(lex);
                if (is_char(lex, '+') || is_char(lex, '-')) {
                    vstr_add_char(&lex->vstr, CUR_CHAR(lex));
                    next_char(lex);
                }
            } else if (is_letter(lex) || is_digit(lex) || is_char(lex, '.')) {
                if (is_char_or3(lex, '.', 'j', 'J')) {
                    lex->tok_kind = MP_TOKEN_FLOAT_OR_IMAG;
                }
                vstr_add_char(&lex->vstr, CUR_CHAR(lex));
                next_char(lex);
            } else if (is_char(lex, '_')) {
                next_char(lex);
            } else {
                break;
            }
        }

    } else {
        

        const char *t = tok_enc;
        size_t tok_enc_index = 0;
        for (; *t != 0 && !is_char(lex, *t); t += 1) {
            if (*t == 'e' || *t == 'c') {
                t += 1;
            }
            tok_enc_index += 1;
        }

        next_char(lex);

        if (*t == 0) {
            
            lex->tok_kind = MP_TOKEN_INVALID;

        } else if (*t == '!') {
            
            if (is_char(lex, '=')) {
                next_char(lex);
                lex->tok_kind = MP_TOKEN_OP_NOT_EQUAL;
            } else {
                lex->tok_kind = MP_TOKEN_INVALID;
            }

        } else if (*t == '.') {
            
            if (is_char_and(lex, '.', '.')) {
                next_char(lex);
                next_char(lex);
                lex->tok_kind = MP_TOKEN_ELLIPSIS;
            } else {
                lex->tok_kind = MP_TOKEN_DEL_PERIOD;
            }

        } else {
            

            
            t += 1;
            size_t t_index = tok_enc_index;
            while (*t == 'c' || *t == 'e') {
                t_index += 1;
                if (is_char(lex, t[1])) {
                    next_char(lex);
                    tok_enc_index = t_index;
                    if (*t == 'e') {
                        break;
                    }
                } else if (*t == 'c') {
                    break;
                }
                t += 2;
            }

            
            lex->tok_kind = tok_enc_kind[tok_enc_index];

            
            if (lex->tok_kind == MP_TOKEN_DEL_PAREN_OPEN || lex->tok_kind == MP_TOKEN_DEL_BRACKET_OPEN || lex->tok_kind == MP_TOKEN_DEL_BRACE_OPEN) {
                lex->nested_bracket_level += 1;
            } else if (lex->tok_kind == MP_TOKEN_DEL_PAREN_CLOSE || lex->tok_kind == MP_TOKEN_DEL_BRACKET_CLOSE || lex->tok_kind == MP_TOKEN_DEL_BRACE_CLOSE) {
                lex->nested_bracket_level -= 1;
            }
        }
    }
}

mp_lexer_t *mp_lexer_new(qstr src_name, mp_reader_t reader) {
    mp_lexer_t *lex = m_new_obj(mp_lexer_t);

    lex->source_name = src_name;
    lex->reader = reader;
    lex->line = 1;
    lex->column = (size_t)-2; 
    lex->emit_dent = 0;
    lex->nested_bracket_level = 0;
    lex->alloc_indent_level = MICROPY_ALLOC_LEXER_INDENT_INIT;
    lex->num_indent_level = 1;
    lex->indent_level = m_new(uint16_t, lex->alloc_indent_level);
    vstr_init(&lex->vstr, 32);
    #if MICROPY_PY_FSTRINGS
    vstr_init(&lex->fstring_args, 0);
    lex->fstring_args_idx = 0;
    #endif

    
    lex->indent_level[0] = 0;

    
    
    lex->chr0 = lex->chr1 = lex->chr2 = 0;
    next_char(lex);
    next_char(lex);
    next_char(lex);

    
    mp_lexer_to_next(lex);

    
    
    
    if (lex->tok_column != 1 && lex->tok_kind != MP_TOKEN_NEWLINE) {
        lex->tok_kind = MP_TOKEN_INDENT;
    }

    return lex;
}

mp_lexer_t *mp_lexer_new_from_str_len(qstr src_name, const char *str, size_t len, size_t free_len) {
    mp_reader_t reader;
    mp_reader_new_mem(&reader, (const byte *)str, len, free_len);
    return mp_lexer_new(src_name, reader);
}

#if MICROPY_READER_POSIX || MICROPY_READER_VFS

mp_lexer_t *mp_lexer_new_from_file(qstr filename) {
    mp_reader_t reader;
    mp_reader_new_file(&reader, filename);
    return mp_lexer_new(filename, reader);
}

#if MICROPY_HELPER_LEXER_UNIX

mp_lexer_t *mp_lexer_new_from_fd(qstr filename, int fd, bool close_fd) {
    mp_reader_t reader;
    mp_reader_new_file_from_fd(&reader, fd, close_fd);
    return mp_lexer_new(filename, reader);
}

#endif

#endif

void mp_lexer_free(mp_lexer_t *lex) {
    if (lex) {
        lex->reader.close(lex->reader.data);
        vstr_clear(&lex->vstr);
        #if MICROPY_PY_FSTRINGS
        vstr_clear(&lex->fstring_args);
        #endif
        m_del(uint16_t, lex->indent_level, lex->alloc_indent_level);
        m_del_obj(mp_lexer_t, lex);
    }
}

#if 0


void mp_lexer_show_token(const mp_lexer_t *lex) {
    printf("(" UINT_FMT ":" UINT_FMT ") kind:%u str:%p len:%zu", lex->tok_line, lex->tok_column, lex->tok_kind, lex->vstr.buf, lex->vstr.len);
    if (lex->vstr.len > 0) {
        const byte *i = (const byte *)lex->vstr.buf;
        const byte *j = (const byte *)i + lex->vstr.len;
        printf(" ");
        while (i < j) {
            unichar c = utf8_get_char(i);
            i = utf8_next_char(i);
            if (unichar_isprint(c)) {
                printf("%c", (int)c);
            } else {
                printf("?");
            }
        }
    }
    printf("\n");
}
#endif

#endif 
