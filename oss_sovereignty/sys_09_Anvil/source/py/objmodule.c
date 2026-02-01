 

#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include "py/bc.h"
#include "py/objmodule.h"
#include "py/runtime.h"
#include "py/builtin.h"

static void module_print(const mp_print_t *print, mp_obj_t self_in, mp_print_kind_t kind) {
    (void)kind;
    mp_obj_module_t *self = MP_OBJ_TO_PTR(self_in);

    const char *module_name = "";
    mp_map_elem_t *elem = mp_map_lookup(&self->globals->map, MP_OBJ_NEW_QSTR(MP_QSTR___name__), MP_MAP_LOOKUP);
    if (elem != NULL) {
        module_name = mp_obj_str_get_str(elem->value);
    }

    #if MICROPY_PY___FILE__
    
    
    elem = mp_map_lookup(&self->globals->map, MP_OBJ_NEW_QSTR(MP_QSTR___file__), MP_MAP_LOOKUP);
    if (elem != NULL) {
        mp_printf(print, "<module '%s' from '%s'>", module_name, mp_obj_str_get_str(elem->value));
        return;
    }
    #endif

    mp_printf(print, "<module '%s'>", module_name);
}

static void module_attr_try_delegation(mp_obj_t self_in, qstr attr, mp_obj_t *dest);

static void module_attr(mp_obj_t self_in, qstr attr, mp_obj_t *dest) {
    mp_obj_module_t *self = MP_OBJ_TO_PTR(self_in);
    if (dest[0] == MP_OBJ_NULL) {
        
        mp_map_elem_t *elem = mp_map_lookup(&self->globals->map, MP_OBJ_NEW_QSTR(attr), MP_MAP_LOOKUP);
        if (elem != NULL) {
            dest[0] = elem->value;
        #if MICROPY_CPYTHON_COMPAT
        } else if (attr == MP_QSTR___dict__) {
            dest[0] = MP_OBJ_FROM_PTR(self->globals);
        #endif
        #if MICROPY_MODULE_GETATTR
        } else if (attr != MP_QSTR___getattr__) {
            elem = mp_map_lookup(&self->globals->map, MP_OBJ_NEW_QSTR(MP_QSTR___getattr__), MP_MAP_LOOKUP);
            if (elem != NULL) {
                dest[0] = mp_call_function_1(elem->value, MP_OBJ_NEW_QSTR(attr));
            } else {
                module_attr_try_delegation(self_in, attr, dest);
            }
        #endif
        } else {
            module_attr_try_delegation(self_in, attr, dest);
        }
    } else {
        
        mp_obj_dict_t *dict = self->globals;
        if (dict->map.is_fixed) {
            #if MICROPY_CAN_OVERRIDE_BUILTINS
            if (dict == &mp_module_builtins_globals) {
                if (MP_STATE_VM(mp_module_builtins_override_dict) == NULL) {
                    MP_STATE_VM(mp_module_builtins_override_dict) = MP_OBJ_TO_PTR(mp_obj_new_dict(1));
                }
                dict = MP_STATE_VM(mp_module_builtins_override_dict);
            } else
            #endif
            {
                
                module_attr_try_delegation(self_in, attr, dest);
                return;
            }
        }
        if (dest[1] == MP_OBJ_NULL) {
            
            mp_obj_dict_delete(MP_OBJ_FROM_PTR(dict), MP_OBJ_NEW_QSTR(attr));
        } else {
            
            mp_obj_dict_store(MP_OBJ_FROM_PTR(dict), MP_OBJ_NEW_QSTR(attr), dest[1]);
        }
        dest[0] = MP_OBJ_NULL; 
    }
}

MP_DEFINE_CONST_OBJ_TYPE(
    mp_type_module,
    MP_QSTR_module,
    MP_TYPE_FLAG_NONE,
    print, module_print,
    attr, module_attr
    );

mp_obj_t mp_obj_new_module(qstr module_name) {
    mp_map_t *mp_loaded_modules_map = &MP_STATE_VM(mp_loaded_modules_dict).map;
    mp_map_elem_t *el = mp_map_lookup(mp_loaded_modules_map, MP_OBJ_NEW_QSTR(module_name), MP_MAP_LOOKUP_ADD_IF_NOT_FOUND);
    
    
    if (el->value != MP_OBJ_NULL) {
        return el->value;
    }

    
    mp_module_context_t *o = m_new_obj(mp_module_context_t);
    o->module.base.type = &mp_type_module;
    o->module.globals = MP_OBJ_TO_PTR(mp_obj_new_dict(MICROPY_MODULE_DICT_SIZE));

    
    mp_obj_dict_store(MP_OBJ_FROM_PTR(o->module.globals), MP_OBJ_NEW_QSTR(MP_QSTR___name__), MP_OBJ_NEW_QSTR(module_name));

    
    el->value = MP_OBJ_FROM_PTR(o);

    
    return MP_OBJ_FROM_PTR(o);
}

 


static const mp_rom_map_elem_t mp_builtin_module_table[] = {
    
    MICROPY_REGISTERED_MODULES
};
MP_DEFINE_CONST_MAP(mp_builtin_module_map, mp_builtin_module_table);

static const mp_rom_map_elem_t mp_builtin_extensible_module_table[] = {
    
    MICROPY_REGISTERED_EXTENSIBLE_MODULES
};
MP_DEFINE_CONST_MAP(mp_builtin_extensible_module_map, mp_builtin_extensible_module_table);

#if MICROPY_MODULE_ATTR_DELEGATION && defined(MICROPY_MODULE_DELEGATIONS)
typedef struct _mp_module_delegation_entry_t {
    mp_rom_obj_t mod;
    mp_attr_fun_t fun;
} mp_module_delegation_entry_t;

static const mp_module_delegation_entry_t mp_builtin_module_delegation_table[] = {
    
    MICROPY_MODULE_DELEGATIONS
};
#endif



mp_obj_t mp_module_get_builtin(qstr module_name, bool extensible) {
    mp_map_elem_t *elem = mp_map_lookup((mp_map_t *)(extensible ? &mp_builtin_extensible_module_map : &mp_builtin_module_map), MP_OBJ_NEW_QSTR(module_name), MP_MAP_LOOKUP);
    if (!elem) {
        #if MICROPY_PY_SYS
        
        
        if (module_name == MP_QSTR_usys) {
            return MP_OBJ_FROM_PTR(&mp_module_sys);
        }
        #endif

        if (extensible) {
            
            
            
            return MP_OBJ_NULL;
        }

        
        
        
        
        
        
        
        
        size_t module_name_len;
        const char *module_name_str = (const char *)qstr_data(module_name, &module_name_len);
        if (module_name_str[0] != 'u') {
            return MP_OBJ_NULL;
        }
        elem = mp_map_lookup((mp_map_t *)&mp_builtin_extensible_module_map, MP_OBJ_NEW_QSTR(qstr_from_strn(module_name_str + 1, module_name_len - 1)), MP_MAP_LOOKUP);
        if (!elem) {
            return MP_OBJ_NULL;
        }
    }

    #if MICROPY_MODULE_BUILTIN_INIT
    
    
    
    mp_obj_t dest[2];
    mp_load_method_maybe(elem->value, MP_QSTR___init__, dest);
    if (dest[0] != MP_OBJ_NULL) {
        mp_call_method_n_kw(0, 0, dest);
    }
    #endif

    return elem->value;
}

static void module_attr_try_delegation(mp_obj_t self_in, qstr attr, mp_obj_t *dest) {
    #if MICROPY_MODULE_ATTR_DELEGATION && defined(MICROPY_MODULE_DELEGATIONS)
    
    size_t n = MP_ARRAY_SIZE(mp_builtin_module_delegation_table);
    for (size_t i = 0; i < n; ++i) {
        if (*(mp_obj_t *)(&mp_builtin_module_delegation_table[i].mod) == self_in) {
            mp_builtin_module_delegation_table[i].fun(self_in, attr, dest);
            break;
        }
    }
    #else
    (void)self_in;
    (void)attr;
    (void)dest;
    #endif
}

void mp_module_generic_attr(qstr attr, mp_obj_t *dest, const uint16_t *keys, mp_obj_t *values) {
    for (size_t i = 0; keys[i] != MP_QSTRnull; ++i) {
        if (attr == keys[i]) {
            if (dest[0] == MP_OBJ_NULL) {
                
                dest[0] = values[i];
            } else {
                
                values[i] = dest[1];
                dest[0] = MP_OBJ_NULL; 
            }
            return;
        }
    }
}
