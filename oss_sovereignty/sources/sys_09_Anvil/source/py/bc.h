
#ifndef MICROPY_INCLUDED_PY_BC_H
#define MICROPY_INCLUDED_PY_BC_H

#include "py/runtime.h"







































#define MP_ENCODE_UINT_MAX_BYTES ((MP_BYTES_PER_OBJ_WORD * 8 + 6) / 7)

#define MP_BC_PRELUDE_SIG_ENCODE(S, E, scope, out_byte, out_env) \
    do {                                                            \
                              \
        size_t F = scope->scope_flags & MP_SCOPE_FLAG_ALL_SIG;      \
        size_t A = scope->num_pos_args;                             \
        size_t K = scope->num_kwonly_args;                          \
        size_t D = scope->num_def_pos_args;                         \
                                                                \
                  \
        S -= 1;                                                     \
                                                                \
                                                \
                                                      \
        uint8_t z = (S & 0xf) << 3 | (E & 1) << 2 | (A & 3);        \
        S >>= 4;                                                    \
        E >>= 1;                                                    \
        A >>= 2;                                                    \
        while (S | E | F | A | K | D) {                             \
            out_byte(out_env, 0x80 | z);                            \
                                                      \
            z = (F & 1) << 6 | (S & 3) << 4 | (K & 1) << 3          \
                | (A & 1) << 2 | (E & 1) << 1 | (D & 1);            \
            S >>= 2;                                                \
            E >>= 1;                                                \
            F >>= 1;                                                \
            A >>= 1;                                                \
            K >>= 1;                                                \
            D >>= 1;                                                \
        }                                                           \
        out_byte(out_env, z);                                       \
    } while (0)

#define MP_BC_PRELUDE_SIG_DECODE_INTO(ip, S, E, F, A, K, D)     \
    do {                                                            \
        uint8_t z = *(ip)++;                                        \
                                                      \
        S = (z >> 3) & 0xf;                                         \
        E = (z >> 2) & 0x1;                                         \
        F = 0;                                                      \
        A = z & 0x3;                                                \
        K = 0;                                                      \
        D = 0;                                                      \
        for (unsigned n = 0; z & 0x80; ++n) {                       \
            z = *(ip)++;                                            \
                                                      \
            S |= (z & 0x30) << (2 * n);                             \
            E |= (z & 0x02) << n;                                   \
            F |= ((z & 0x40) >> 6) << n;                            \
            A |= (z & 0x4) << n;                                    \
            K |= ((z & 0x08) >> 3) << n;                            \
            D |= (z & 0x1) << n;                                    \
        }                                                           \
        S += 1;                                                     \
    } while (0)

#define MP_BC_PRELUDE_SIG_DECODE(ip) \
    size_t n_state, n_exc_stack, scope_flags, n_pos_args, n_kwonly_args, n_def_pos_args; \
    MP_BC_PRELUDE_SIG_DECODE_INTO(ip, n_state, n_exc_stack, scope_flags, n_pos_args, n_kwonly_args, n_def_pos_args); \
    (void)n_state; (void)n_exc_stack; (void)scope_flags; \
    (void)n_pos_args; (void)n_kwonly_args; (void)n_def_pos_args

#define MP_BC_PRELUDE_SIZE_ENCODE(I, C, out_byte, out_env)      \
    do {                                                            \
                                  \
        uint8_t z = 0;                                              \
        do {                                                        \
            z = (I & 0x3f) << 1 | (C & 1);                          \
            C >>= 1;                                                \
            I >>= 6;                                                \
            if (C | I) {                                            \
                z |= 0x80;                                          \
            }                                                       \
            out_byte(out_env, z);                                   \
        } while (C | I);                                            \
    } while (0)

#define MP_BC_PRELUDE_SIZE_DECODE_INTO(ip, I, C)                \
    do {                                                            \
        uint8_t z;                                                  \
        C = 0;                                                      \
        I = 0;                                                      \
        for (unsigned n = 0;; ++n) {                                \
            z = *(ip)++;                                            \
                                                      \
            C |= (z & 1) << n;                                      \
            I |= ((z & 0x7e) >> 1) << (6 * n);                      \
            if (!(z & 0x80)) {                                      \
                break;                                              \
            }                                                       \
        }                                                           \
    } while (0)

#define MP_BC_PRELUDE_SIZE_DECODE(ip) \
    size_t n_info, n_cell; \
    MP_BC_PRELUDE_SIZE_DECODE_INTO(ip, n_info, n_cell); \
    (void)n_info; (void)n_cell


#define MP_CODE_STATE_EXC_SP_IDX_SENTINEL ((uint16_t)-1)


#define MP_CODE_STATE_EXC_SP_IDX_FROM_PTR(exc_stack, exc_sp) ((exc_sp) + 1 - (exc_stack))
#define MP_CODE_STATE_EXC_SP_IDX_TO_PTR(exc_stack, exc_sp_idx) ((exc_stack) + (exc_sp_idx) - 1)

typedef struct _mp_bytecode_prelude_t {
    uint n_state;
    uint n_exc_stack;
    uint scope_flags;
    uint n_pos_args;
    uint n_kwonly_args;
    uint n_def_pos_args;
    qstr qstr_block_name_idx;
    const byte *line_info;
    const byte *line_info_top;
    const byte *opcodes;
} mp_bytecode_prelude_t;


typedef struct _mp_exc_stack_t {
    const byte *handler;
    
    
    mp_obj_t *val_sp;
    
    mp_obj_base_t *prev_exc;
} mp_exc_stack_t;


typedef struct _mp_module_constants_t {
    #if MICROPY_EMIT_BYTECODE_USES_QSTR_TABLE
    qstr_short_t *qstr_table;
    #else
    qstr source_file;
    #endif
    mp_obj_t *obj_table;
} mp_module_constants_t;


typedef struct _mp_module_context_t {
    mp_obj_module_t module;
    mp_module_constants_t constants;
} mp_module_context_t;


typedef struct _mp_compiled_module_t {
    mp_module_context_t *context;
    const struct _mp_raw_code_t *rc;
    #if MICROPY_PERSISTENT_CODE_SAVE
    bool has_native;
    size_t n_qstr;
    size_t n_obj;
    #endif
} mp_compiled_module_t;


typedef struct _mp_frozen_module_t {
    const mp_module_constants_t constants;
    const void *proto_fun;
} mp_frozen_module_t;


typedef struct _mp_code_state_t {
    
    
    
    
    struct _mp_obj_fun_bc_t *fun_bc;
    const byte *ip;
    mp_obj_t *sp;
    uint16_t n_state;
    uint16_t exc_sp_idx;
    mp_obj_dict_t *old_globals;
    #if MICROPY_STACKLESS
    struct _mp_code_state_t *prev;
    #endif
    #if MICROPY_PY_SYS_SETTRACE
    struct _mp_code_state_t *prev_state;
    struct _mp_obj_frame_t *frame;
    #endif
    
    mp_obj_t state[0];
    
    
} mp_code_state_t;


typedef struct _mp_code_state_native_t {
    struct _mp_obj_fun_bc_t *fun_bc;
    const byte *ip;
    mp_obj_t *sp;
    uint16_t n_state;
    uint16_t exc_sp_idx;
    mp_obj_dict_t *old_globals;
    mp_obj_t state[0];
} mp_code_state_native_t;


typedef uint8_t *(*mp_encode_uint_allocator_t)(void *env, size_t nbytes);

void mp_encode_uint(void *env, mp_encode_uint_allocator_t allocator, mp_uint_t val);
mp_uint_t mp_decode_uint(const byte **ptr);
mp_uint_t mp_decode_uint_value(const byte *ptr);
const byte *mp_decode_uint_skip(const byte *ptr);

mp_vm_return_kind_t mp_execute_bytecode(mp_code_state_t *code_state,
#ifndef __cplusplus
    volatile
#endif
    mp_obj_t inject_exc);
mp_code_state_t *mp_obj_fun_bc_prepare_codestate(mp_obj_t func, size_t n_args, size_t n_kw, const mp_obj_t *args);
void mp_setup_code_state(mp_code_state_t *code_state, size_t n_args, size_t n_kw, const mp_obj_t *args);
void mp_setup_code_state_native(mp_code_state_native_t *code_state, size_t n_args, size_t n_kw, const mp_obj_t *args);
void mp_bytecode_print(const mp_print_t *print, const struct _mp_raw_code_t *rc, size_t fun_data_len, const mp_module_constants_t *cm);
void mp_bytecode_print2(const mp_print_t *print, const byte *ip, size_t len, struct _mp_raw_code_t *const *child_table, const mp_module_constants_t *cm);
const byte *mp_bytecode_print_str(const mp_print_t *print, const byte *ip_start, const byte *ip, struct _mp_raw_code_t *const *child_table, const mp_module_constants_t *cm);
#define mp_bytecode_print_inst(print, code, x_table) mp_bytecode_print2(print, code, 1, x_table)


#define MP_TAGPTR_PTR(x) ((void *)((uintptr_t)(x) & ~((uintptr_t)3)))
#define MP_TAGPTR_TAG0(x) ((uintptr_t)(x) & 1)
#define MP_TAGPTR_TAG1(x) ((uintptr_t)(x) & 2)
#define MP_TAGPTR_MAKE(ptr, tag) ((void *)((uintptr_t)(ptr) | (tag)))

static inline void mp_module_context_alloc_tables(mp_module_context_t *context, size_t n_qstr, size_t n_obj) {
    #if MICROPY_EMIT_BYTECODE_USES_QSTR_TABLE
    size_t nq = (n_qstr * sizeof(qstr_short_t) + sizeof(mp_uint_t) - 1) / sizeof(mp_uint_t);
    size_t no = n_obj;
    mp_uint_t *mem = m_new(mp_uint_t, nq + no);
    context->constants.qstr_table = (qstr_short_t *)mem;
    context->constants.obj_table = (mp_obj_t *)(mem + nq);
    #else
    if (n_obj == 0) {
        context->constants.obj_table = NULL;
    } else {
        context->constants.obj_table = m_new(mp_obj_t, n_obj);
    }
    #endif
}

static inline size_t mp_bytecode_get_source_line(const byte *line_info, const byte *line_info_top, size_t bc_offset) {
    size_t source_line = 1;
    while (line_info < line_info_top) {
        size_t c = *line_info;
        size_t b, l;
        if ((c & 0x80) == 0) {
            
            b = c & 0x1f;
            l = c >> 5;
            line_info += 1;
        } else {
            
            b = c & 0xf;
            l = ((c << 4) & 0x700) | line_info[1];
            line_info += 2;
        }
        if (bc_offset >= b) {
            bc_offset -= b;
            source_line += l;
        } else {
            
            break;
        }
    }
    return source_line;
}

#endif 
