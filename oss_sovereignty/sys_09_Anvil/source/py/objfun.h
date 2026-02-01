 
#ifndef MICROPY_INCLUDED_PY_OBJFUN_H
#define MICROPY_INCLUDED_PY_OBJFUN_H

#include "py/bc.h"
#include "py/obj.h"

typedef struct _mp_obj_fun_bc_t {
    mp_obj_base_t base;
    const mp_module_context_t *context;         
    struct _mp_raw_code_t *const *child_table;  
    const byte *bytecode;                       
    #if MICROPY_PY_SYS_SETTRACE
    const struct _mp_raw_code_t *rc;
    #endif
    
    
    
    mp_obj_t extra_args[];
} mp_obj_fun_bc_t;

typedef struct _mp_obj_fun_asm_t {
    mp_obj_base_t base;
    size_t n_args;
    const void *fun_data; 
    mp_uint_t type_sig;
} mp_obj_fun_asm_t;

mp_obj_t mp_obj_new_fun_bc(const mp_obj_t *def_args, const byte *code, const mp_module_context_t *cm, struct _mp_raw_code_t *const *raw_code_table);
void mp_obj_fun_bc_attr(mp_obj_t self_in, qstr attr, mp_obj_t *dest);

#if MICROPY_EMIT_NATIVE

static inline mp_obj_t mp_obj_new_fun_native(const mp_obj_t *def_args, const void *fun_data, const mp_module_context_t *mc, struct _mp_raw_code_t *const *child_table) {
    mp_obj_fun_bc_t *o = (mp_obj_fun_bc_t *)MP_OBJ_TO_PTR(mp_obj_new_fun_bc(def_args, (const byte *)fun_data, mc, child_table));
    o->base.type = &mp_type_fun_native;
    return MP_OBJ_FROM_PTR(o);
}

static inline mp_obj_t mp_obj_new_fun_viper(const void *fun_data, const mp_module_context_t *mc, struct _mp_raw_code_t *const *child_table) {
    mp_obj_fun_bc_t *o = mp_obj_malloc(mp_obj_fun_bc_t, &mp_type_fun_viper);
    o->bytecode = (const byte *)fun_data;
    o->context = mc;
    o->child_table = child_table;
    return MP_OBJ_FROM_PTR(o);
}

static inline const uint8_t *mp_obj_fun_native_get_prelude_ptr(const mp_obj_fun_bc_t *fun_native) {
    
    uintptr_t prelude_ptr_index = ((uintptr_t *)fun_native->bytecode)[0];
    const uint8_t *prelude_ptr;
    if (prelude_ptr_index == 0) {
        prelude_ptr = (const uint8_t *)fun_native->child_table;
    } else {
        prelude_ptr = (const uint8_t *)fun_native->child_table[prelude_ptr_index];
    }
    return prelude_ptr;
}

static inline void *mp_obj_fun_native_get_function_start(const mp_obj_fun_bc_t *fun_native) {
    
    return MICROPY_MAKE_POINTER_CALLABLE((void *)(fun_native->bytecode + sizeof(uintptr_t)));
}

static inline void *mp_obj_fun_native_get_generator_start(const mp_obj_fun_bc_t *fun_native) {
    
    uintptr_t start_offset = ((uintptr_t *)fun_native->bytecode)[1];
    return MICROPY_MAKE_POINTER_CALLABLE((void *)(fun_native->bytecode + start_offset));
}

static inline void *mp_obj_fun_native_get_generator_resume(const mp_obj_fun_bc_t *fun_native) {
    
    return MICROPY_MAKE_POINTER_CALLABLE((void *)&((uintptr_t *)fun_native->bytecode)[2]);
}

#endif

#if MICROPY_EMIT_INLINE_ASM
static inline mp_obj_t mp_obj_new_fun_asm(size_t n_args, const void *fun_data, mp_uint_t type_sig) {
    mp_obj_fun_asm_t *o = (mp_obj_fun_asm_t *)mp_obj_malloc(mp_obj_fun_asm_t, &mp_type_fun_asm);
    o->n_args = n_args;
    o->fun_data = (const byte *)fun_data;
    o->type_sig = type_sig;
    return MP_OBJ_FROM_PTR(o);
}
#endif

#endif 
