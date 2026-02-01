 
#ifndef MICROPY_INCLUDED_PY_COMPILE_H
#define MICROPY_INCLUDED_PY_COMPILE_H

#include "py/lexer.h"
#include "py/parse.h"
#include "py/emitglue.h"

#if MICROPY_COMP_ALLOW_TOP_LEVEL_AWAIT

extern bool mp_compile_allow_top_level_await;
#endif




mp_obj_t mp_compile(mp_parse_tree_t *parse_tree, qstr source_file, bool is_repl);

#if MICROPY_PERSISTENT_CODE_SAVE

void mp_compile_to_raw_code(mp_parse_tree_t *parse_tree, qstr source_file, bool is_repl, mp_compiled_module_t *cm);
#endif


mp_obj_t mp_parse_compile_execute(mp_lexer_t *lex, mp_parse_input_kind_t parse_input_kind, mp_obj_dict_t *globals, mp_obj_dict_t *locals);

#endif 
