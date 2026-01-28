
#ifndef MICROPY_INCLUDED_PY_EMITGLUE_H
#define MICROPY_INCLUDED_PY_EMITGLUE_H

#include "py/obj.h"
#include "py/bc.h"





#define MP_PROTO_FUN_INDICATOR_RAW_CODE_0 (0)
#define MP_PROTO_FUN_INDICATOR_RAW_CODE_1 (0)


enum {
    MP_EMIT_OPT_NONE,
    MP_EMIT_OPT_BYTECODE,
    MP_EMIT_OPT_NATIVE_PYTHON,
    MP_EMIT_OPT_VIPER,
    MP_EMIT_OPT_ASM,
};

typedef enum {
    MP_CODE_UNUSED,
    MP_CODE_RESERVED,
    MP_CODE_BYTECODE,
    MP_CODE_NATIVE_PY,
    MP_CODE_NATIVE_VIPER,
    MP_CODE_NATIVE_ASM,
} mp_raw_code_kind_t;




typedef const void *mp_proto_fun_t;




static inline bool mp_proto_fun_is_bytecode(mp_proto_fun_t proto_fun) {
    const uint8_t *header = (const uint8_t *)proto_fun;
    return (header[0] | (header[1] << 8)) != (MP_PROTO_FUN_INDICATOR_RAW_CODE_0 | (MP_PROTO_FUN_INDICATOR_RAW_CODE_1 << 8));
}





typedef struct _mp_raw_code_t {
    uint8_t proto_fun_indicator[2];
    uint8_t kind; 
    bool is_generator;
    const void *fun_data;
    struct _mp_raw_code_t **children;
    #if MICROPY_PERSISTENT_CODE_SAVE
    uint32_t fun_data_len; 
    uint16_t n_children;
    #if MICROPY_EMIT_MACHINE_CODE
    uint16_t prelude_offset;
    #endif
    #if MICROPY_PY_SYS_SETTRACE
    
    
    
    
    uint32_t line_of_definition;
    mp_bytecode_prelude_t prelude;
    #endif
    #endif
    #if MICROPY_EMIT_INLINE_ASM
    uint32_t asm_n_pos_args : 8;
    uint32_t asm_type_sig : 24; 
    #endif
} mp_raw_code_t;




typedef struct _mp_raw_code_truncated_t {
    uint8_t proto_fun_indicator[2];
    uint8_t kind;
    bool is_generator;
    const void *fun_data;
    struct _mp_raw_code_t **children;
    #if MICROPY_PERSISTENT_CODE_SAVE
    uint32_t fun_data_len;
    uint16_t n_children;
    #if MICROPY_EMIT_MACHINE_CODE
    uint16_t prelude_offset;
    #endif
    #if MICROPY_PY_SYS_SETTRACE
    uint32_t line_of_definition;
    mp_bytecode_prelude_t prelude;
    #endif
    #endif
} mp_raw_code_truncated_t;

mp_raw_code_t *mp_emit_glue_new_raw_code(void);

void mp_emit_glue_assign_bytecode(mp_raw_code_t *rc, const byte *code,
    mp_raw_code_t **children,
    #if MICROPY_PERSISTENT_CODE_SAVE
    size_t len,
    uint16_t n_children,
    #endif
    uint16_t scope_flags);

void mp_emit_glue_assign_native(mp_raw_code_t *rc, mp_raw_code_kind_t kind, const void *fun_data, mp_uint_t fun_len,
    mp_raw_code_t **children,
    #if MICROPY_PERSISTENT_CODE_SAVE
    uint16_t n_children,
    uint16_t prelude_offset,
    #endif
    uint16_t scope_flags, uint32_t asm_n_pos_args, uint32_t asm_type_sig);

mp_obj_t mp_make_function_from_proto_fun(mp_proto_fun_t proto_fun, const mp_module_context_t *context, const mp_obj_t *def_args);
mp_obj_t mp_make_closure_from_proto_fun(mp_proto_fun_t proto_fun, const mp_module_context_t *context, mp_uint_t n_closed_over, const mp_obj_t *args);

#endif 
