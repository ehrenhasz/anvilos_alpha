 

#include <string.h>
#include "py/obj.h"
#include "py/objmodule.h"
#include "py/runtime.h"
#include "py/builtin.h"
#include "py/repl.h"

#if MICROPY_HELPER_REPL

#if MICROPY_PY_SYS_PS1_PS2
const char *mp_repl_get_psx(unsigned int entry) {
    if (mp_obj_is_str(MP_STATE_VM(sys_mutable)[entry])) {
        return mp_obj_str_get_str(MP_STATE_VM(sys_mutable)[entry]);
    } else {
        return "";
    }
}
#endif

static bool str_startswith_word(const char *str, const char *head) {
    size_t i;
    for (i = 0; str[i] && head[i]; i++) {
        if (str[i] != head[i]) {
            return false;
        }
    }
    return head[i] == '\0' && (str[i] == '\0' || !unichar_isident(str[i]));
}

bool mp_repl_continue_with_input(const char *input) {
    
    if (input[0] == '\0') {
        return false;
    }

    
    bool starts_with_compound_keyword =
        input[0] == '@'
        || str_startswith_word(input, "if")
        || str_startswith_word(input, "while")
        || str_startswith_word(input, "for")
        || str_startswith_word(input, "try")
        || str_startswith_word(input, "with")
        || str_startswith_word(input, "def")
        || str_startswith_word(input, "class")
        #if MICROPY_PY_ASYNC_AWAIT
        || str_startswith_word(input, "async")
        #endif
    ;

    
    #define Q_NONE (0)
    #define Q_1_SINGLE (1)
    #define Q_1_DOUBLE (2)
    #define Q_3_SINGLE (3)
    #define Q_3_DOUBLE (4)
    int n_paren = 0;
    int n_brack = 0;
    int n_brace = 0;
    int in_quote = Q_NONE;
    const char *i;
    for (i = input; *i; i++) {
        if (*i == '\'') {
            if ((in_quote == Q_NONE || in_quote == Q_3_SINGLE) && i[1] == '\'' && i[2] == '\'') {
                i += 2;
                in_quote = Q_3_SINGLE - in_quote;
            } else if (in_quote == Q_NONE || in_quote == Q_1_SINGLE) {
                in_quote = Q_1_SINGLE - in_quote;
            }
        } else if (*i == '"') {
            if ((in_quote == Q_NONE || in_quote == Q_3_DOUBLE) && i[1] == '"' && i[2] == '"') {
                i += 2;
                in_quote = Q_3_DOUBLE - in_quote;
            } else if (in_quote == Q_NONE || in_quote == Q_1_DOUBLE) {
                in_quote = Q_1_DOUBLE - in_quote;
            }
        } else if (*i == '\\' && (i[1] == '\'' || i[1] == '"' || i[1] == '\\')) {
            if (in_quote != Q_NONE) {
                i++;
            }
        } else if (in_quote == Q_NONE) {
            switch (*i) {
                case '(':
                    n_paren += 1;
                    break;
                case ')':
                    n_paren -= 1;
                    break;
                case '[':
                    n_brack += 1;
                    break;
                case ']':
                    n_brack -= 1;
                    break;
                case '{':
                    n_brace += 1;
                    break;
                case '}':
                    n_brace -= 1;
                    break;
                default:
                    break;
            }
        }
    }

    
    if (in_quote == Q_3_SINGLE || in_quote == Q_3_DOUBLE) {
        return true;
    }

    
    if ((n_paren > 0 || n_brack > 0 || n_brace > 0) && in_quote == Q_NONE) {
        return true;
    }

    
    if (i[-1] == '\\') {
        return true;
    }

    
    if (starts_with_compound_keyword && i[-1] != '\n') {
        return true;
    }

    
    return false;
}

static bool test_qstr(mp_obj_t obj, qstr name) {
    if (obj) {
        
        mp_obj_t dest[2];
        mp_load_method_protected(obj, name, dest, true);
        return dest[0] != MP_OBJ_NULL;
    } else {
        
        return mp_map_lookup((mp_map_t *)&mp_builtin_module_map, MP_OBJ_NEW_QSTR(name), MP_MAP_LOOKUP) ||
               mp_map_lookup((mp_map_t *)&mp_builtin_extensible_module_map, MP_OBJ_NEW_QSTR(name), MP_MAP_LOOKUP);
    }
}

static const char *find_completions(const char *s_start, size_t s_len,
    mp_obj_t obj, size_t *match_len, qstr *q_first, qstr *q_last) {

    const char *match_str = NULL;
    *match_len = 0;
    *q_first = *q_last = 0;
    size_t nqstr = QSTR_TOTAL();
    for (qstr q = MP_QSTR_ + 1; q < nqstr; ++q) {
        size_t d_len;
        const char *d_str = (const char *)qstr_data(q, &d_len);
        
        
        if (s_len == 0 && d_str[0] == '_') {
            continue;
        }
        if (s_len <= d_len && strncmp(s_start, d_str, s_len) == 0) {
            if (test_qstr(obj, q)) {
                if (match_str == NULL) {
                    match_str = d_str;
                    *match_len = d_len;
                } else {
                    
                    
                    for (size_t j = s_len; j <= *match_len && j <= d_len; ++j) {
                        if (match_str[j] != d_str[j]) {
                            *match_len = j;
                            break;
                        }
                    }
                }
                if (*q_first == 0) {
                    *q_first = q;
                }
                *q_last = q;
            }
        }
    }
    return match_str;
}

static void print_completions(const mp_print_t *print,
    const char *s_start, size_t s_len,
    mp_obj_t obj, qstr q_first, qstr q_last) {

    #define WORD_SLOT_LEN (16)
    #define MAX_LINE_LEN  (4 * WORD_SLOT_LEN)

    int line_len = MAX_LINE_LEN; 
    for (qstr q = q_first; q <= q_last; ++q) {
        size_t d_len;
        const char *d_str = (const char *)qstr_data(q, &d_len);
        if (s_len <= d_len && strncmp(s_start, d_str, s_len) == 0) {
            if (test_qstr(obj, q)) {
                int gap = (line_len + WORD_SLOT_LEN - 1) / WORD_SLOT_LEN * WORD_SLOT_LEN - line_len;
                if (gap < 2) {
                    gap += WORD_SLOT_LEN;
                }
                if (line_len + gap + d_len <= MAX_LINE_LEN) {
                    
                    for (int j = 0; j < gap; ++j) {
                        mp_print_str(print, " ");
                    }
                    mp_print_str(print, d_str);
                    line_len += gap + d_len;
                } else {
                    mp_printf(print, "\n%s", d_str);
                    line_len = d_len;
                }
            }
        }
    }
    mp_print_str(print, "\n");
}

size_t mp_repl_autocomplete(const char *str, size_t len, const mp_print_t *print, const char **compl_str) {
    
    const char *org_str = str;
    const char *top = str + len;
    for (const char *s = top; --s >= str;) {
        if (!(unichar_isalpha(*s) || unichar_isdigit(*s) || *s == '_' || *s == '.')) {
            ++s;
            str = s;
            break;
        }
    }

    
    mp_obj_t obj = MP_OBJ_FROM_PTR(&mp_module___main__);
    mp_obj_t dest[2];

    const char *s_start;
    size_t s_len;

    for (;;) {
        
        s_start = str;
        while (str < top && *str != '.') {
            ++str;
        }
        s_len = str - s_start;

        if (str == top) {
            
            break;
        }

        
        qstr q = qstr_find_strn(s_start, s_len);
        if (q == MP_QSTRnull) {
            
            return 0;
        }
        mp_load_method_protected(obj, q, dest, true);
        obj = dest[0]; 

        if (obj == MP_OBJ_NULL) {
            
            return 0;
        }

        
        ++str;
    }

    
    static const char import_str[] = "import ";
    if (len >= 7 && !memcmp(org_str, import_str, 7)) {
        obj = MP_OBJ_NULL;
    }

    
    size_t match_len;
    qstr q_first, q_last;
    const char *match_str =
        find_completions(s_start, s_len, obj, &match_len, &q_first, &q_last);

    
    if (q_first == 0) {
        
        
        if (s_start == org_str && s_len > 0 && s_len < sizeof(import_str) - 1) {
            if (memcmp(s_start, import_str, s_len) == 0) {
                *compl_str = import_str + s_len;
                return sizeof(import_str) - 1 - s_len;
            }
        }
        return 0;
    }

    
    if (q_first == q_last || match_len > s_len) {
        *compl_str = match_str + s_len;
        return match_len - s_len;
    }

    
    print_completions(print, s_start, s_len, obj, q_first, q_last);

    return (size_t)(-1); 
}

#endif 
