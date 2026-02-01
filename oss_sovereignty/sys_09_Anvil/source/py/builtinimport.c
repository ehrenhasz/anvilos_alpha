 

#include <stdio.h>
#include <string.h>
#include <assert.h>

#include "py/compile.h"
#include "py/objmodule.h"
#include "py/persistentcode.h"
#include "py/runtime.h"
#include "py/builtin.h"
#include "py/frozenmod.h"

#if MICROPY_DEBUG_VERBOSE 
#define DEBUG_PRINT (1)
#define DEBUG_printf DEBUG_printf
#else 
#define DEBUG_PRINT (0)
#define DEBUG_printf(...) (void)0
#endif

#if MICROPY_ENABLE_EXTERNAL_IMPORT


#define PATH_SEP_CHAR "/"


#define MP_FROZEN_PATH_PREFIX ".frozen/"





static mp_import_stat_t stat_path(vstr_t *path) {
    const char *str = vstr_null_terminated_str(path);
    #if MICROPY_MODULE_FROZEN
    
    const int frozen_path_prefix_len = strlen(MP_FROZEN_PATH_PREFIX);
    if (strncmp(str, MP_FROZEN_PATH_PREFIX, frozen_path_prefix_len) == 0) {
        
        return mp_find_frozen_module(str + frozen_path_prefix_len, NULL, NULL);
    }
    #endif
    return mp_import_stat(str);
}






static mp_import_stat_t stat_file_py_or_mpy(vstr_t *path) {
    mp_import_stat_t stat = stat_path(path);
    if (stat == MP_IMPORT_STAT_FILE) {
        return stat;
    }

    #if MICROPY_PERSISTENT_CODE_LOAD
    
    
    
    vstr_ins_byte(path, path->len - 2, 'm');
    stat = stat_path(path);
    if (stat == MP_IMPORT_STAT_FILE) {
        return stat;
    }
    #endif

    return MP_IMPORT_STAT_NO_EXIST;
}





static mp_import_stat_t stat_module(vstr_t *path) {
    mp_import_stat_t stat = stat_path(path);
    DEBUG_printf("stat %s: %d\n", vstr_str(path), stat);
    if (stat == MP_IMPORT_STAT_DIR) {
        return stat;
    }

    
    vstr_add_str(path, ".py");
    return stat_file_py_or_mpy(path);
}




static mp_import_stat_t stat_top_level(qstr mod_name, vstr_t *dest) {
    DEBUG_printf("stat_top_level: '%s'\n", qstr_str(mod_name));
    #if MICROPY_PY_SYS
    size_t path_num;
    mp_obj_t *path_items;
    mp_obj_get_array(mp_sys_path, &path_num, &path_items);

    
    for (size_t i = 0; i < path_num; i++) {
        vstr_reset(dest);
        size_t p_len;
        const char *p = mp_obj_str_get_data(path_items[i], &p_len);
        if (p_len > 0) {
            
            vstr_add_strn(dest, p, p_len);
            vstr_add_char(dest, PATH_SEP_CHAR[0]);
        }
        vstr_add_str(dest, qstr_str(mod_name));
        mp_import_stat_t stat = stat_module(dest);
        if (stat != MP_IMPORT_STAT_NO_EXIST) {
            return stat;
        }
    }

    
    
    return MP_IMPORT_STAT_NO_EXIST;

    #else

    
    vstr_add_str(dest, qstr_str(mod_name));
    return stat_module(dest);

    #endif
}

#if MICROPY_MODULE_FROZEN_STR || MICROPY_ENABLE_COMPILER
static void do_load_from_lexer(mp_module_context_t *context, mp_lexer_t *lex) {
    #if MICROPY_PY___FILE__
    qstr source_name = lex->source_name;
    mp_store_attr(MP_OBJ_FROM_PTR(&context->module), MP_QSTR___file__, MP_OBJ_NEW_QSTR(source_name));
    #endif

    
    mp_obj_dict_t *mod_globals = context->module.globals;
    mp_parse_compile_execute(lex, MP_PARSE_FILE_INPUT, mod_globals, mod_globals);
}
#endif

#if (MICROPY_HAS_FILE_READER && MICROPY_PERSISTENT_CODE_LOAD) || MICROPY_MODULE_FROZEN_MPY
static void do_execute_proto_fun(const mp_module_context_t *context, mp_proto_fun_t proto_fun, qstr source_name) {
    #if MICROPY_PY___FILE__
    mp_store_attr(MP_OBJ_FROM_PTR(&context->module), MP_QSTR___file__, MP_OBJ_NEW_QSTR(source_name));
    #else
    (void)source_name;
    #endif

    
    mp_obj_dict_t *mod_globals = context->module.globals;

    
    nlr_jump_callback_node_globals_locals_t ctx;
    ctx.globals = mp_globals_get();
    ctx.locals = mp_locals_get();

    
    mp_globals_set(mod_globals);
    mp_locals_set(mod_globals);

    
    nlr_push_jump_callback(&ctx.callback, mp_globals_locals_set_from_nlr_jump_callback);

    
    mp_obj_t module_fun = mp_make_function_from_proto_fun(proto_fun, context, NULL);
    mp_call_function_0(module_fun);

    
    nlr_pop_jump_callback(true);
}
#endif

static void do_load(mp_module_context_t *module_obj, vstr_t *file) {
    #if MICROPY_MODULE_FROZEN || MICROPY_ENABLE_COMPILER || (MICROPY_PERSISTENT_CODE_LOAD && MICROPY_HAS_FILE_READER)
    const char *file_str = vstr_null_terminated_str(file);
    #endif

    
    
    #if MICROPY_MODULE_FROZEN
    void *modref;
    int frozen_type;
    const int frozen_path_prefix_len = strlen(MP_FROZEN_PATH_PREFIX);
    if (strncmp(file_str, MP_FROZEN_PATH_PREFIX, frozen_path_prefix_len) == 0) {
        mp_find_frozen_module(file_str + frozen_path_prefix_len, &frozen_type, &modref);

        
        
        #if MICROPY_MODULE_FROZEN_STR
        if (frozen_type == MP_FROZEN_STR) {
            do_load_from_lexer(module_obj, modref);
            return;
        }
        #endif

        
        
        #if MICROPY_MODULE_FROZEN_MPY
        if (frozen_type == MP_FROZEN_MPY) {
            const mp_frozen_module_t *frozen = modref;
            module_obj->constants = frozen->constants;
            #if MICROPY_PY___FILE__
            qstr frozen_file_qstr = qstr_from_str(file_str + frozen_path_prefix_len);
            #else
            qstr frozen_file_qstr = MP_QSTRnull;
            #endif
            do_execute_proto_fun(module_obj, frozen->proto_fun, frozen_file_qstr);
            return;
        }
        #endif
    }

    #endif 

    qstr file_qstr = qstr_from_str(file_str);

    
    
    #if MICROPY_HAS_FILE_READER && MICROPY_PERSISTENT_CODE_LOAD
    if (file_str[file->len - 3] == 'm') {
        mp_compiled_module_t cm;
        cm.context = module_obj;
        mp_raw_code_load_file(file_qstr, &cm);
        do_execute_proto_fun(cm.context, cm.rc, file_qstr);
        return;
    }
    #endif

    
    #if MICROPY_ENABLE_COMPILER
    {
        mp_lexer_t *lex = mp_lexer_new_from_file(file_qstr);
        do_load_from_lexer(module_obj, lex);
        return;
    }
    #else
    
    mp_raise_msg(&mp_type_ImportError, MP_ERROR_TEXT("script compilation not supported"));
    #endif
}



static void evaluate_relative_import(mp_int_t level, const char **module_name, size_t *module_name_len) {
    
    
    
    
    
    
    

    mp_obj_t current_module_name_obj = mp_obj_dict_get(MP_OBJ_FROM_PTR(mp_globals_get()), MP_OBJ_NEW_QSTR(MP_QSTR___name__));
    assert(current_module_name_obj != MP_OBJ_NULL);

    #if MICROPY_MODULE_OVERRIDE_MAIN_IMPORT && MICROPY_CPYTHON_COMPAT
    if (MP_OBJ_QSTR_VALUE(current_module_name_obj) == MP_QSTR___main__) {
        
        
        
        current_module_name_obj = mp_obj_dict_get(MP_OBJ_FROM_PTR(mp_globals_get()), MP_OBJ_NEW_QSTR(MP_QSTR___main__));
    }
    #endif

    
    bool is_pkg = mp_map_lookup(&mp_globals_get()->map, MP_OBJ_NEW_QSTR(MP_QSTR___path__), MP_MAP_LOOKUP);

    #if DEBUG_PRINT
    DEBUG_printf("Current module/package: ");
    mp_obj_print_helper(MICROPY_DEBUG_PRINTER, current_module_name_obj, PRINT_REPR);
    DEBUG_printf(", is_package: %d", is_pkg);
    DEBUG_printf("\n");
    #endif

    size_t current_module_name_len;
    const char *current_module_name = mp_obj_str_get_data(current_module_name_obj, &current_module_name_len);

    const char *p = current_module_name + current_module_name_len;
    if (is_pkg) {
        
        
        
        --level;
    }

    
    while (level && p > current_module_name) {
        if (*--p == '.') {
            --level;
        }
    }

    
    if (p == current_module_name) {
        mp_raise_msg(&mp_type_ImportError, MP_ERROR_TEXT("can't perform relative import"));
    }

    
    
    
    uint new_module_name_len = (size_t)(p - current_module_name) + 1 + *module_name_len;
    char *new_mod = mp_local_alloc(new_module_name_len);
    memcpy(new_mod, current_module_name, p - current_module_name);

    
    if (*module_name_len != 0) {
        new_mod[p - current_module_name] = '.';
        memcpy(new_mod + (p - current_module_name) + 1, *module_name, *module_name_len);
    } else {
        --new_module_name_len;
    }

    
    qstr new_mod_q = qstr_from_strn(new_mod, new_module_name_len);
    mp_local_free(new_mod);

    DEBUG_printf("Resolved base name for relative import: '%s'\n", qstr_str(new_mod_q));
    *module_name = qstr_str(new_mod_q);
    *module_name_len = new_module_name_len;
}

typedef struct _nlr_jump_callback_node_unregister_module_t {
    nlr_jump_callback_node_t callback;
    qstr name;
} nlr_jump_callback_node_unregister_module_t;

static void unregister_module_from_nlr_jump_callback(void *ctx_in) {
    nlr_jump_callback_node_unregister_module_t *ctx = ctx_in;
    mp_map_t *mp_loaded_modules_map = &MP_STATE_VM(mp_loaded_modules_dict).map;
    mp_map_lookup(mp_loaded_modules_map, MP_OBJ_NEW_QSTR(ctx->name), MP_MAP_LOOKUP_REMOVE_IF_FOUND);
}








static mp_obj_t process_import_at_level(qstr full_mod_name, qstr level_mod_name, mp_obj_t outer_module_obj, bool override_main) {
    
    mp_map_elem_t *elem;

    #if MICROPY_PY_SYS
    
    
    
    size_t path_num;
    mp_obj_t *path_items;
    mp_obj_get_array(mp_sys_path, &path_num, &path_items);
    if (path_num)
    #endif
    {
        elem = mp_map_lookup(&MP_STATE_VM(mp_loaded_modules_dict).map, MP_OBJ_NEW_QSTR(full_mod_name), MP_MAP_LOOKUP);
        if (elem) {
            return elem->value;
        }
    }

    VSTR_FIXED(path, MICROPY_ALLOC_PATH_MAX);
    mp_import_stat_t stat = MP_IMPORT_STAT_NO_EXIST;
    mp_obj_t module_obj;

    if (outer_module_obj == MP_OBJ_NULL) {
        
        DEBUG_printf("Searching for top-level module\n");

        
        
        
        module_obj = mp_module_get_builtin(level_mod_name, false);
        if (module_obj != MP_OBJ_NULL) {
            return module_obj;
        }

        
        
        stat = stat_top_level(level_mod_name, &path);

        
        
        if (stat == MP_IMPORT_STAT_NO_EXIST) {
            module_obj = mp_module_get_builtin(level_mod_name, true);
            if (module_obj != MP_OBJ_NULL) {
                return module_obj;
            }
        }
    } else {
        DEBUG_printf("Searching for sub-module\n");

        #if MICROPY_MODULE_BUILTIN_SUBPACKAGES
        
        
        
        mp_obj_module_t *mod = MP_OBJ_TO_PTR(outer_module_obj);
        if (mod->globals->map.is_fixed) {
            elem = mp_map_lookup(&mod->globals->map, MP_OBJ_NEW_QSTR(level_mod_name), MP_MAP_LOOKUP);
            
            if (elem && mp_obj_is_type(elem->value, &mp_type_module)) {
                return elem->value;
            }
        }
        #endif

        
        
        mp_obj_t dest[2];
        mp_load_method_maybe(outer_module_obj, MP_QSTR___path__, dest);
        if (dest[0] != MP_OBJ_NULL) {
            
            vstr_add_str(&path, mp_obj_str_get_str(dest[0]));

            
            vstr_add_char(&path, PATH_SEP_CHAR[0]);
            vstr_add_str(&path, qstr_str(level_mod_name));

            stat = stat_module(&path);
        }
    }

    

    if (stat == MP_IMPORT_STAT_NO_EXIST) {
        
        #if MICROPY_ERROR_REPORTING <= MICROPY_ERROR_REPORTING_TERSE
        mp_raise_msg(&mp_type_ImportError, MP_ERROR_TEXT("module not found"));
        #else
        mp_raise_msg_varg(&mp_type_ImportError, MP_ERROR_TEXT("no module named '%q'"), full_mod_name);
        #endif
    }

    
    DEBUG_printf("Found path to load: %.*s\n", (int)vstr_len(&path), vstr_str(&path));

    
    
    
    module_obj = mp_obj_new_module(full_mod_name);
    nlr_jump_callback_node_unregister_module_t ctx;
    ctx.name = full_mod_name;
    nlr_push_jump_callback(&ctx.callback, unregister_module_from_nlr_jump_callback);

    #if MICROPY_MODULE_OVERRIDE_MAIN_IMPORT
    
    
    
    
    
    
    if (override_main && stat != MP_IMPORT_STAT_DIR) {
        mp_obj_module_t *o = MP_OBJ_TO_PTR(module_obj);
        mp_obj_dict_store(MP_OBJ_FROM_PTR(o->globals), MP_OBJ_NEW_QSTR(MP_QSTR___name__), MP_OBJ_NEW_QSTR(MP_QSTR___main__));
        #if MICROPY_CPYTHON_COMPAT
        
        mp_obj_dict_store(MP_OBJ_FROM_PTR(&MP_STATE_VM(mp_loaded_modules_dict)), MP_OBJ_NEW_QSTR(MP_QSTR___main__), module_obj);
        
        
        
        mp_obj_dict_store(MP_OBJ_FROM_PTR(o->globals), MP_OBJ_NEW_QSTR(MP_QSTR___main__), MP_OBJ_NEW_QSTR(full_mod_name));
        #endif
    }
    #endif 

    if (stat == MP_IMPORT_STAT_DIR) {
        
        DEBUG_printf("%.*s is dir\n", (int)vstr_len(&path), vstr_str(&path));

        
        
        
        
        mp_store_attr(module_obj, MP_QSTR___path__, mp_obj_new_str(vstr_str(&path), vstr_len(&path)));
        size_t orig_path_len = path.len;
        vstr_add_str(&path, PATH_SEP_CHAR "__init__.py");

        
        if (stat_file_py_or_mpy(&path) == MP_IMPORT_STAT_FILE) {
            do_load(MP_OBJ_TO_PTR(module_obj), &path);
        } else {
            
            
        }
        
        path.len = orig_path_len;
    } else { 
        
        do_load(MP_OBJ_TO_PTR(module_obj), &path);
        
        
        
        
    }

    if (outer_module_obj != MP_OBJ_NULL) {
        
        mp_store_attr(outer_module_obj, level_mod_name, module_obj);
    }

    nlr_pop_jump_callback(false);

    return module_obj;
}

mp_obj_t mp_builtin___import___default(size_t n_args, const mp_obj_t *args) {
    #if DEBUG_PRINT
    DEBUG_printf("__import__:\n");
    for (size_t i = 0; i < n_args; i++) {
        DEBUG_printf("  ");
        mp_obj_print_helper(MICROPY_DEBUG_PRINTER, args[i], PRINT_REPR);
        DEBUG_printf("\n");
    }
    #endif

    
    
    
    
    
    mp_obj_t module_name_obj = args[0];

    
    
    
    mp_obj_t fromtuple = mp_const_none;

    
    
    
    mp_int_t level = 0;
    if (n_args >= 4) {
        fromtuple = args[3];
        if (n_args >= 5) {
            level = MP_OBJ_SMALL_INT_VALUE(args[4]);
            if (level < 0) {
                mp_raise_ValueError(NULL);
            }
        }
    }

    size_t module_name_len;
    const char *module_name = mp_obj_str_get_data(module_name_obj, &module_name_len);

    if (level != 0) {
        
        
        evaluate_relative_import(level, &module_name, &module_name_len);
        
    }

    if (module_name_len == 0) {
        mp_raise_ValueError(NULL);
    }

    DEBUG_printf("Starting module search for '%s'\n", module_name);

    mp_obj_t top_module_obj = MP_OBJ_NULL;
    mp_obj_t outer_module_obj = MP_OBJ_NULL;

    
    
    
    size_t current_component_start = 0;
    for (size_t i = 1; i <= module_name_len; i++) {
        if (i == module_name_len || module_name[i] == '.') {
            
            qstr full_mod_name = qstr_from_strn(module_name, i);
            
            qstr level_mod_name = qstr_from_strn(module_name + current_component_start, i - current_component_start);

            DEBUG_printf("Processing module: '%s' at level '%s'\n", qstr_str(full_mod_name), qstr_str(level_mod_name));

            #if MICROPY_MODULE_OVERRIDE_MAIN_IMPORT
            
            
            
            bool override_main = (i == module_name_len && fromtuple == mp_const_false);
            #else
            bool override_main = false;
            #endif

            
            mp_obj_t module_obj = process_import_at_level(full_mod_name, level_mod_name, outer_module_obj, override_main);

            
            outer_module_obj = module_obj;
            if (top_module_obj == MP_OBJ_NULL) {
                top_module_obj = module_obj;
            }

            current_component_start = i + 1;
        }
    }

    if (fromtuple != mp_const_none) {
        
        return outer_module_obj;
    } else {
        
        return top_module_obj;
    }
}

#else 

mp_obj_t mp_builtin___import___default(size_t n_args, const mp_obj_t *args) {
    
    if (n_args >= 5 && MP_OBJ_SMALL_INT_VALUE(args[4]) != 0) {
        mp_raise_NotImplementedError(MP_ERROR_TEXT("relative import"));
    }

    
    mp_map_elem_t *elem = mp_map_lookup(&MP_STATE_VM(mp_loaded_modules_dict).map, args[0], MP_MAP_LOOKUP);
    if (elem) {
        return elem->value;
    }

    
    qstr module_name_qstr = mp_obj_str_get_qstr(args[0]);
    mp_obj_t module_obj = mp_module_get_builtin(module_name_qstr, false);
    if (module_obj != MP_OBJ_NULL) {
        return module_obj;
    }
    
    module_obj = mp_module_get_builtin(module_name_qstr, true);
    if (module_obj != MP_OBJ_NULL) {
        return module_obj;
    }

    
    #if MICROPY_ERROR_REPORTING <= MICROPY_ERROR_REPORTING_TERSE
    mp_raise_msg(&mp_type_ImportError, MP_ERROR_TEXT("module not found"));
    #else
    mp_raise_msg_varg(&mp_type_ImportError, MP_ERROR_TEXT("no module named '%q'"), module_name_qstr);
    #endif
}

#endif 

MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(mp_builtin___import___obj, 1, 5, mp_builtin___import__);
