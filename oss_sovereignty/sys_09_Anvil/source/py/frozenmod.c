 

#include <string.h>
#include <stdint.h>

#include "py/lexer.h"
#include "py/frozenmod.h"

#if MICROPY_MODULE_FROZEN




extern const char mp_frozen_names[];

#if MICROPY_MODULE_FROZEN_STR

#ifndef MICROPY_MODULE_FROZEN_LEXER
#define MICROPY_MODULE_FROZEN_LEXER mp_lexer_new_from_str_len
#else
mp_lexer_t *MICROPY_MODULE_FROZEN_LEXER(qstr src_name, const char *str, mp_uint_t len, mp_uint_t free_len);
#endif


extern const uint32_t mp_frozen_str_sizes[];

extern const char mp_frozen_str_content[];
#endif 

#if MICROPY_MODULE_FROZEN_MPY

#include "py/emitglue.h"

extern const mp_frozen_module_t *const mp_frozen_mpy_content[];

#endif 




mp_import_stat_t mp_find_frozen_module(const char *str, int *frozen_type, void **data) {
    size_t len = strlen(str);
    const char *name = mp_frozen_names;

    if (frozen_type != NULL) {
        *frozen_type = MP_FROZEN_NONE;
    }

    
    size_t num_str = 0;
    #if MICROPY_MODULE_FROZEN_STR && MICROPY_MODULE_FROZEN_MPY
    for (const uint32_t *s = mp_frozen_str_sizes; *s != 0; ++s) {
        ++num_str;
    }
    #endif

    for (size_t i = 0; *name != 0; i++) {
        size_t entry_len = strlen(name);
        if (entry_len >= len && memcmp(str, name, len) == 0) {
            
            if (entry_len == len) {
                

                if (frozen_type != NULL) {
                    #if MICROPY_MODULE_FROZEN_STR
                    if (i < num_str) {
                        *frozen_type = MP_FROZEN_STR;
                        
                        size_t offset = 0;
                        for (size_t j = 0; j < i; ++j) {
                            offset += mp_frozen_str_sizes[j] + 1;
                        }
                        size_t content_len = mp_frozen_str_sizes[i];
                        const char *content = &mp_frozen_str_content[offset];

                        
                        
                        
                        qstr source = qstr_from_strn(str, len);
                        mp_lexer_t *lex = MICROPY_MODULE_FROZEN_LEXER(source, content, content_len, 0);
                        *data = lex;
                    }
                    #endif

                    #if MICROPY_MODULE_FROZEN_MPY
                    if (i >= num_str) {
                        *frozen_type = MP_FROZEN_MPY;
                        
                        
                        *data = (void *)mp_frozen_mpy_content[i - num_str];
                    }
                    #endif
                }

                return MP_IMPORT_STAT_FILE;
            } else if (name[len] == '/') {
                
                
                return MP_IMPORT_STAT_DIR;
            }
        }
        
        name += entry_len + 1;
    }

    return MP_IMPORT_STAT_NO_EXIST;
}

#endif 
