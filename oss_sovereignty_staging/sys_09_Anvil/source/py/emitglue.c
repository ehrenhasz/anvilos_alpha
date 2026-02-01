 



#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>

#include "py/emitglue.h"
#include "py/runtime0.h"
#include "py/bc.h"
#include "py/objfun.h"
#include "py/profile.h"

#if MICROPY_DEBUG_VERBOSE 
#define DEBUG_PRINT (1)
#define WRITE_CODE (1)
#define DEBUG_printf DEBUG_printf
#define DEBUG_OP_printf(...) DEBUG_printf(__VA_ARGS__)
#else 
#define DEBUG_printf(...) (void)0
#define DEBUG_OP_printf(...) (void)0
#endif

#if MICROPY_DEBUG_PRINTERS
mp_uint_t mp_verbose_flag = 0;
#endif

mp_raw_code_t *mp_emit_glue_new_raw_code(void) {
    mp_raw_code_t *rc = m_new0(mp_raw_code_t, 1);
    rc->kind = MP_CODE_RESERVED;
    #if MICROPY_PY_SYS_SETTRACE
    rc->line_of_definition = 0;
    #endif
    return rc;
}

void mp_emit_glue_assign_bytecode(mp_raw_code_t *rc, const byte *code,
    mp_raw_code_t **children,
    #if MICROPY_PERSISTENT_CODE_SAVE
    size_t len,
    uint16_t n_children,
    #endif
    uint16_t scope_flags) {

    rc->kind = MP_CODE_BYTECODE;
    rc->is_generator = (scope_flags & MP_SCOPE_FLAG_GENERATOR) != 0;
    rc->fun_data = code;
    rc->children = children;

    #if MICROPY_PERSISTENT_CODE_SAVE
    rc->fun_data_len = len;
    rc->n_children = n_children;
    #endif

    #if MICROPY_PY_SYS_SETTRACE
    mp_bytecode_prelude_t *prelude = &rc->prelude;
    mp_prof_extract_prelude(code, prelude);
    #endif

    #if DEBUG_PRINT
    #if !MICROPY_PERSISTENT_CODE_SAVE
    const size_t len = 0;
    #endif
    DEBUG_printf("assign byte code: code=%p len=" UINT_FMT " flags=%x\n", code, len, (uint)scope_flags);
    #endif
}

#if MICROPY_EMIT_MACHINE_CODE
void mp_emit_glue_assign_native(mp_raw_code_t *rc, mp_raw_code_kind_t kind, const void *fun_data, mp_uint_t fun_len,
    mp_raw_code_t **children,
    #if MICROPY_PERSISTENT_CODE_SAVE
    uint16_t n_children,
    uint16_t prelude_offset,
    #endif
    uint16_t scope_flags, uint32_t asm_n_pos_args, uint32_t asm_type_sig
    ) {

    assert(kind == MP_CODE_NATIVE_PY || kind == MP_CODE_NATIVE_VIPER || kind == MP_CODE_NATIVE_ASM);

    
    
    
    #if MICROPY_EMIT_THUMB || MICROPY_EMIT_INLINE_THUMB
    #if __ICACHE_PRESENT == 1
    
    MP_HAL_CLEAN_DCACHE(fun_data, fun_len);
    
    SCB_InvalidateICache();
    #endif
    #elif MICROPY_EMIT_ARM
    #if (defined(__linux__) && defined(__GNUC__)) || __ARM_ARCH == 7
    __builtin___clear_cache((void *)fun_data, (uint8_t *)fun_data + fun_len);
    #elif defined(__arm__)
    
    asm volatile (
        "0:"
        "mrc p15, 0, r15, c7, c10, 3\n" 
        "bne 0b\n"
        "mov r0, #0\n"
        "mcr p15, 0, r0, c7, c7, 0\n" 
        : : : "r0", "cc");
    #endif
    #endif

    rc->kind = kind;
    rc->is_generator = (scope_flags & MP_SCOPE_FLAG_GENERATOR) != 0;
    rc->fun_data = fun_data;

    #if MICROPY_PERSISTENT_CODE_SAVE
    rc->fun_data_len = fun_len;
    #endif
    rc->children = children;

    #if MICROPY_PERSISTENT_CODE_SAVE
    rc->n_children = n_children;
    rc->prelude_offset = prelude_offset;
    #endif

    #if MICROPY_EMIT_INLINE_ASM
    
    rc->asm_n_pos_args = asm_n_pos_args;
    rc->asm_type_sig = asm_type_sig;
    #endif

    #if DEBUG_PRINT
    DEBUG_printf("assign native: kind=%d fun=%p len=" UINT_FMT " flags=%x\n", kind, fun_data, fun_len, (uint)scope_flags);
    for (mp_uint_t i = 0; i < fun_len; i++) {
        if (i > 0 && i % 16 == 0) {
            DEBUG_printf("\n");
        }
        DEBUG_printf(" %02x", ((const byte *)fun_data)[i]);
    }
    DEBUG_printf("\n");

    #ifdef WRITE_CODE
    FILE *fp_write_code = fopen("out-code", "wb");
    fwrite(fun_data, fun_len, 1, fp_write_code);
    fclose(fp_write_code);
    #endif
    #else
    (void)fun_len;
    #endif
}
#endif

mp_obj_t mp_make_function_from_proto_fun(mp_proto_fun_t proto_fun, const mp_module_context_t *context, const mp_obj_t *def_args) {
    DEBUG_OP_printf("make_function_from_proto_fun %p\n", proto_fun);
    assert(proto_fun != NULL);

    
    assert(def_args == NULL || def_args[0] == MP_OBJ_NULL || mp_obj_is_type(def_args[0], &mp_type_tuple));

    
    assert(def_args == NULL || def_args[1] == MP_OBJ_NULL || mp_obj_is_type(def_args[1], &mp_type_dict));

    #if MICROPY_MODULE_FROZEN_MPY
    if (mp_proto_fun_is_bytecode(proto_fun)) {
        const uint8_t *bc = proto_fun;
        mp_obj_t fun = mp_obj_new_fun_bc(def_args, bc, context, NULL);
        MP_BC_PRELUDE_SIG_DECODE(bc);
        if (scope_flags & MP_SCOPE_FLAG_GENERATOR) {
            ((mp_obj_base_t *)MP_OBJ_TO_PTR(fun))->type = &mp_type_gen_wrap;
        }
        return fun;
    }
    #endif

    
    const mp_raw_code_t *rc = proto_fun;

    
    mp_obj_t fun;
    switch (rc->kind) {
        #if MICROPY_EMIT_NATIVE
        case MP_CODE_NATIVE_PY:
            fun = mp_obj_new_fun_native(def_args, rc->fun_data, context, rc->children);
            
            if (rc->is_generator) {
                ((mp_obj_base_t *)MP_OBJ_TO_PTR(fun))->type = &mp_type_native_gen_wrap;
            }
            break;
        case MP_CODE_NATIVE_VIPER:
            fun = mp_obj_new_fun_viper(rc->fun_data, context, rc->children);
            break;
        #endif
        #if MICROPY_EMIT_INLINE_ASM
        case MP_CODE_NATIVE_ASM:
            fun = mp_obj_new_fun_asm(rc->asm_n_pos_args, rc->fun_data, rc->asm_type_sig);
            break;
        #endif
        default:
            
            assert(rc->kind == MP_CODE_BYTECODE);
            fun = mp_obj_new_fun_bc(def_args, rc->fun_data, context, rc->children);
            
            if (rc->is_generator) {
                ((mp_obj_base_t *)MP_OBJ_TO_PTR(fun))->type = &mp_type_gen_wrap;
            }

            #if MICROPY_PY_SYS_SETTRACE
            mp_obj_fun_bc_t *self_fun = (mp_obj_fun_bc_t *)MP_OBJ_TO_PTR(fun);
            self_fun->rc = rc;
            #endif

            break;
    }

    return fun;
}

mp_obj_t mp_make_closure_from_proto_fun(mp_proto_fun_t proto_fun, const mp_module_context_t *context, mp_uint_t n_closed_over, const mp_obj_t *args) {
    DEBUG_OP_printf("make_closure_from_proto_fun %p " UINT_FMT " %p\n", proto_fun, n_closed_over, args);
    
    mp_obj_t ffun;
    if (n_closed_over & 0x100) {
        
        ffun = mp_make_function_from_proto_fun(proto_fun, context, args);
    } else {
        
        ffun = mp_make_function_from_proto_fun(proto_fun, context, NULL);
    }
    
    return mp_obj_new_closure(ffun, n_closed_over & 0xff, args + ((n_closed_over >> 7) & 2));
}
