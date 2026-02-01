 



















#include <stdio.h>
#include <string.h>
#include <assert.h>

#include "py/emit.h"
#include "py/nativeglue.h"
#include "py/objfun.h"
#include "py/objstr.h"

#if MICROPY_DEBUG_VERBOSE 
#define DEBUG_PRINT (1)
#define DEBUG_printf DEBUG_printf
#else 
#define DEBUG_printf(...) (void)0
#endif


#if N_X64 || N_X86 || N_THUMB || N_ARM || N_XTENSA || N_XTENSAWIN





























#if MICROPY_DYNAMIC_COMPILER
#define SIZEOF_NLR_BUF (2 + mp_dynamic_compiler.nlr_buf_num_regs + 1) 
#else
#define SIZEOF_NLR_BUF (sizeof(nlr_buf_t) / sizeof(uintptr_t))
#endif
#define SIZEOF_CODE_STATE (sizeof(mp_code_state_native_t) / sizeof(uintptr_t))
#define OFFSETOF_CODE_STATE_STATE (offsetof(mp_code_state_native_t, state) / sizeof(uintptr_t))
#define OFFSETOF_CODE_STATE_FUN_BC (offsetof(mp_code_state_native_t, fun_bc) / sizeof(uintptr_t))
#define OFFSETOF_CODE_STATE_IP (offsetof(mp_code_state_native_t, ip) / sizeof(uintptr_t))
#define OFFSETOF_CODE_STATE_SP (offsetof(mp_code_state_native_t, sp) / sizeof(uintptr_t))
#define OFFSETOF_CODE_STATE_N_STATE (offsetof(mp_code_state_native_t, n_state) / sizeof(uintptr_t))
#define OFFSETOF_OBJ_FUN_BC_CONTEXT (offsetof(mp_obj_fun_bc_t, context) / sizeof(uintptr_t))
#define OFFSETOF_OBJ_FUN_BC_CHILD_TABLE (offsetof(mp_obj_fun_bc_t, child_table) / sizeof(uintptr_t))
#define OFFSETOF_OBJ_FUN_BC_BYTECODE (offsetof(mp_obj_fun_bc_t, bytecode) / sizeof(uintptr_t))
#define OFFSETOF_MODULE_CONTEXT_QSTR_TABLE (offsetof(mp_module_context_t, constants.qstr_table) / sizeof(uintptr_t))
#define OFFSETOF_MODULE_CONTEXT_OBJ_TABLE (offsetof(mp_module_context_t, constants.obj_table) / sizeof(uintptr_t))
#define OFFSETOF_MODULE_CONTEXT_GLOBALS (offsetof(mp_module_context_t, module.globals) / sizeof(uintptr_t))


#ifndef REG_PARENT_RET
#define REG_PARENT_RET REG_RET
#define REG_PARENT_ARG_1 REG_ARG_1
#define REG_PARENT_ARG_2 REG_ARG_2
#define REG_PARENT_ARG_3 REG_ARG_3
#define REG_PARENT_ARG_4 REG_ARG_4
#endif


#define NLR_BUF_IDX_RET_VAL (1)


#define NEED_FUN_OBJ(emit) ((emit)->scope->exc_stack_size > 0 \
    || ((emit)->scope->scope_flags & (MP_SCOPE_FLAG_REFGLOBALS | MP_SCOPE_FLAG_HASCONSTS)))


#define NEED_GLOBAL_EXC_HANDLER(emit) ((emit)->scope->exc_stack_size > 0 \
    || ((emit)->scope->scope_flags & (MP_SCOPE_FLAG_GENERATOR | MP_SCOPE_FLAG_REFGLOBALS)))


#define NEED_EXC_HANDLER_UNWIND(emit) ((emit)->scope->exc_stack_size > 0)




#define CAN_USE_REGS_FOR_LOCALS(emit) ((emit)->scope->exc_stack_size == 0 && !(emit->scope->scope_flags & MP_SCOPE_FLAG_GENERATOR))


#define LOCAL_IDX_EXC_VAL(emit) (NLR_BUF_IDX_RET_VAL)
#define LOCAL_IDX_EXC_HANDLER_PC(emit) (NLR_BUF_IDX_LOCAL_1)
#define LOCAL_IDX_EXC_HANDLER_UNWIND(emit) (SIZEOF_NLR_BUF + 1) 
#define LOCAL_IDX_RET_VAL(emit) (SIZEOF_NLR_BUF) 
#define LOCAL_IDX_FUN_OBJ(emit) ((emit)->code_state_start + OFFSETOF_CODE_STATE_FUN_BC)
#define LOCAL_IDX_OLD_GLOBALS(emit) ((emit)->code_state_start + OFFSETOF_CODE_STATE_IP)
#define LOCAL_IDX_GEN_PC(emit) ((emit)->code_state_start + OFFSETOF_CODE_STATE_IP)
#define LOCAL_IDX_LOCAL_VAR(emit, local_num) ((emit)->stack_start + (emit)->n_state - 1 - (local_num))

#if MICROPY_PERSISTENT_CODE_SAVE






#define REG_GENERATOR_STATE (REG_LOCAL_2)
#define REG_QSTR_TABLE (REG_LOCAL_3)
#define MAX_REGS_FOR_LOCAL_VARS (2)

static const uint8_t reg_local_table[MAX_REGS_FOR_LOCAL_VARS] = {REG_LOCAL_1, REG_LOCAL_2};

#else






#define REG_GENERATOR_STATE (REG_LOCAL_3)
#define MAX_REGS_FOR_LOCAL_VARS (3)

static const uint8_t reg_local_table[MAX_REGS_FOR_LOCAL_VARS] = {REG_LOCAL_1, REG_LOCAL_2, REG_LOCAL_3};

#endif

#define REG_LOCAL_LAST (reg_local_table[MAX_REGS_FOR_LOCAL_VARS - 1])

#define EMIT_NATIVE_VIPER_TYPE_ERROR(emit, ...) do { \
        *emit->error_slot = mp_obj_new_exception_msg_varg(&mp_type_ViperTypeError, __VA_ARGS__); \
} while (0)

typedef enum {
    STACK_VALUE,
    STACK_REG,
    STACK_IMM,
} stack_info_kind_t;



typedef enum {
    VTYPE_PYOBJ = 0x00 | MP_NATIVE_TYPE_OBJ,
    VTYPE_BOOL = 0x00 | MP_NATIVE_TYPE_BOOL,
    VTYPE_INT = 0x00 | MP_NATIVE_TYPE_INT,
    VTYPE_UINT = 0x00 | MP_NATIVE_TYPE_UINT,
    VTYPE_PTR = 0x00 | MP_NATIVE_TYPE_PTR,
    VTYPE_PTR8 = 0x00 | MP_NATIVE_TYPE_PTR8,
    VTYPE_PTR16 = 0x00 | MP_NATIVE_TYPE_PTR16,
    VTYPE_PTR32 = 0x00 | MP_NATIVE_TYPE_PTR32,

    VTYPE_PTR_NONE = 0x50 | MP_NATIVE_TYPE_PTR,

    VTYPE_UNBOUND = 0x60 | MP_NATIVE_TYPE_OBJ,
    VTYPE_BUILTIN_CAST = 0x70 | MP_NATIVE_TYPE_OBJ,
} vtype_kind_t;

static qstr vtype_to_qstr(vtype_kind_t vtype) {
    switch (vtype) {
        case VTYPE_PYOBJ:
            return MP_QSTR_object;
        case VTYPE_BOOL:
            return MP_QSTR_bool;
        case VTYPE_INT:
            return MP_QSTR_int;
        case VTYPE_UINT:
            return MP_QSTR_uint;
        case VTYPE_PTR:
            return MP_QSTR_ptr;
        case VTYPE_PTR8:
            return MP_QSTR_ptr8;
        case VTYPE_PTR16:
            return MP_QSTR_ptr16;
        case VTYPE_PTR32:
            return MP_QSTR_ptr32;
        case VTYPE_PTR_NONE:
        default:
            return MP_QSTR_None;
    }
}

typedef struct _stack_info_t {
    vtype_kind_t vtype;
    stack_info_kind_t kind;
    union {
        int u_reg;
        mp_int_t u_imm;
    } data;
} stack_info_t;

#define UNWIND_LABEL_UNUSED (0x7fff)
#define UNWIND_LABEL_DO_FINAL_UNWIND (0x7ffe)

typedef struct _exc_stack_entry_t {
    uint16_t label : 15;
    uint16_t is_finally : 1;
    uint16_t unwind_label : 15;
    uint16_t is_active : 1;
} exc_stack_entry_t;

struct _emit_t {
    mp_emit_common_t *emit_common;
    mp_obj_t *error_slot;
    uint *label_slot;
    uint exit_label;
    int pass;

    bool do_viper_types;

    mp_uint_t local_vtype_alloc;
    vtype_kind_t *local_vtype;

    mp_uint_t stack_info_alloc;
    stack_info_t *stack_info;
    vtype_kind_t saved_stack_vtype;

    size_t exc_stack_alloc;
    size_t exc_stack_size;
    exc_stack_entry_t *exc_stack;

    int prelude_offset;
    int prelude_ptr_index;
    int start_offset;
    int n_state;
    uint16_t code_state_start;
    uint16_t stack_start;
    int stack_size;
    uint16_t n_info;
    uint16_t n_cell;

    scope_t *scope;

    ASM_T *as;
};

static void emit_load_reg_with_object(emit_t *emit, int reg, mp_obj_t obj);
static void emit_native_global_exc_entry(emit_t *emit);
static void emit_native_global_exc_exit(emit_t *emit);
static void emit_native_load_const_obj(emit_t *emit, mp_obj_t obj);

emit_t *EXPORT_FUN(new)(mp_emit_common_t * emit_common, mp_obj_t *error_slot, uint *label_slot, mp_uint_t max_num_labels) {
    emit_t *emit = m_new0(emit_t, 1);
    emit->emit_common = emit_common;
    emit->error_slot = error_slot;
    emit->label_slot = label_slot;
    emit->stack_info_alloc = 8;
    emit->stack_info = m_new(stack_info_t, emit->stack_info_alloc);
    emit->exc_stack_alloc = 8;
    emit->exc_stack = m_new(exc_stack_entry_t, emit->exc_stack_alloc);
    emit->as = m_new0(ASM_T, 1);
    mp_asm_base_init(&emit->as->base, max_num_labels);
    return emit;
}

void EXPORT_FUN(free)(emit_t * emit) {
    mp_asm_base_deinit(&emit->as->base, false);
    m_del_obj(ASM_T, emit->as);
    m_del(exc_stack_entry_t, emit->exc_stack, emit->exc_stack_alloc);
    m_del(vtype_kind_t, emit->local_vtype, emit->local_vtype_alloc);
    m_del(stack_info_t, emit->stack_info, emit->stack_info_alloc);
    m_del_obj(emit_t, emit);
}

static void emit_call_with_imm_arg(emit_t *emit, mp_fun_kind_t fun_kind, mp_int_t arg_val, int arg_reg);

static void emit_native_mov_reg_const(emit_t *emit, int reg_dest, int const_val) {
    ASM_LOAD_REG_REG_OFFSET(emit->as, reg_dest, REG_FUN_TABLE, const_val);
}

static void emit_native_mov_state_reg(emit_t *emit, int local_num, int reg_src) {
    if (emit->scope->scope_flags & MP_SCOPE_FLAG_GENERATOR) {
        ASM_STORE_REG_REG_OFFSET(emit->as, reg_src, REG_GENERATOR_STATE, local_num);
    } else {
        ASM_MOV_LOCAL_REG(emit->as, local_num, reg_src);
    }
}

static void emit_native_mov_reg_state(emit_t *emit, int reg_dest, int local_num) {
    if (emit->scope->scope_flags & MP_SCOPE_FLAG_GENERATOR) {
        ASM_LOAD_REG_REG_OFFSET(emit->as, reg_dest, REG_GENERATOR_STATE, local_num);
    } else {
        ASM_MOV_REG_LOCAL(emit->as, reg_dest, local_num);
    }
}

static void emit_native_mov_reg_state_addr(emit_t *emit, int reg_dest, int local_num) {
    if (emit->scope->scope_flags & MP_SCOPE_FLAG_GENERATOR) {
        ASM_MOV_REG_IMM(emit->as, reg_dest, local_num * ASM_WORD_SIZE);
        ASM_ADD_REG_REG(emit->as, reg_dest, REG_GENERATOR_STATE);
    } else {
        ASM_MOV_REG_LOCAL_ADDR(emit->as, reg_dest, local_num);
    }
}

static void emit_native_mov_reg_qstr(emit_t *emit, int arg_reg, qstr qst) {
    #if MICROPY_PERSISTENT_CODE_SAVE
    ASM_LOAD16_REG_REG_OFFSET(emit->as, arg_reg, REG_QSTR_TABLE, mp_emit_common_use_qstr(emit->emit_common, qst));
    #else
    ASM_MOV_REG_IMM(emit->as, arg_reg, qst);
    #endif
}

static void emit_native_mov_reg_qstr_obj(emit_t *emit, int reg_dest, qstr qst) {
    #if MICROPY_PERSISTENT_CODE_SAVE
    emit_load_reg_with_object(emit, reg_dest, MP_OBJ_NEW_QSTR(qst));
    #else
    ASM_MOV_REG_IMM(emit->as, reg_dest, (mp_uint_t)MP_OBJ_NEW_QSTR(qst));
    #endif
}

#define emit_native_mov_state_imm_via(emit, local_num, imm, reg_temp) \
    do { \
        ASM_MOV_REG_IMM((emit)->as, (reg_temp), (imm)); \
        emit_native_mov_state_reg((emit), (local_num), (reg_temp)); \
    } while (false)

static void emit_native_start_pass(emit_t *emit, pass_kind_t pass, scope_t *scope) {
    DEBUG_printf("start_pass(pass=%u, scope=%p)\n", pass, scope);

    emit->pass = pass;
    emit->do_viper_types = scope->emit_options == MP_EMIT_OPT_VIPER;
    emit->stack_size = 0;
    emit->scope = scope;

    
    if (emit->local_vtype_alloc < scope->num_locals) {
        emit->local_vtype = m_renew(vtype_kind_t, emit->local_vtype, emit->local_vtype_alloc, scope->num_locals);
        emit->local_vtype_alloc = scope->num_locals;
    }

    
    mp_uint_t num_args = emit->scope->num_pos_args + emit->scope->num_kwonly_args;
    if (scope->scope_flags & MP_SCOPE_FLAG_VARARGS) {
        num_args += 1;
    }
    if (scope->scope_flags & MP_SCOPE_FLAG_VARKEYWORDS) {
        num_args += 1;
    }
    for (mp_uint_t i = 0; i < num_args; i++) {
        emit->local_vtype[i] = VTYPE_PYOBJ;
    }

    
    if (emit->do_viper_types) {
        for (int i = 0; i < emit->scope->id_info_len; ++i) {
            id_info_t *id = &emit->scope->id_info[i];
            if (id->flags & ID_FLAG_IS_PARAM) {
                assert(id->local_num < emit->local_vtype_alloc);
                emit->local_vtype[id->local_num] = id->flags >> ID_FLAG_VIPER_TYPE_POS;
            }
        }
    }

    
    for (mp_uint_t i = num_args; i < emit->local_vtype_alloc; i++) {
        emit->local_vtype[i] = emit->do_viper_types ? VTYPE_UNBOUND : VTYPE_PYOBJ;
    }

    
    for (mp_uint_t i = 0; i < emit->stack_info_alloc; i++) {
        emit->stack_info[i].kind = STACK_VALUE;
        emit->stack_info[i].vtype = VTYPE_UNBOUND;
    }

    mp_asm_base_start_pass(&emit->as->base, pass == MP_PASS_EMIT ? MP_ASM_PASS_EMIT : MP_ASM_PASS_COMPUTE);

    

    
    emit->code_state_start = 0;
    if (NEED_GLOBAL_EXC_HANDLER(emit)) {
        emit->code_state_start = SIZEOF_NLR_BUF; 
        emit->code_state_start += 1;  
        if (NEED_EXC_HANDLER_UNWIND(emit)) {
            emit->code_state_start += 1;
        }
    }

    size_t fun_table_off = mp_emit_common_use_const_obj(emit->emit_common, MP_OBJ_FROM_PTR(&mp_fun_table));

    if (emit->do_viper_types) {
        
        
        emit->n_state = scope->num_locals + scope->stack_size;
        int num_locals_in_regs = 0;
        if (CAN_USE_REGS_FOR_LOCALS(emit)) {
            num_locals_in_regs = scope->num_locals;
            if (num_locals_in_regs > MAX_REGS_FOR_LOCAL_VARS) {
                num_locals_in_regs = MAX_REGS_FOR_LOCAL_VARS;
            }
            
            if (scope->num_pos_args >= MAX_REGS_FOR_LOCAL_VARS + 1) {
                --num_locals_in_regs;
            }
        }

        
        if (NEED_GLOBAL_EXC_HANDLER(emit)) {
            
            emit->stack_start = emit->code_state_start + 2;
        } else if (scope->scope_flags & MP_SCOPE_FLAG_HASCONSTS) {
            
            emit->stack_start = emit->code_state_start + 1;
        } else {
            emit->stack_start = emit->code_state_start + 0;
        }

        
        ASM_ENTRY(emit->as, emit->stack_start + emit->n_state - num_locals_in_regs);

        #if N_X86
        asm_x86_mov_arg_to_r32(emit->as, 0, REG_PARENT_ARG_1);
        #endif

        
        ASM_LOAD_REG_REG_OFFSET(emit->as, REG_FUN_TABLE, REG_PARENT_ARG_1, OFFSETOF_OBJ_FUN_BC_CONTEXT);
        #if MICROPY_PERSISTENT_CODE_SAVE
        ASM_LOAD_REG_REG_OFFSET(emit->as, REG_QSTR_TABLE, REG_FUN_TABLE, OFFSETOF_MODULE_CONTEXT_QSTR_TABLE);
        #endif
        ASM_LOAD_REG_REG_OFFSET(emit->as, REG_FUN_TABLE, REG_FUN_TABLE, OFFSETOF_MODULE_CONTEXT_OBJ_TABLE);
        ASM_LOAD_REG_REG_OFFSET(emit->as, REG_FUN_TABLE, REG_FUN_TABLE, fun_table_off);

        
        if (NEED_FUN_OBJ(emit)) {
            ASM_MOV_LOCAL_REG(emit->as, LOCAL_IDX_FUN_OBJ(emit), REG_PARENT_ARG_1);
        }

        
        #if N_X86
        asm_x86_mov_arg_to_r32(emit->as, 1, REG_ARG_1);
        asm_x86_mov_arg_to_r32(emit->as, 2, REG_ARG_2);
        asm_x86_mov_arg_to_r32(emit->as, 3, REG_LOCAL_LAST);
        #else
        ASM_MOV_REG_REG(emit->as, REG_ARG_1, REG_PARENT_ARG_2);
        ASM_MOV_REG_REG(emit->as, REG_ARG_2, REG_PARENT_ARG_3);
        ASM_MOV_REG_REG(emit->as, REG_LOCAL_LAST, REG_PARENT_ARG_4);
        #endif

        
        ASM_JUMP_IF_REG_NONZERO(emit->as, REG_ARG_2, *emit->label_slot + 4, true);
        ASM_MOV_REG_IMM(emit->as, REG_ARG_3, scope->num_pos_args);
        ASM_JUMP_IF_REG_EQ(emit->as, REG_ARG_1, REG_ARG_3, *emit->label_slot + 5);
        mp_asm_base_label_assign(&emit->as->base, *emit->label_slot + 4);
        ASM_MOV_REG_IMM(emit->as, REG_ARG_3, MP_OBJ_FUN_MAKE_SIG(scope->num_pos_args, scope->num_pos_args, false));
        ASM_CALL_IND(emit->as, MP_F_ARG_CHECK_NUM_SIG);
        mp_asm_base_label_assign(&emit->as->base, *emit->label_slot + 5);

        
        for (int i = 0; i < emit->scope->num_pos_args; i++) {
            int r = REG_ARG_1;
            ASM_LOAD_REG_REG_OFFSET(emit->as, REG_ARG_1, REG_LOCAL_LAST, i);
            if (emit->local_vtype[i] != VTYPE_PYOBJ) {
                emit_call_with_imm_arg(emit, MP_F_CONVERT_OBJ_TO_NATIVE, emit->local_vtype[i], REG_ARG_2);
                r = REG_RET;
            }
            
            if (i < MAX_REGS_FOR_LOCAL_VARS && CAN_USE_REGS_FOR_LOCALS(emit) && (i != MAX_REGS_FOR_LOCAL_VARS - 1 || emit->scope->num_pos_args == MAX_REGS_FOR_LOCAL_VARS)) {
                ASM_MOV_REG_REG(emit->as, reg_local_table[i], r);
            } else {
                emit_native_mov_state_reg(emit, LOCAL_IDX_LOCAL_VAR(emit, i), r);
            }
        }
        
        if (emit->scope->num_pos_args >= MAX_REGS_FOR_LOCAL_VARS + 1 && CAN_USE_REGS_FOR_LOCALS(emit)) {
            ASM_MOV_REG_LOCAL(emit->as, REG_LOCAL_LAST, LOCAL_IDX_LOCAL_VAR(emit, MAX_REGS_FOR_LOCAL_VARS - 1));
        }

        emit_native_global_exc_entry(emit);

    } else {
        
        emit->n_state = scope->num_locals + scope->stack_size;

        
        
        mp_asm_base_data(&emit->as->base, ASM_WORD_SIZE, (uintptr_t)emit->prelude_ptr_index);

        if (emit->scope->scope_flags & MP_SCOPE_FLAG_GENERATOR) {
            mp_asm_base_data(&emit->as->base, ASM_WORD_SIZE, (uintptr_t)emit->start_offset);
            ASM_ENTRY(emit->as, emit->code_state_start);

            
            emit->code_state_start = 0;
            emit->stack_start = SIZEOF_CODE_STATE;

            
            #if N_X86
            asm_x86_mov_arg_to_r32(emit->as, 0, REG_GENERATOR_STATE);
            #else
            ASM_MOV_REG_REG(emit->as, REG_GENERATOR_STATE, REG_PARENT_ARG_1);
            #endif

            
            #if N_X86
            asm_x86_mov_arg_to_r32(emit->as, 1, REG_PARENT_ARG_2);
            #endif
            ASM_MOV_LOCAL_REG(emit->as, LOCAL_IDX_EXC_VAL(emit), REG_PARENT_ARG_2);

            
            ASM_LOAD_REG_REG_OFFSET(emit->as, REG_TEMP0, REG_GENERATOR_STATE, LOCAL_IDX_FUN_OBJ(emit));
            ASM_LOAD_REG_REG_OFFSET(emit->as, REG_TEMP0, REG_TEMP0, OFFSETOF_OBJ_FUN_BC_CONTEXT);
            #if MICROPY_PERSISTENT_CODE_SAVE
            ASM_LOAD_REG_REG_OFFSET(emit->as, REG_QSTR_TABLE, REG_TEMP0, OFFSETOF_MODULE_CONTEXT_QSTR_TABLE);
            #endif
            ASM_LOAD_REG_REG_OFFSET(emit->as, REG_TEMP0, REG_TEMP0, OFFSETOF_MODULE_CONTEXT_OBJ_TABLE);
            ASM_LOAD_REG_REG_OFFSET(emit->as, REG_FUN_TABLE, REG_TEMP0, fun_table_off);
        } else {
            
            emit->stack_start = emit->code_state_start + SIZEOF_CODE_STATE;

            
            ASM_ENTRY(emit->as, emit->stack_start + emit->n_state);

            

            #if N_X86
            asm_x86_mov_arg_to_r32(emit->as, 0, REG_PARENT_ARG_1);
            asm_x86_mov_arg_to_r32(emit->as, 1, REG_PARENT_ARG_2);
            asm_x86_mov_arg_to_r32(emit->as, 2, REG_PARENT_ARG_3);
            asm_x86_mov_arg_to_r32(emit->as, 3, REG_PARENT_ARG_4);
            #endif

            
            ASM_LOAD_REG_REG_OFFSET(emit->as, REG_FUN_TABLE, REG_PARENT_ARG_1, OFFSETOF_OBJ_FUN_BC_CONTEXT);
            #if MICROPY_PERSISTENT_CODE_SAVE
            ASM_LOAD_REG_REG_OFFSET(emit->as, REG_QSTR_TABLE, REG_FUN_TABLE, OFFSETOF_MODULE_CONTEXT_QSTR_TABLE);
            #endif
            ASM_LOAD_REG_REG_OFFSET(emit->as, REG_FUN_TABLE, REG_FUN_TABLE, OFFSETOF_MODULE_CONTEXT_OBJ_TABLE);
            ASM_LOAD_REG_REG_OFFSET(emit->as, REG_FUN_TABLE, REG_FUN_TABLE, fun_table_off);

            
            ASM_MOV_LOCAL_REG(emit->as, LOCAL_IDX_FUN_OBJ(emit), REG_PARENT_ARG_1);

            
            emit_native_mov_state_imm_via(emit, emit->code_state_start + OFFSETOF_CODE_STATE_N_STATE, emit->n_state, REG_ARG_1);

            
            ASM_MOV_REG_LOCAL_ADDR(emit->as, REG_ARG_1, emit->code_state_start);

            
            #if REG_ARG_2 != REG_PARENT_ARG_2
            ASM_MOV_REG_REG(emit->as, REG_ARG_2, REG_PARENT_ARG_2);
            #endif
            #if REG_ARG_3 != REG_PARENT_ARG_3
            ASM_MOV_REG_REG(emit->as, REG_ARG_3, REG_PARENT_ARG_3);
            #endif
            #if REG_ARG_4 != REG_PARENT_ARG_4
            ASM_MOV_REG_REG(emit->as, REG_ARG_4, REG_PARENT_ARG_4);
            #endif

            
            #if N_THUMB
            asm_thumb_bl_ind(emit->as, MP_F_SETUP_CODE_STATE, ASM_THUMB_REG_R4);
            #elif N_ARM
            asm_arm_bl_ind(emit->as, MP_F_SETUP_CODE_STATE, ASM_ARM_REG_R4);
            #else
            ASM_CALL_IND(emit->as, MP_F_SETUP_CODE_STATE);
            #endif
        }

        emit_native_global_exc_entry(emit);

        
        if (CAN_USE_REGS_FOR_LOCALS(emit)) {
            for (int i = 0; i < MAX_REGS_FOR_LOCAL_VARS && i < scope->num_locals; ++i) {
                ASM_MOV_REG_LOCAL(emit->as, reg_local_table[i], LOCAL_IDX_LOCAL_VAR(emit, i));
            }
        }

        
        for (mp_uint_t i = 0; i < scope->id_info_len; i++) {
            id_info_t *id = &scope->id_info[i];
            if (id->kind == ID_INFO_KIND_CELL) {
                emit->local_vtype[id->local_num] = VTYPE_PYOBJ;
            }
        }
    }
}

static inline void emit_native_write_code_info_byte(emit_t *emit, byte val) {
    mp_asm_base_data(&emit->as->base, 1, val);
}

static inline void emit_native_write_code_info_qstr(emit_t *emit, qstr qst) {
    mp_encode_uint(&emit->as->base, mp_asm_base_get_cur_to_write_bytes, mp_emit_common_use_qstr(emit->emit_common, qst));
}

static bool emit_native_end_pass(emit_t *emit) {
    emit_native_global_exc_exit(emit);

    if (!emit->do_viper_types) {
        emit->prelude_offset = mp_asm_base_get_code_pos(&emit->as->base);
        emit->prelude_ptr_index = emit->emit_common->ct_cur_child;

        size_t n_state = emit->n_state;
        size_t n_exc_stack = 0; 
        MP_BC_PRELUDE_SIG_ENCODE(n_state, n_exc_stack, emit->scope, emit_native_write_code_info_byte, emit);

        size_t n_info = emit->n_info;
        size_t n_cell = emit->n_cell;
        MP_BC_PRELUDE_SIZE_ENCODE(n_info, n_cell, emit_native_write_code_info_byte, emit);

        
        size_t info_start = mp_asm_base_get_code_pos(&emit->as->base);
        emit_native_write_code_info_qstr(emit, emit->scope->simple_name);
        for (int i = 0; i < emit->scope->num_pos_args + emit->scope->num_kwonly_args; i++) {
            qstr qst = MP_QSTR__star_;
            for (int j = 0; j < emit->scope->id_info_len; ++j) {
                id_info_t *id = &emit->scope->id_info[j];
                if ((id->flags & ID_FLAG_IS_PARAM) && id->local_num == i) {
                    qst = id->qst;
                    break;
                }
            }
            emit_native_write_code_info_qstr(emit, qst);
        }
        emit->n_info = mp_asm_base_get_code_pos(&emit->as->base) - info_start;

        
        size_t cell_start = mp_asm_base_get_code_pos(&emit->as->base);
        for (int i = 0; i < emit->scope->id_info_len; i++) {
            id_info_t *id = &emit->scope->id_info[i];
            if (id->kind == ID_INFO_KIND_CELL) {
                assert(id->local_num <= 255);
                mp_asm_base_data(&emit->as->base, 1, id->local_num); 
            }
        }
        emit->n_cell = mp_asm_base_get_code_pos(&emit->as->base) - cell_start;

    }

    ASM_END_PASS(emit->as);

    
    assert(emit->stack_size == 0);
    assert(emit->exc_stack_size == 0);

    if (emit->pass == MP_PASS_EMIT) {
        void *f = mp_asm_base_get_code(&emit->as->base);
        mp_uint_t f_len = mp_asm_base_get_code_size(&emit->as->base);

        mp_raw_code_t **children = emit->emit_common->children;
        if (!emit->do_viper_types) {
            #if MICROPY_EMIT_NATIVE_PRELUDE_SEPARATE_FROM_MACHINE_CODE
            
            
            void *buf = emit->as->base.code_base + emit->prelude_offset;
            size_t n = emit->as->base.code_offset - emit->prelude_offset;
            const uint8_t *prelude_ptr = memcpy(m_new(uint8_t, n), buf, n);
            #else
            
            const uint8_t *prelude_ptr = (const uint8_t *)f + emit->prelude_offset;
            #endif

            
            assert(emit->prelude_ptr_index == emit->emit_common->ct_cur_child);
            if (emit->prelude_ptr_index == 0) {
                children = (void *)prelude_ptr;
            } else {
                children = m_renew(mp_raw_code_t *, children, emit->prelude_ptr_index, emit->prelude_ptr_index + 1);
                children[emit->prelude_ptr_index] = (void *)prelude_ptr;
            }
        }

        mp_emit_glue_assign_native(emit->scope->raw_code,
            emit->do_viper_types ? MP_CODE_NATIVE_VIPER : MP_CODE_NATIVE_PY,
            f, f_len,
            children,
            #if MICROPY_PERSISTENT_CODE_SAVE
            emit->emit_common->ct_cur_child,
            emit->prelude_offset,
            #endif
            emit->scope->scope_flags, 0, 0);
    }

    return true;
}

static void ensure_extra_stack(emit_t *emit, size_t delta) {
    if (emit->stack_size + delta > emit->stack_info_alloc) {
        size_t new_alloc = (emit->stack_size + delta + 8) & ~3;
        emit->stack_info = m_renew(stack_info_t, emit->stack_info, emit->stack_info_alloc, new_alloc);
        emit->stack_info_alloc = new_alloc;
    }
}

static void adjust_stack(emit_t *emit, mp_int_t stack_size_delta) {
    assert((mp_int_t)emit->stack_size + stack_size_delta >= 0);
    assert((mp_int_t)emit->stack_size + stack_size_delta <= (mp_int_t)emit->stack_info_alloc);
    emit->stack_size += stack_size_delta;
    if (emit->pass > MP_PASS_SCOPE && emit->stack_size > emit->scope->stack_size) {
        emit->scope->stack_size = emit->stack_size;
    }
    #if DEBUG_PRINT
    DEBUG_printf("  adjust_stack; stack_size=%d+%d; stack now:", emit->stack_size - stack_size_delta, stack_size_delta);
    for (int i = 0; i < emit->stack_size; i++) {
        stack_info_t *si = &emit->stack_info[i];
        DEBUG_printf(" (v=%d k=%d %d)", si->vtype, si->kind, si->data.u_reg);
    }
    DEBUG_printf("\n");
    #endif
}

static void emit_native_adjust_stack_size(emit_t *emit, mp_int_t delta) {
    DEBUG_printf("adjust_stack_size(" INT_FMT ")\n", delta);
    if (delta > 0) {
        ensure_extra_stack(emit, delta);
    }
    
    
    
    
    
    for (mp_int_t i = 0; i < delta; i++) {
        stack_info_t *si = &emit->stack_info[emit->stack_size + i];
        si->kind = STACK_VALUE;
        
        
        if (delta == 1) {
            si->vtype = emit->saved_stack_vtype;
        } else {
            si->vtype = VTYPE_PYOBJ;
        }
    }
    adjust_stack(emit, delta);
}

static void emit_native_set_source_line(emit_t *emit, mp_uint_t source_line) {
    (void)emit;
    (void)source_line;
}


static void emit_native_pre(emit_t *emit) {
    (void)emit;
}


static stack_info_t *peek_stack(emit_t *emit, mp_uint_t depth) {
    return &emit->stack_info[emit->stack_size - 1 - depth];
}


static vtype_kind_t peek_vtype(emit_t *emit, mp_uint_t depth) {
    if (emit->do_viper_types) {
        return peek_stack(emit, depth)->vtype;
    } else {
        
        return VTYPE_PYOBJ;
    }
}



static void need_reg_single(emit_t *emit, int reg_needed, int skip_stack_pos) {
    skip_stack_pos = emit->stack_size - skip_stack_pos;
    for (int i = 0; i < emit->stack_size; i++) {
        if (i != skip_stack_pos) {
            stack_info_t *si = &emit->stack_info[i];
            if (si->kind == STACK_REG && si->data.u_reg == reg_needed) {
                si->kind = STACK_VALUE;
                emit_native_mov_state_reg(emit, emit->stack_start + i, si->data.u_reg);
            }
        }
    }
}



static void need_reg_all(emit_t *emit) {
    for (int i = 0; i < emit->stack_size; i++) {
        stack_info_t *si = &emit->stack_info[i];
        if (si->kind == STACK_REG) {
            DEBUG_printf("    reg(%u) to local(%u)\n", si->data.u_reg, emit->stack_start + i);
            si->kind = STACK_VALUE;
            emit_native_mov_state_reg(emit, emit->stack_start + i, si->data.u_reg);
        }
    }
}

static vtype_kind_t load_reg_stack_imm(emit_t *emit, int reg_dest, const stack_info_t *si, bool convert_to_pyobj) {
    if (!convert_to_pyobj && emit->do_viper_types) {
        ASM_MOV_REG_IMM(emit->as, reg_dest, si->data.u_imm);
        return si->vtype;
    } else {
        if (si->vtype == VTYPE_PYOBJ) {
            ASM_MOV_REG_IMM(emit->as, reg_dest, si->data.u_imm);
        } else if (si->vtype == VTYPE_BOOL) {
            emit_native_mov_reg_const(emit, reg_dest, MP_F_CONST_FALSE_OBJ + si->data.u_imm);
        } else if (si->vtype == VTYPE_INT || si->vtype == VTYPE_UINT) {
            ASM_MOV_REG_IMM(emit->as, reg_dest, (uintptr_t)MP_OBJ_NEW_SMALL_INT(si->data.u_imm));
        } else if (si->vtype == VTYPE_PTR_NONE) {
            emit_native_mov_reg_const(emit, reg_dest, MP_F_CONST_NONE_OBJ);
        } else {
            mp_raise_NotImplementedError(MP_ERROR_TEXT("conversion to object"));
        }
        return VTYPE_PYOBJ;
    }
}





static void need_stack_settled(emit_t *emit) {
    DEBUG_printf("  need_stack_settled; stack_size=%d\n", emit->stack_size);
    need_reg_all(emit);
    for (int i = 0; i < emit->stack_size; i++) {
        stack_info_t *si = &emit->stack_info[i];
        if (si->kind == STACK_IMM) {
            DEBUG_printf("    imm(" INT_FMT ") to local(%u)\n", si->data.u_imm, emit->stack_start + i);
            si->kind = STACK_VALUE;
            
            si->vtype = load_reg_stack_imm(emit, REG_TEMP1, si, false);
            emit_native_mov_state_reg(emit, emit->stack_start + i, REG_TEMP1);
        }
    }
}


static void emit_access_stack(emit_t *emit, int pos, vtype_kind_t *vtype, int reg_dest) {
    need_reg_single(emit, reg_dest, pos);
    stack_info_t *si = &emit->stack_info[emit->stack_size - pos];
    *vtype = si->vtype;
    switch (si->kind) {
        case STACK_VALUE:
            emit_native_mov_reg_state(emit, reg_dest, emit->stack_start + emit->stack_size - pos);
            break;

        case STACK_REG:
            if (si->data.u_reg != reg_dest) {
                ASM_MOV_REG_REG(emit->as, reg_dest, si->data.u_reg);
            }
            break;

        case STACK_IMM:
            *vtype = load_reg_stack_imm(emit, reg_dest, si, false);
            break;
    }
}



static void emit_fold_stack_top(emit_t *emit, int reg_dest) {
    stack_info_t *si = &emit->stack_info[emit->stack_size - 2];
    si[0] = si[1];
    if (si->kind == STACK_VALUE) {
        
        emit_native_mov_reg_state(emit, reg_dest, emit->stack_start + emit->stack_size - 1);
        si->kind = STACK_REG;
        si->data.u_reg = reg_dest;
    }
    adjust_stack(emit, -1);
}



static void emit_pre_pop_reg_flexible(emit_t *emit, vtype_kind_t *vtype, int *reg_dest, int not_r1, int not_r2) {
    stack_info_t *si = peek_stack(emit, 0);
    if (si->kind == STACK_REG && si->data.u_reg != not_r1 && si->data.u_reg != not_r2) {
        *vtype = si->vtype;
        *reg_dest = si->data.u_reg;
        need_reg_single(emit, *reg_dest, 1);
    } else {
        emit_access_stack(emit, 1, vtype, *reg_dest);
    }
    adjust_stack(emit, -1);
}

static void emit_pre_pop_discard(emit_t *emit) {
    adjust_stack(emit, -1);
}

static void emit_pre_pop_reg(emit_t *emit, vtype_kind_t *vtype, int reg_dest) {
    emit_access_stack(emit, 1, vtype, reg_dest);
    adjust_stack(emit, -1);
}

static void emit_pre_pop_reg_reg(emit_t *emit, vtype_kind_t *vtypea, int rega, vtype_kind_t *vtypeb, int regb) {
    emit_pre_pop_reg(emit, vtypea, rega);
    emit_pre_pop_reg(emit, vtypeb, regb);
}

static void emit_pre_pop_reg_reg_reg(emit_t *emit, vtype_kind_t *vtypea, int rega, vtype_kind_t *vtypeb, int regb, vtype_kind_t *vtypec, int regc) {
    emit_pre_pop_reg(emit, vtypea, rega);
    emit_pre_pop_reg(emit, vtypeb, regb);
    emit_pre_pop_reg(emit, vtypec, regc);
}

static void emit_post(emit_t *emit) {
    (void)emit;
}

static void emit_post_top_set_vtype(emit_t *emit, vtype_kind_t new_vtype) {
    stack_info_t *si = &emit->stack_info[emit->stack_size - 1];
    si->vtype = new_vtype;
}

static void emit_post_push_reg(emit_t *emit, vtype_kind_t vtype, int reg) {
    ensure_extra_stack(emit, 1);
    stack_info_t *si = &emit->stack_info[emit->stack_size];
    si->vtype = vtype;
    si->kind = STACK_REG;
    si->data.u_reg = reg;
    adjust_stack(emit, 1);
}

static void emit_post_push_imm(emit_t *emit, vtype_kind_t vtype, mp_int_t imm) {
    ensure_extra_stack(emit, 1);
    stack_info_t *si = &emit->stack_info[emit->stack_size];
    si->vtype = vtype;
    si->kind = STACK_IMM;
    si->data.u_imm = imm;
    adjust_stack(emit, 1);
}

static void emit_post_push_reg_reg(emit_t *emit, vtype_kind_t vtypea, int rega, vtype_kind_t vtypeb, int regb) {
    emit_post_push_reg(emit, vtypea, rega);
    emit_post_push_reg(emit, vtypeb, regb);
}

static void emit_post_push_reg_reg_reg(emit_t *emit, vtype_kind_t vtypea, int rega, vtype_kind_t vtypeb, int regb, vtype_kind_t vtypec, int regc) {
    emit_post_push_reg(emit, vtypea, rega);
    emit_post_push_reg(emit, vtypeb, regb);
    emit_post_push_reg(emit, vtypec, regc);
}

static void emit_post_push_reg_reg_reg_reg(emit_t *emit, vtype_kind_t vtypea, int rega, vtype_kind_t vtypeb, int regb, vtype_kind_t vtypec, int regc, vtype_kind_t vtyped, int regd) {
    emit_post_push_reg(emit, vtypea, rega);
    emit_post_push_reg(emit, vtypeb, regb);
    emit_post_push_reg(emit, vtypec, regc);
    emit_post_push_reg(emit, vtyped, regd);
}

static void emit_call(emit_t *emit, mp_fun_kind_t fun_kind) {
    need_reg_all(emit);
    ASM_CALL_IND(emit->as, fun_kind);
}

static void emit_call_with_imm_arg(emit_t *emit, mp_fun_kind_t fun_kind, mp_int_t arg_val, int arg_reg) {
    need_reg_all(emit);
    ASM_MOV_REG_IMM(emit->as, arg_reg, arg_val);
    ASM_CALL_IND(emit->as, fun_kind);
}

static void emit_call_with_2_imm_args(emit_t *emit, mp_fun_kind_t fun_kind, mp_int_t arg_val1, int arg_reg1, mp_int_t arg_val2, int arg_reg2) {
    need_reg_all(emit);
    ASM_MOV_REG_IMM(emit->as, arg_reg1, arg_val1);
    ASM_MOV_REG_IMM(emit->as, arg_reg2, arg_val2);
    ASM_CALL_IND(emit->as, fun_kind);
}

static void emit_call_with_qstr_arg(emit_t *emit, mp_fun_kind_t fun_kind, qstr qst, int arg_reg) {
    need_reg_all(emit);
    emit_native_mov_reg_qstr(emit, arg_reg, qst);
    ASM_CALL_IND(emit->as, fun_kind);
}





static void emit_get_stack_pointer_to_reg_for_pop(emit_t *emit, mp_uint_t reg_dest, mp_uint_t n_pop) {
    need_reg_all(emit);

    
    for (mp_uint_t i = 0; i < n_pop; i++) {
        stack_info_t *si = &emit->stack_info[emit->stack_size - 1 - i];
        
        
        if (si->kind == STACK_IMM) {
            si->kind = STACK_VALUE;
            si->vtype = load_reg_stack_imm(emit, reg_dest, si, true);
            emit_native_mov_state_reg(emit, emit->stack_start + emit->stack_size - 1 - i, reg_dest);
        }

        
        assert(si->kind == STACK_VALUE);
    }

    
    for (mp_uint_t i = 0; i < n_pop; i++) {
        stack_info_t *si = &emit->stack_info[emit->stack_size - 1 - i];
        if (si->vtype != VTYPE_PYOBJ) {
            mp_uint_t local_num = emit->stack_start + emit->stack_size - 1 - i;
            emit_native_mov_reg_state(emit, REG_ARG_1, local_num);
            emit_call_with_imm_arg(emit, MP_F_CONVERT_NATIVE_TO_OBJ, si->vtype, REG_ARG_2); 
            emit_native_mov_state_reg(emit, local_num, REG_RET);
            si->vtype = VTYPE_PYOBJ;
            DEBUG_printf("  convert_native_to_obj(local_num=" UINT_FMT ")\n", local_num);
        }
    }

    
    adjust_stack(emit, -n_pop);
    emit_native_mov_reg_state_addr(emit, reg_dest, emit->stack_start + emit->stack_size);
}


static void emit_get_stack_pointer_to_reg_for_push(emit_t *emit, mp_uint_t reg_dest, mp_uint_t n_push) {
    need_reg_all(emit);
    ensure_extra_stack(emit, n_push);
    for (mp_uint_t i = 0; i < n_push; i++) {
        emit->stack_info[emit->stack_size + i].kind = STACK_VALUE;
        emit->stack_info[emit->stack_size + i].vtype = VTYPE_PYOBJ;
    }
    emit_native_mov_reg_state_addr(emit, reg_dest, emit->stack_start + emit->stack_size);
    adjust_stack(emit, n_push);
}

static void emit_native_push_exc_stack(emit_t *emit, uint label, bool is_finally) {
    if (emit->exc_stack_size + 1 > emit->exc_stack_alloc) {
        size_t new_alloc = emit->exc_stack_alloc + 4;
        emit->exc_stack = m_renew(exc_stack_entry_t, emit->exc_stack, emit->exc_stack_alloc, new_alloc);
        emit->exc_stack_alloc = new_alloc;
    }

    exc_stack_entry_t *e = &emit->exc_stack[emit->exc_stack_size++];
    e->label = label;
    e->is_finally = is_finally;
    e->unwind_label = UNWIND_LABEL_UNUSED;
    e->is_active = true;

    ASM_MOV_REG_PCREL(emit->as, REG_RET, label);
    ASM_MOV_LOCAL_REG(emit->as, LOCAL_IDX_EXC_HANDLER_PC(emit), REG_RET);
}

static void emit_native_leave_exc_stack(emit_t *emit, bool start_of_handler) {
    assert(emit->exc_stack_size > 0);

    
    exc_stack_entry_t *e = &emit->exc_stack[emit->exc_stack_size - 1];
    e->is_active = false;

    
    for (--e; e >= emit->exc_stack && !e->is_active; --e) {
    }

    
    if (e < emit->exc_stack) {
        
        if (start_of_handler) {
            
            return;
        }
        ASM_XOR_REG_REG(emit->as, REG_RET, REG_RET);
    } else {
        
        ASM_MOV_REG_PCREL(emit->as, REG_RET, e->label);
    }
    ASM_MOV_LOCAL_REG(emit->as, LOCAL_IDX_EXC_HANDLER_PC(emit), REG_RET);
}

static exc_stack_entry_t *emit_native_pop_exc_stack(emit_t *emit) {
    assert(emit->exc_stack_size > 0);
    exc_stack_entry_t *e = &emit->exc_stack[--emit->exc_stack_size];
    assert(e->is_active == false);
    return e;
}

static void emit_load_reg_with_object(emit_t *emit, int reg, mp_obj_t obj) {
    emit->scope->scope_flags |= MP_SCOPE_FLAG_HASCONSTS;
    size_t table_off = mp_emit_common_use_const_obj(emit->emit_common, obj);
    emit_native_mov_reg_state(emit, REG_TEMP0, LOCAL_IDX_FUN_OBJ(emit));
    ASM_LOAD_REG_REG_OFFSET(emit->as, REG_TEMP0, REG_TEMP0, OFFSETOF_OBJ_FUN_BC_CONTEXT);
    ASM_LOAD_REG_REG_OFFSET(emit->as, REG_TEMP0, REG_TEMP0, OFFSETOF_MODULE_CONTEXT_OBJ_TABLE);
    ASM_LOAD_REG_REG_OFFSET(emit->as, reg, REG_TEMP0, table_off);
}

static void emit_load_reg_with_child(emit_t *emit, int reg, mp_raw_code_t *rc) {
    size_t table_off = mp_emit_common_alloc_const_child(emit->emit_common, rc);
    emit_native_mov_reg_state(emit, REG_TEMP0, LOCAL_IDX_FUN_OBJ(emit));
    ASM_LOAD_REG_REG_OFFSET(emit->as, REG_TEMP0, REG_TEMP0, OFFSETOF_OBJ_FUN_BC_CHILD_TABLE);
    ASM_LOAD_REG_REG_OFFSET(emit->as, reg, REG_TEMP0, table_off);
}

static void emit_native_label_assign(emit_t *emit, mp_uint_t l) {
    DEBUG_printf("label_assign(" UINT_FMT ")\n", l);

    bool is_finally = false;
    if (emit->exc_stack_size > 0) {
        exc_stack_entry_t *e = &emit->exc_stack[emit->exc_stack_size - 1];
        is_finally = e->is_finally && e->label == l;
    }

    if (is_finally) {
        
        vtype_kind_t vtype;
        emit_pre_pop_reg(emit, &vtype, REG_TEMP0);
        ASM_MOV_LOCAL_REG(emit->as, LOCAL_IDX_EXC_VAL(emit), REG_TEMP0);
    }

    emit_native_pre(emit);
    
    need_stack_settled(emit);
    mp_asm_base_label_assign(&emit->as->base, l);
    emit_post(emit);

    if (is_finally) {
        
        emit_native_leave_exc_stack(emit, false);
    }
}

static void emit_native_global_exc_entry(emit_t *emit) {
    

    emit->exit_label = *emit->label_slot;

    if (NEED_GLOBAL_EXC_HANDLER(emit)) {
        mp_uint_t nlr_label = *emit->label_slot + 1;
        mp_uint_t start_label = *emit->label_slot + 2;
        mp_uint_t global_except_label = *emit->label_slot + 3;

        if (!(emit->scope->scope_flags & MP_SCOPE_FLAG_GENERATOR)) {
            
            emit_native_mov_reg_state(emit, REG_ARG_1, LOCAL_IDX_FUN_OBJ(emit));
            ASM_LOAD_REG_REG_OFFSET(emit->as, REG_ARG_1, REG_ARG_1, OFFSETOF_OBJ_FUN_BC_CONTEXT);
            ASM_LOAD_REG_REG_OFFSET(emit->as, REG_ARG_1, REG_ARG_1, OFFSETOF_MODULE_CONTEXT_GLOBALS);
            emit_call(emit, MP_F_NATIVE_SWAP_GLOBALS);

            
            emit_native_mov_state_reg(emit, LOCAL_IDX_OLD_GLOBALS(emit), REG_RET);
        }

        if (emit->scope->exc_stack_size == 0) {
            if (!(emit->scope->scope_flags & MP_SCOPE_FLAG_GENERATOR)) {
                
                ASM_JUMP_IF_REG_ZERO(emit->as, REG_RET, start_label, false);
            }

            
            ASM_MOV_REG_LOCAL_ADDR(emit->as, REG_ARG_1, 0);
            emit_call(emit, MP_F_NLR_PUSH);
            #if N_NLR_SETJMP
            ASM_MOV_REG_LOCAL_ADDR(emit->as, REG_ARG_1, 2);
            emit_call(emit, MP_F_SETJMP);
            #endif
            ASM_JUMP_IF_REG_ZERO(emit->as, REG_RET, start_label, true);
        } else {
            
            ASM_XOR_REG_REG(emit->as, REG_TEMP0, REG_TEMP0);
            ASM_MOV_LOCAL_REG(emit->as, LOCAL_IDX_EXC_HANDLER_UNWIND(emit), REG_TEMP0);

            
            ASM_MOV_REG_PCREL(emit->as, REG_LOCAL_1, start_label);

            
            emit_native_label_assign(emit, nlr_label);
            ASM_MOV_REG_LOCAL_ADDR(emit->as, REG_ARG_1, 0);
            emit_call(emit, MP_F_NLR_PUSH);
            #if N_NLR_SETJMP
            ASM_MOV_REG_LOCAL_ADDR(emit->as, REG_ARG_1, 2);
            emit_call(emit, MP_F_SETJMP);
            #endif
            ASM_JUMP_IF_REG_NONZERO(emit->as, REG_RET, global_except_label, true);

            
            ASM_XOR_REG_REG(emit->as, REG_TEMP0, REG_TEMP0);
            ASM_MOV_LOCAL_REG(emit->as, LOCAL_IDX_EXC_HANDLER_PC(emit), REG_TEMP0);
            ASM_JUMP_REG(emit->as, REG_LOCAL_1);

            
            emit_native_label_assign(emit, global_except_label);
            ASM_MOV_REG_LOCAL(emit->as, REG_LOCAL_1, LOCAL_IDX_EXC_HANDLER_PC(emit));
            ASM_JUMP_IF_REG_NONZERO(emit->as, REG_LOCAL_1, nlr_label, false);
        }

        if (!(emit->scope->scope_flags & MP_SCOPE_FLAG_GENERATOR)) {
            
            emit_native_mov_reg_state(emit, REG_ARG_1, LOCAL_IDX_OLD_GLOBALS(emit));
            emit_call(emit, MP_F_NATIVE_SWAP_GLOBALS);
        }

        if (emit->scope->scope_flags & MP_SCOPE_FLAG_GENERATOR) {
            
            ASM_MOV_REG_LOCAL(emit->as, REG_TEMP0, LOCAL_IDX_EXC_VAL(emit));
            ASM_STORE_REG_REG_OFFSET(emit->as, REG_TEMP0, REG_GENERATOR_STATE, OFFSETOF_CODE_STATE_STATE);

            
            ASM_MOV_REG_IMM(emit->as, REG_PARENT_RET, MP_VM_RETURN_EXCEPTION);

            ASM_EXIT(emit->as);
        } else {
            
            ASM_MOV_REG_LOCAL(emit->as, REG_ARG_1, LOCAL_IDX_EXC_VAL(emit));
            emit_call(emit, MP_F_NATIVE_RAISE);
        }

        
        emit_native_label_assign(emit, start_label);

        if (emit->scope->scope_flags & MP_SCOPE_FLAG_GENERATOR) {
            emit_native_mov_reg_state(emit, REG_TEMP0, LOCAL_IDX_GEN_PC(emit));
            ASM_JUMP_REG(emit->as, REG_TEMP0);
            emit->start_offset = mp_asm_base_get_code_pos(&emit->as->base);

            

            
            ASM_MOV_REG_LOCAL(emit->as, REG_ARG_1, LOCAL_IDX_EXC_VAL(emit));
            emit_call(emit, MP_F_NATIVE_RAISE);
        }
    }
}

static void emit_native_global_exc_exit(emit_t *emit) {
    
    emit_native_label_assign(emit, emit->exit_label);

    if (NEED_GLOBAL_EXC_HANDLER(emit)) {
        
        if (!(emit->scope->scope_flags & MP_SCOPE_FLAG_GENERATOR)) {
            emit_native_mov_reg_state(emit, REG_ARG_1, LOCAL_IDX_OLD_GLOBALS(emit));

            if (emit->scope->exc_stack_size == 0) {
                
                ASM_JUMP_IF_REG_ZERO(emit->as, REG_ARG_1, emit->exit_label + 1, false);
            }

            
            emit_call(emit, MP_F_NATIVE_SWAP_GLOBALS);
        }

        
        emit_call(emit, MP_F_NLR_POP);

        if (!(emit->scope->scope_flags & MP_SCOPE_FLAG_GENERATOR)) {
            if (emit->scope->exc_stack_size == 0) {
                
                emit_native_label_assign(emit, emit->exit_label + 1);
            }
        }

        
        ASM_MOV_REG_LOCAL(emit->as, REG_PARENT_RET, LOCAL_IDX_RET_VAL(emit));
    }

    ASM_EXIT(emit->as);
}

static void emit_native_import_name(emit_t *emit, qstr qst) {
    DEBUG_printf("import_name %s\n", qstr_str(qst));

    
    
    
    bool orig_do_viper_types = emit->do_viper_types;
    emit->do_viper_types = false;
    vtype_kind_t vtype_fromlist;
    vtype_kind_t vtype_level;
    emit_pre_pop_reg_reg(emit, &vtype_fromlist, REG_ARG_2, &vtype_level, REG_ARG_3);
    assert(vtype_fromlist == VTYPE_PYOBJ);
    assert(vtype_level == VTYPE_PYOBJ);
    emit->do_viper_types = orig_do_viper_types;

    emit_call_with_qstr_arg(emit, MP_F_IMPORT_NAME, qst, REG_ARG_1); 
    emit_post_push_reg(emit, VTYPE_PYOBJ, REG_RET);
}

static void emit_native_import_from(emit_t *emit, qstr qst) {
    DEBUG_printf("import_from %s\n", qstr_str(qst));
    emit_native_pre(emit);
    vtype_kind_t vtype_module;
    emit_access_stack(emit, 1, &vtype_module, REG_ARG_1); 
    assert(vtype_module == VTYPE_PYOBJ);
    emit_call_with_qstr_arg(emit, MP_F_IMPORT_FROM, qst, REG_ARG_2); 
    emit_post_push_reg(emit, VTYPE_PYOBJ, REG_RET);
}

static void emit_native_import_star(emit_t *emit) {
    DEBUG_printf("import_star\n");
    vtype_kind_t vtype_module;
    emit_pre_pop_reg(emit, &vtype_module, REG_ARG_1); 
    assert(vtype_module == VTYPE_PYOBJ);
    emit_call(emit, MP_F_IMPORT_ALL);
    emit_post(emit);
}

static void emit_native_import(emit_t *emit, qstr qst, int kind) {
    if (kind == MP_EMIT_IMPORT_NAME) {
        emit_native_import_name(emit, qst);
    } else if (kind == MP_EMIT_IMPORT_FROM) {
        emit_native_import_from(emit, qst);
    } else {
        emit_native_import_star(emit);
    }
}

static void emit_native_load_const_tok(emit_t *emit, mp_token_kind_t tok) {
    DEBUG_printf("load_const_tok(tok=%u)\n", tok);
    if (tok == MP_TOKEN_ELLIPSIS) {
        emit_native_load_const_obj(emit, MP_OBJ_FROM_PTR(&mp_const_ellipsis_obj));
    } else {
        emit_native_pre(emit);
        if (tok == MP_TOKEN_KW_NONE) {
            emit_post_push_imm(emit, VTYPE_PTR_NONE, 0);
        } else {
            emit_post_push_imm(emit, VTYPE_BOOL, tok == MP_TOKEN_KW_FALSE ? 0 : 1);
        }
    }
}

static void emit_native_load_const_small_int(emit_t *emit, mp_int_t arg) {
    DEBUG_printf("load_const_small_int(int=" INT_FMT ")\n", arg);
    emit_native_pre(emit);
    emit_post_push_imm(emit, VTYPE_INT, arg);
}

static void emit_native_load_const_str(emit_t *emit, qstr qst) {
    emit_native_pre(emit);
    
    
     
    {
        need_reg_single(emit, REG_TEMP0, 0);
        emit_native_mov_reg_qstr_obj(emit, REG_TEMP0, qst);
        emit_post_push_reg(emit, VTYPE_PYOBJ, REG_TEMP0);
    }
}

static void emit_native_load_const_obj(emit_t *emit, mp_obj_t obj) {
    emit_native_pre(emit);
    need_reg_single(emit, REG_RET, 0);
    emit_load_reg_with_object(emit, REG_RET, obj);
    emit_post_push_reg(emit, VTYPE_PYOBJ, REG_RET);
}

static void emit_native_load_null(emit_t *emit) {
    emit_native_pre(emit);
    emit_post_push_imm(emit, VTYPE_PYOBJ, 0);
}

static void emit_native_load_fast(emit_t *emit, qstr qst, mp_uint_t local_num) {
    DEBUG_printf("load_fast(%s, " UINT_FMT ")\n", qstr_str(qst), local_num);
    vtype_kind_t vtype = emit->local_vtype[local_num];
    if (vtype == VTYPE_UNBOUND) {
        EMIT_NATIVE_VIPER_TYPE_ERROR(emit, MP_ERROR_TEXT("local '%q' used before type known"), qst);
    }
    emit_native_pre(emit);
    if (local_num < MAX_REGS_FOR_LOCAL_VARS && CAN_USE_REGS_FOR_LOCALS(emit)) {
        emit_post_push_reg(emit, vtype, reg_local_table[local_num]);
    } else {
        need_reg_single(emit, REG_TEMP0, 0);
        emit_native_mov_reg_state(emit, REG_TEMP0, LOCAL_IDX_LOCAL_VAR(emit, local_num));
        emit_post_push_reg(emit, vtype, REG_TEMP0);
    }
}

static void emit_native_load_deref(emit_t *emit, qstr qst, mp_uint_t local_num) {
    DEBUG_printf("load_deref(%s, " UINT_FMT ")\n", qstr_str(qst), local_num);
    need_reg_single(emit, REG_RET, 0);
    emit_native_load_fast(emit, qst, local_num);
    vtype_kind_t vtype;
    int reg_base = REG_RET;
    emit_pre_pop_reg_flexible(emit, &vtype, &reg_base, -1, -1);
    ASM_LOAD_REG_REG_OFFSET(emit->as, REG_RET, reg_base, 1);
    
    emit_post_push_reg(emit, VTYPE_PYOBJ, REG_RET);
}

static void emit_native_load_local(emit_t *emit, qstr qst, mp_uint_t local_num, int kind) {
    if (kind == MP_EMIT_IDOP_LOCAL_FAST) {
        emit_native_load_fast(emit, qst, local_num);
    } else {
        emit_native_load_deref(emit, qst, local_num);
    }
}

static void emit_native_load_global(emit_t *emit, qstr qst, int kind) {
    MP_STATIC_ASSERT(MP_F_LOAD_NAME + MP_EMIT_IDOP_GLOBAL_NAME == MP_F_LOAD_NAME);
    MP_STATIC_ASSERT(MP_F_LOAD_NAME + MP_EMIT_IDOP_GLOBAL_GLOBAL == MP_F_LOAD_GLOBAL);
    emit_native_pre(emit);
    if (kind == MP_EMIT_IDOP_GLOBAL_NAME) {
        DEBUG_printf("load_name(%s)\n", qstr_str(qst));
    } else {
        DEBUG_printf("load_global(%s)\n", qstr_str(qst));
        if (emit->do_viper_types) {
            
            int native_type = mp_native_type_from_qstr(qst);
            if (native_type >= MP_NATIVE_TYPE_BOOL) {
                emit_post_push_imm(emit, VTYPE_BUILTIN_CAST, native_type);
                return;
            }
        }
    }
    emit_call_with_qstr_arg(emit, MP_F_LOAD_NAME + kind, qst, REG_ARG_1);
    emit_post_push_reg(emit, VTYPE_PYOBJ, REG_RET);
}

static void emit_native_load_attr(emit_t *emit, qstr qst) {
    
    
    
    
    vtype_kind_t vtype_base;
    emit_pre_pop_reg(emit, &vtype_base, REG_ARG_1); 
    assert(vtype_base == VTYPE_PYOBJ);
    emit_call_with_qstr_arg(emit, MP_F_LOAD_ATTR, qst, REG_ARG_2); 
    emit_post_push_reg(emit, VTYPE_PYOBJ, REG_RET);
}

static void emit_native_load_method(emit_t *emit, qstr qst, bool is_super) {
    if (is_super) {
        emit_get_stack_pointer_to_reg_for_pop(emit, REG_ARG_2, 3); 
        emit_get_stack_pointer_to_reg_for_push(emit, REG_ARG_2, 2); 
        emit_call_with_qstr_arg(emit, MP_F_LOAD_SUPER_METHOD, qst, REG_ARG_1); 
    } else {
        vtype_kind_t vtype_base;
        emit_pre_pop_reg(emit, &vtype_base, REG_ARG_1); 
        assert(vtype_base == VTYPE_PYOBJ);
        emit_get_stack_pointer_to_reg_for_push(emit, REG_ARG_3, 2); 
        emit_call_with_qstr_arg(emit, MP_F_LOAD_METHOD, qst, REG_ARG_2); 
    }
}

static void emit_native_load_build_class(emit_t *emit) {
    emit_native_pre(emit);
    emit_call(emit, MP_F_LOAD_BUILD_CLASS);
    emit_post_push_reg(emit, VTYPE_PYOBJ, REG_RET);
}

static void emit_native_load_subscr(emit_t *emit) {
    DEBUG_printf("load_subscr\n");
    

    
    
    vtype_kind_t vtype_base = peek_vtype(emit, 1);

    if (vtype_base == VTYPE_PYOBJ) {
        
        
        vtype_kind_t vtype_index = peek_vtype(emit, 0);
        if (vtype_index == VTYPE_PYOBJ) {
            emit_pre_pop_reg(emit, &vtype_index, REG_ARG_2);
        } else {
            emit_pre_pop_reg(emit, &vtype_index, REG_ARG_1);
            emit_call_with_imm_arg(emit, MP_F_CONVERT_NATIVE_TO_OBJ, vtype_index, REG_ARG_2); 
            ASM_MOV_REG_REG(emit->as, REG_ARG_2, REG_RET);
        }
        emit_pre_pop_reg(emit, &vtype_base, REG_ARG_1);
        emit_call_with_imm_arg(emit, MP_F_OBJ_SUBSCR, (mp_uint_t)MP_OBJ_SENTINEL, REG_ARG_3);
        emit_post_push_reg(emit, VTYPE_PYOBJ, REG_RET);
    } else {
        
        
        
        
        stack_info_t *top = peek_stack(emit, 0);
        if (top->vtype == VTYPE_INT && top->kind == STACK_IMM) {
            
            mp_int_t index_value = top->data.u_imm;
            emit_pre_pop_discard(emit); 
            int reg_base = REG_ARG_1;
            int reg_index = REG_ARG_2;
            emit_pre_pop_reg_flexible(emit, &vtype_base, &reg_base, reg_index, reg_index);
            need_reg_single(emit, REG_RET, 0);
            switch (vtype_base) {
                case VTYPE_PTR8: {
                    
                    
                    if (index_value != 0) {
                        
                        #if N_THUMB
                        if (index_value > 0 && index_value < 32) {
                            asm_thumb_ldrb_rlo_rlo_i5(emit->as, REG_RET, reg_base, index_value);
                            break;
                        }
                        #endif
                        need_reg_single(emit, reg_index, 0);
                        ASM_MOV_REG_IMM(emit->as, reg_index, index_value);
                        ASM_ADD_REG_REG(emit->as, reg_index, reg_base); 
                        reg_base = reg_index;
                    }
                    ASM_LOAD8_REG_REG(emit->as, REG_RET, reg_base); 
                    break;
                }
                case VTYPE_PTR16: {
                    
                    if (index_value != 0) {
                        
                        #if N_THUMB
                        if (index_value > 0 && index_value < 32) {
                            asm_thumb_ldrh_rlo_rlo_i5(emit->as, REG_RET, reg_base, index_value);
                            break;
                        }
                        #endif
                        need_reg_single(emit, reg_index, 0);
                        ASM_MOV_REG_IMM(emit->as, reg_index, index_value << 1);
                        ASM_ADD_REG_REG(emit->as, reg_index, reg_base); 
                        reg_base = reg_index;
                    }
                    ASM_LOAD16_REG_REG(emit->as, REG_RET, reg_base); 
                    break;
                }
                case VTYPE_PTR32: {
                    
                    if (index_value != 0) {
                        
                        #if N_THUMB
                        if (index_value > 0 && index_value < 32) {
                            asm_thumb_ldr_rlo_rlo_i5(emit->as, REG_RET, reg_base, index_value);
                            break;
                        }
                        #endif
                        need_reg_single(emit, reg_index, 0);
                        ASM_MOV_REG_IMM(emit->as, reg_index, index_value << 2);
                        ASM_ADD_REG_REG(emit->as, reg_index, reg_base); 
                        reg_base = reg_index;
                    }
                    ASM_LOAD32_REG_REG(emit->as, REG_RET, reg_base); 
                    break;
                }
                default:
                    EMIT_NATIVE_VIPER_TYPE_ERROR(emit,
                        MP_ERROR_TEXT("can't load from '%q'"), vtype_to_qstr(vtype_base));
            }
        } else {
            
            vtype_kind_t vtype_index;
            int reg_index = REG_ARG_2;
            emit_pre_pop_reg_flexible(emit, &vtype_index, &reg_index, REG_ARG_1, REG_ARG_1);
            emit_pre_pop_reg(emit, &vtype_base, REG_ARG_1);
            need_reg_single(emit, REG_RET, 0);
            if (vtype_index != VTYPE_INT && vtype_index != VTYPE_UINT) {
                EMIT_NATIVE_VIPER_TYPE_ERROR(emit,
                    MP_ERROR_TEXT("can't load with '%q' index"), vtype_to_qstr(vtype_index));
            }
            switch (vtype_base) {
                case VTYPE_PTR8: {
                    
                    
                    ASM_ADD_REG_REG(emit->as, REG_ARG_1, reg_index); 
                    ASM_LOAD8_REG_REG(emit->as, REG_RET, REG_ARG_1); 
                    break;
                }
                case VTYPE_PTR16: {
                    
                    ASM_ADD_REG_REG(emit->as, REG_ARG_1, reg_index); 
                    ASM_ADD_REG_REG(emit->as, REG_ARG_1, reg_index); 
                    ASM_LOAD16_REG_REG(emit->as, REG_RET, REG_ARG_1); 
                    break;
                }
                case VTYPE_PTR32: {
                    
                    ASM_ADD_REG_REG(emit->as, REG_ARG_1, reg_index); 
                    ASM_ADD_REG_REG(emit->as, REG_ARG_1, reg_index); 
                    ASM_ADD_REG_REG(emit->as, REG_ARG_1, reg_index); 
                    ASM_ADD_REG_REG(emit->as, REG_ARG_1, reg_index); 
                    ASM_LOAD32_REG_REG(emit->as, REG_RET, REG_ARG_1); 
                    break;
                }
                default:
                    EMIT_NATIVE_VIPER_TYPE_ERROR(emit,
                        MP_ERROR_TEXT("can't load from '%q'"), vtype_to_qstr(vtype_base));
            }
        }
        emit_post_push_reg(emit, VTYPE_INT, REG_RET);
    }
}

static void emit_native_store_fast(emit_t *emit, qstr qst, mp_uint_t local_num) {
    vtype_kind_t vtype;
    if (local_num < MAX_REGS_FOR_LOCAL_VARS && CAN_USE_REGS_FOR_LOCALS(emit)) {
        emit_pre_pop_reg(emit, &vtype, reg_local_table[local_num]);
    } else {
        emit_pre_pop_reg(emit, &vtype, REG_TEMP0);
        emit_native_mov_state_reg(emit, LOCAL_IDX_LOCAL_VAR(emit, local_num), REG_TEMP0);
    }
    emit_post(emit);

    
    if (emit->local_vtype[local_num] == VTYPE_UNBOUND) {
        
        emit->local_vtype[local_num] = vtype;
    } else if (emit->local_vtype[local_num] != vtype) {
        
        EMIT_NATIVE_VIPER_TYPE_ERROR(emit,
            MP_ERROR_TEXT("local '%q' has type '%q' but source is '%q'"),
            qst, vtype_to_qstr(emit->local_vtype[local_num]), vtype_to_qstr(vtype));
    }
}

static void emit_native_store_deref(emit_t *emit, qstr qst, mp_uint_t local_num) {
    DEBUG_printf("store_deref(%s, " UINT_FMT ")\n", qstr_str(qst), local_num);
    need_reg_single(emit, REG_TEMP0, 0);
    need_reg_single(emit, REG_TEMP1, 0);
    emit_native_load_fast(emit, qst, local_num);
    vtype_kind_t vtype;
    int reg_base = REG_TEMP0;
    emit_pre_pop_reg_flexible(emit, &vtype, &reg_base, -1, -1);
    int reg_src = REG_TEMP1;
    emit_pre_pop_reg_flexible(emit, &vtype, &reg_src, reg_base, reg_base);
    ASM_STORE_REG_REG_OFFSET(emit->as, reg_src, reg_base, 1);
    emit_post(emit);
}

static void emit_native_store_local(emit_t *emit, qstr qst, mp_uint_t local_num, int kind) {
    if (kind == MP_EMIT_IDOP_LOCAL_FAST) {
        emit_native_store_fast(emit, qst, local_num);
    } else {
        emit_native_store_deref(emit, qst, local_num);
    }
}

static void emit_native_store_global(emit_t *emit, qstr qst, int kind) {
    MP_STATIC_ASSERT(MP_F_STORE_NAME + MP_EMIT_IDOP_GLOBAL_NAME == MP_F_STORE_NAME);
    MP_STATIC_ASSERT(MP_F_STORE_NAME + MP_EMIT_IDOP_GLOBAL_GLOBAL == MP_F_STORE_GLOBAL);
    if (kind == MP_EMIT_IDOP_GLOBAL_NAME) {
        
        vtype_kind_t vtype;
        emit_pre_pop_reg(emit, &vtype, REG_ARG_2);
        assert(vtype == VTYPE_PYOBJ);
    } else {
        vtype_kind_t vtype = peek_vtype(emit, 0);
        if (vtype == VTYPE_PYOBJ) {
            emit_pre_pop_reg(emit, &vtype, REG_ARG_2);
        } else {
            emit_pre_pop_reg(emit, &vtype, REG_ARG_1);
            emit_call_with_imm_arg(emit, MP_F_CONVERT_NATIVE_TO_OBJ, vtype, REG_ARG_2); 
            ASM_MOV_REG_REG(emit->as, REG_ARG_2, REG_RET);
        }
    }
    emit_call_with_qstr_arg(emit, MP_F_STORE_NAME + kind, qst, REG_ARG_1); 
    emit_post(emit);
}

static void emit_native_store_attr(emit_t *emit, qstr qst) {
    vtype_kind_t vtype_base;
    vtype_kind_t vtype_val = peek_vtype(emit, 1);
    if (vtype_val == VTYPE_PYOBJ) {
        emit_pre_pop_reg_reg(emit, &vtype_base, REG_ARG_1, &vtype_val, REG_ARG_3); 
    } else {
        emit_access_stack(emit, 2, &vtype_val, REG_ARG_1); 
        emit_call_with_imm_arg(emit, MP_F_CONVERT_NATIVE_TO_OBJ, vtype_val, REG_ARG_2); 
        ASM_MOV_REG_REG(emit->as, REG_ARG_3, REG_RET); 
        emit_pre_pop_reg(emit, &vtype_base, REG_ARG_1); 
        adjust_stack(emit, -1); 
    }
    assert(vtype_base == VTYPE_PYOBJ);
    emit_call_with_qstr_arg(emit, MP_F_STORE_ATTR, qst, REG_ARG_2); 
    emit_post(emit);
}

static void emit_native_store_subscr(emit_t *emit) {
    DEBUG_printf("store_subscr\n");
    

    
    
    vtype_kind_t vtype_base = peek_vtype(emit, 1);

    if (vtype_base == VTYPE_PYOBJ) {
        
        vtype_kind_t vtype_index = peek_vtype(emit, 0);
        vtype_kind_t vtype_value = peek_vtype(emit, 2);
        if (vtype_index != VTYPE_PYOBJ || vtype_value != VTYPE_PYOBJ) {
            
            
            emit_get_stack_pointer_to_reg_for_pop(emit, REG_ARG_1, 3);
            adjust_stack(emit, 3);
        }
        emit_pre_pop_reg_reg_reg(emit, &vtype_index, REG_ARG_2, &vtype_base, REG_ARG_1, &vtype_value, REG_ARG_3);
        emit_call(emit, MP_F_OBJ_SUBSCR);
    } else {
        
        
        
        
        stack_info_t *top = peek_stack(emit, 0);
        if (top->vtype == VTYPE_INT && top->kind == STACK_IMM) {
            
            mp_int_t index_value = top->data.u_imm;
            emit_pre_pop_discard(emit); 
            vtype_kind_t vtype_value;
            int reg_base = REG_ARG_1;
            int reg_index = REG_ARG_2;
            int reg_value = REG_ARG_3;
            emit_pre_pop_reg_flexible(emit, &vtype_base, &reg_base, reg_index, reg_value);
            #if N_X64 || N_X86
            
            emit_pre_pop_reg(emit, &vtype_value, reg_value);
            #else
            emit_pre_pop_reg_flexible(emit, &vtype_value, &reg_value, reg_base, reg_index);
            #endif
            if (vtype_value != VTYPE_BOOL && vtype_value != VTYPE_INT && vtype_value != VTYPE_UINT) {
                EMIT_NATIVE_VIPER_TYPE_ERROR(emit,
                    MP_ERROR_TEXT("can't store '%q'"), vtype_to_qstr(vtype_value));
            }
            switch (vtype_base) {
                case VTYPE_PTR8: {
                    
                    
                    if (index_value != 0) {
                        
                        #if N_THUMB
                        if (index_value > 0 && index_value < 32) {
                            asm_thumb_strb_rlo_rlo_i5(emit->as, reg_value, reg_base, index_value);
                            break;
                        }
                        #endif
                        ASM_MOV_REG_IMM(emit->as, reg_index, index_value);
                        #if N_ARM
                        asm_arm_strb_reg_reg_reg(emit->as, reg_value, reg_base, reg_index);
                        return;
                        #endif
                        ASM_ADD_REG_REG(emit->as, reg_index, reg_base); 
                        reg_base = reg_index;
                    }
                    ASM_STORE8_REG_REG(emit->as, reg_value, reg_base); 
                    break;
                }
                case VTYPE_PTR16: {
                    
                    if (index_value != 0) {
                        
                        #if N_THUMB
                        if (index_value > 0 && index_value < 32) {
                            asm_thumb_strh_rlo_rlo_i5(emit->as, reg_value, reg_base, index_value);
                            break;
                        }
                        #endif
                        ASM_MOV_REG_IMM(emit->as, reg_index, index_value << 1);
                        ASM_ADD_REG_REG(emit->as, reg_index, reg_base); 
                        reg_base = reg_index;
                    }
                    ASM_STORE16_REG_REG(emit->as, reg_value, reg_base); 
                    break;
                }
                case VTYPE_PTR32: {
                    
                    if (index_value != 0) {
                        
                        #if N_THUMB
                        if (index_value > 0 && index_value < 32) {
                            asm_thumb_str_rlo_rlo_i5(emit->as, reg_value, reg_base, index_value);
                            break;
                        }
                        #endif
                        #if N_ARM
                        ASM_MOV_REG_IMM(emit->as, reg_index, index_value);
                        asm_arm_str_reg_reg_reg(emit->as, reg_value, reg_base, reg_index);
                        return;
                        #endif
                        ASM_MOV_REG_IMM(emit->as, reg_index, index_value << 2);
                        ASM_ADD_REG_REG(emit->as, reg_index, reg_base); 
                        reg_base = reg_index;
                    }
                    ASM_STORE32_REG_REG(emit->as, reg_value, reg_base); 
                    break;
                }
                default:
                    EMIT_NATIVE_VIPER_TYPE_ERROR(emit,
                        MP_ERROR_TEXT("can't store to '%q'"), vtype_to_qstr(vtype_base));
            }
        } else {
            
            vtype_kind_t vtype_index, vtype_value;
            int reg_index = REG_ARG_2;
            int reg_value = REG_ARG_3;
            emit_pre_pop_reg_flexible(emit, &vtype_index, &reg_index, REG_ARG_1, reg_value);
            emit_pre_pop_reg(emit, &vtype_base, REG_ARG_1);
            if (vtype_index != VTYPE_INT && vtype_index != VTYPE_UINT) {
                EMIT_NATIVE_VIPER_TYPE_ERROR(emit,
                    MP_ERROR_TEXT("can't store with '%q' index"), vtype_to_qstr(vtype_index));
            }
            #if N_X64 || N_X86
            
            emit_pre_pop_reg(emit, &vtype_value, reg_value);
            #else
            emit_pre_pop_reg_flexible(emit, &vtype_value, &reg_value, REG_ARG_1, reg_index);
            #endif
            if (vtype_value != VTYPE_BOOL && vtype_value != VTYPE_INT && vtype_value != VTYPE_UINT) {
                EMIT_NATIVE_VIPER_TYPE_ERROR(emit,
                    MP_ERROR_TEXT("can't store '%q'"), vtype_to_qstr(vtype_value));
            }
            switch (vtype_base) {
                case VTYPE_PTR8: {
                    
                    
                    #if N_ARM
                    asm_arm_strb_reg_reg_reg(emit->as, reg_value, REG_ARG_1, reg_index);
                    break;
                    #endif
                    ASM_ADD_REG_REG(emit->as, REG_ARG_1, reg_index); 
                    ASM_STORE8_REG_REG(emit->as, reg_value, REG_ARG_1); 
                    break;
                }
                case VTYPE_PTR16: {
                    
                    #if N_ARM
                    asm_arm_strh_reg_reg_reg(emit->as, reg_value, REG_ARG_1, reg_index);
                    break;
                    #endif
                    ASM_ADD_REG_REG(emit->as, REG_ARG_1, reg_index); 
                    ASM_ADD_REG_REG(emit->as, REG_ARG_1, reg_index); 
                    ASM_STORE16_REG_REG(emit->as, reg_value, REG_ARG_1); 
                    break;
                }
                case VTYPE_PTR32: {
                    
                    #if N_ARM
                    asm_arm_str_reg_reg_reg(emit->as, reg_value, REG_ARG_1, reg_index);
                    break;
                    #endif
                    ASM_ADD_REG_REG(emit->as, REG_ARG_1, reg_index); 
                    ASM_ADD_REG_REG(emit->as, REG_ARG_1, reg_index); 
                    ASM_ADD_REG_REG(emit->as, REG_ARG_1, reg_index); 
                    ASM_ADD_REG_REG(emit->as, REG_ARG_1, reg_index); 
                    ASM_STORE32_REG_REG(emit->as, reg_value, REG_ARG_1); 
                    break;
                }
                default:
                    EMIT_NATIVE_VIPER_TYPE_ERROR(emit,
                        MP_ERROR_TEXT("can't store to '%q'"), vtype_to_qstr(vtype_base));
            }
        }

    }
}

static void emit_native_delete_local(emit_t *emit, qstr qst, mp_uint_t local_num, int kind) {
    if (kind == MP_EMIT_IDOP_LOCAL_FAST) {
        
        
        
        emit_native_load_const_tok(emit, MP_TOKEN_KW_NONE);
        emit_native_store_fast(emit, qst, local_num);
    } else {
        
    }
}

static void emit_native_delete_global(emit_t *emit, qstr qst, int kind) {
    MP_STATIC_ASSERT(MP_F_DELETE_NAME + MP_EMIT_IDOP_GLOBAL_NAME == MP_F_DELETE_NAME);
    MP_STATIC_ASSERT(MP_F_DELETE_NAME + MP_EMIT_IDOP_GLOBAL_GLOBAL == MP_F_DELETE_GLOBAL);
    emit_native_pre(emit);
    emit_call_with_qstr_arg(emit, MP_F_DELETE_NAME + kind, qst, REG_ARG_1);
    emit_post(emit);
}

static void emit_native_delete_attr(emit_t *emit, qstr qst) {
    vtype_kind_t vtype_base;
    emit_pre_pop_reg(emit, &vtype_base, REG_ARG_1); 
    assert(vtype_base == VTYPE_PYOBJ);
    ASM_XOR_REG_REG(emit->as, REG_ARG_3, REG_ARG_3); 
    emit_call_with_qstr_arg(emit, MP_F_STORE_ATTR, qst, REG_ARG_2); 
    emit_post(emit);
}

static void emit_native_delete_subscr(emit_t *emit) {
    vtype_kind_t vtype_index, vtype_base;
    emit_pre_pop_reg_reg(emit, &vtype_index, REG_ARG_2, &vtype_base, REG_ARG_1); 
    assert(vtype_index == VTYPE_PYOBJ);
    assert(vtype_base == VTYPE_PYOBJ);
    emit_call_with_imm_arg(emit, MP_F_OBJ_SUBSCR, (mp_uint_t)MP_OBJ_NULL, REG_ARG_3);
}

static void emit_native_subscr(emit_t *emit, int kind) {
    if (kind == MP_EMIT_SUBSCR_LOAD) {
        emit_native_load_subscr(emit);
    } else if (kind == MP_EMIT_SUBSCR_STORE) {
        emit_native_store_subscr(emit);
    } else {
        emit_native_delete_subscr(emit);
    }
}

static void emit_native_attr(emit_t *emit, qstr qst, int kind) {
    if (kind == MP_EMIT_ATTR_LOAD) {
        emit_native_load_attr(emit, qst);
    } else if (kind == MP_EMIT_ATTR_STORE) {
        emit_native_store_attr(emit, qst);
    } else {
        emit_native_delete_attr(emit, qst);
    }
}

static void emit_native_dup_top(emit_t *emit) {
    DEBUG_printf("dup_top\n");
    vtype_kind_t vtype;
    int reg = REG_TEMP0;
    emit_pre_pop_reg_flexible(emit, &vtype, &reg, -1, -1);
    emit_post_push_reg_reg(emit, vtype, reg, vtype, reg);
}

static void emit_native_dup_top_two(emit_t *emit) {
    vtype_kind_t vtype0, vtype1;
    emit_pre_pop_reg_reg(emit, &vtype0, REG_TEMP0, &vtype1, REG_TEMP1);
    emit_post_push_reg_reg_reg_reg(emit, vtype1, REG_TEMP1, vtype0, REG_TEMP0, vtype1, REG_TEMP1, vtype0, REG_TEMP0);
}

static void emit_native_pop_top(emit_t *emit) {
    DEBUG_printf("pop_top\n");
    emit_pre_pop_discard(emit);
    emit_post(emit);
}

static void emit_native_rot_two(emit_t *emit) {
    DEBUG_printf("rot_two\n");
    vtype_kind_t vtype0, vtype1;
    emit_pre_pop_reg_reg(emit, &vtype0, REG_TEMP0, &vtype1, REG_TEMP1);
    emit_post_push_reg_reg(emit, vtype0, REG_TEMP0, vtype1, REG_TEMP1);
}

static void emit_native_rot_three(emit_t *emit) {
    DEBUG_printf("rot_three\n");
    vtype_kind_t vtype0, vtype1, vtype2;
    emit_pre_pop_reg_reg_reg(emit, &vtype0, REG_TEMP0, &vtype1, REG_TEMP1, &vtype2, REG_TEMP2);
    emit_post_push_reg_reg_reg(emit, vtype0, REG_TEMP0, vtype2, REG_TEMP2, vtype1, REG_TEMP1);
}

static void emit_native_jump(emit_t *emit, mp_uint_t label) {
    DEBUG_printf("jump(label=" UINT_FMT ")\n", label);
    emit_native_pre(emit);
    
    need_stack_settled(emit);
    ASM_JUMP(emit->as, label);
    emit_post(emit);
    mp_asm_base_suppress_code(&emit->as->base);
}

static void emit_native_jump_helper(emit_t *emit, bool cond, mp_uint_t label, bool pop) {
    vtype_kind_t vtype = peek_vtype(emit, 0);
    if (vtype == VTYPE_PYOBJ) {
        emit_pre_pop_reg(emit, &vtype, REG_ARG_1);
        if (!pop) {
            adjust_stack(emit, 1);
        }
        emit_call(emit, MP_F_OBJ_IS_TRUE);
    } else {
        emit_pre_pop_reg(emit, &vtype, REG_RET);
        if (!pop) {
            adjust_stack(emit, 1);
        }
        if (!(vtype == VTYPE_BOOL || vtype == VTYPE_INT || vtype == VTYPE_UINT)) {
            EMIT_NATIVE_VIPER_TYPE_ERROR(emit,
                MP_ERROR_TEXT("can't implicitly convert '%q' to 'bool'"), vtype_to_qstr(vtype));
        }
    }
    
    
    if (!pop) {
        emit->saved_stack_vtype = vtype;
    }
    
    need_stack_settled(emit);
    
    if (cond) {
        ASM_JUMP_IF_REG_NONZERO(emit->as, REG_RET, label, vtype == VTYPE_PYOBJ);
    } else {
        ASM_JUMP_IF_REG_ZERO(emit->as, REG_RET, label, vtype == VTYPE_PYOBJ);
    }
    if (!pop) {
        adjust_stack(emit, -1);
    }
    emit_post(emit);
}

static void emit_native_pop_jump_if(emit_t *emit, bool cond, mp_uint_t label) {
    DEBUG_printf("pop_jump_if(cond=%u, label=" UINT_FMT ")\n", cond, label);
    emit_native_jump_helper(emit, cond, label, true);
}

static void emit_native_jump_if_or_pop(emit_t *emit, bool cond, mp_uint_t label) {
    DEBUG_printf("jump_if_or_pop(cond=%u, label=" UINT_FMT ")\n", cond, label);
    emit_native_jump_helper(emit, cond, label, false);
}

static void emit_native_unwind_jump(emit_t *emit, mp_uint_t label, mp_uint_t except_depth) {
    if (except_depth > 0) {
        exc_stack_entry_t *first_finally = NULL;
        exc_stack_entry_t *prev_finally = NULL;
        exc_stack_entry_t *e = &emit->exc_stack[emit->exc_stack_size - 1];
        for (; except_depth > 0; --except_depth, --e) {
            if (e->is_finally && e->is_active) {
                
                if (first_finally == NULL) {
                    first_finally = e;
                }
                if (prev_finally != NULL) {
                    
                    prev_finally->unwind_label = e->label;
                }
                prev_finally = e;
            }
        }
        if (prev_finally == NULL) {
            
            
            if (e < emit->exc_stack) {
                ASM_XOR_REG_REG(emit->as, REG_RET, REG_RET);
            } else {
                ASM_MOV_REG_PCREL(emit->as, REG_RET, e->label);
            }
            ASM_MOV_LOCAL_REG(emit->as, LOCAL_IDX_EXC_HANDLER_PC(emit), REG_RET);
        } else {
            
            
            prev_finally->unwind_label = UNWIND_LABEL_DO_FINAL_UNWIND;
            ASM_MOV_REG_PCREL(emit->as, REG_RET, label & ~MP_EMIT_BREAK_FROM_FOR);
            ASM_MOV_LOCAL_REG(emit->as, LOCAL_IDX_EXC_HANDLER_UNWIND(emit), REG_RET);
            
            ASM_MOV_REG_IMM(emit->as, REG_RET, (mp_uint_t)MP_OBJ_NULL);
            ASM_MOV_LOCAL_REG(emit->as, LOCAL_IDX_EXC_VAL(emit), REG_RET);
            
            label = first_finally->label;
        }
    }
    emit_native_jump(emit, label & ~MP_EMIT_BREAK_FROM_FOR);
}

static void emit_native_setup_with(emit_t *emit, mp_uint_t label) {
    
    

    
    vtype_kind_t vtype;
    emit_access_stack(emit, 1, &vtype, REG_ARG_1); 
    assert(vtype == VTYPE_PYOBJ);
    emit_get_stack_pointer_to_reg_for_push(emit, REG_ARG_3, 2); 
    emit_call_with_qstr_arg(emit, MP_F_LOAD_METHOD, MP_QSTR___exit__, REG_ARG_2);
    

    emit_pre_pop_reg(emit, &vtype, REG_ARG_3); 
    emit_pre_pop_reg(emit, &vtype, REG_ARG_2); 
    emit_pre_pop_reg(emit, &vtype, REG_ARG_1); 
    emit_post_push_reg(emit, vtype, REG_ARG_2); 
    emit_post_push_reg(emit, vtype, REG_ARG_3); 
    
    

    
    emit_get_stack_pointer_to_reg_for_push(emit, REG_ARG_3, 2); 
    emit_call_with_qstr_arg(emit, MP_F_LOAD_METHOD, MP_QSTR___enter__, REG_ARG_2); 
    

    
    emit_get_stack_pointer_to_reg_for_pop(emit, REG_ARG_3, 2); 
    emit_call_with_2_imm_args(emit, MP_F_CALL_METHOD_N_KW, 0, REG_ARG_1, 0, REG_ARG_2);
    emit_post_push_reg(emit, VTYPE_PYOBJ, REG_RET); 
    

    
    need_stack_settled(emit);
    emit_native_push_exc_stack(emit, label, true);

    emit_native_dup_top(emit);
    
}

static void emit_native_setup_block(emit_t *emit, mp_uint_t label, int kind) {
    if (kind == MP_EMIT_SETUP_BLOCK_WITH) {
        emit_native_setup_with(emit, label);
    } else {
        
        emit_native_pre(emit);
        need_stack_settled(emit);
        emit_native_push_exc_stack(emit, label, kind == MP_EMIT_SETUP_BLOCK_FINALLY);
        emit_post(emit);
    }
}

static void emit_native_with_cleanup(emit_t *emit, mp_uint_t label) {
    

    
    emit_native_pre(emit);
    emit_native_leave_exc_stack(emit, false);
    adjust_stack(emit, -1);
    

    
    emit_native_label_assign(emit, *emit->label_slot + 2);

    
    emit_post_push_imm(emit, VTYPE_PTR_NONE, 0);
    emit_post_push_imm(emit, VTYPE_PTR_NONE, 0);
    emit_post_push_imm(emit, VTYPE_PTR_NONE, 0);
    emit_get_stack_pointer_to_reg_for_pop(emit, REG_ARG_3, 5);
    emit_call_with_2_imm_args(emit, MP_F_CALL_METHOD_N_KW, 3, REG_ARG_1, 0, REG_ARG_2);

    
    emit_native_jump(emit, *emit->label_slot);

    
    
    mp_asm_base_label_assign(&emit->as->base, label);

    
    emit_native_leave_exc_stack(emit, true);

    
    emit_native_adjust_stack_size(emit, 2);
    

    ASM_MOV_REG_LOCAL(emit->as, REG_ARG_1, LOCAL_IDX_EXC_VAL(emit)); 

    
    ASM_JUMP_IF_REG_ZERO(emit->as, REG_ARG_1, *emit->label_slot + 2, false);

    ASM_LOAD_REG_REG_OFFSET(emit->as, REG_ARG_2, REG_ARG_1, 0); 
    emit_post_push_reg(emit, VTYPE_PYOBJ, REG_ARG_2); 
    emit_post_push_reg(emit, VTYPE_PYOBJ, REG_ARG_1); 
    emit_post_push_imm(emit, VTYPE_PTR_NONE, 0); 
    

    
    emit_get_stack_pointer_to_reg_for_pop(emit, REG_ARG_3, 5);
    emit_call_with_2_imm_args(emit, MP_F_CALL_METHOD_N_KW, 3, REG_ARG_1, 0, REG_ARG_2);
    

    
    if (REG_ARG_1 != REG_RET) {
        ASM_MOV_REG_REG(emit->as, REG_ARG_1, REG_RET);
    }
    emit_call(emit, MP_F_OBJ_IS_TRUE);
    ASM_JUMP_IF_REG_ZERO(emit->as, REG_RET, *emit->label_slot + 1, true);

    
    emit_native_label_assign(emit, *emit->label_slot);
    ASM_MOV_REG_IMM(emit->as, REG_TEMP0, (mp_uint_t)MP_OBJ_NULL);
    ASM_MOV_LOCAL_REG(emit->as, LOCAL_IDX_EXC_VAL(emit), REG_TEMP0);

    
    emit_native_label_assign(emit, *emit->label_slot + 1);

    
}

static void emit_native_end_finally(emit_t *emit) {
    
    
    
    
    
    emit_native_pre(emit);
    ASM_MOV_REG_LOCAL(emit->as, REG_ARG_1, LOCAL_IDX_EXC_VAL(emit));
    emit_call(emit, MP_F_NATIVE_RAISE);

    
    exc_stack_entry_t *e = emit_native_pop_exc_stack(emit);
    if (e->unwind_label != UNWIND_LABEL_UNUSED) {
        ASM_MOV_REG_LOCAL(emit->as, REG_RET, LOCAL_IDX_EXC_HANDLER_UNWIND(emit));
        ASM_JUMP_IF_REG_ZERO(emit->as, REG_RET, *emit->label_slot, false);
        if (e->unwind_label == UNWIND_LABEL_DO_FINAL_UNWIND) {
            ASM_JUMP_REG(emit->as, REG_RET);
        } else {
            emit_native_jump(emit, e->unwind_label);
        }
        emit_native_label_assign(emit, *emit->label_slot);
    }

    emit_post(emit);
}

static void emit_native_get_iter(emit_t *emit, bool use_stack) {
    
    

    vtype_kind_t vtype;
    emit_pre_pop_reg(emit, &vtype, REG_ARG_1);
    assert(vtype == VTYPE_PYOBJ);
    if (use_stack) {
        emit_get_stack_pointer_to_reg_for_push(emit, REG_ARG_2, MP_OBJ_ITER_BUF_NSLOTS);
        emit_call(emit, MP_F_NATIVE_GETITER);
    } else {
        
        ASM_MOV_REG_IMM(emit->as, REG_ARG_2, 0);
        emit_call(emit, MP_F_NATIVE_GETITER);
        emit_post_push_reg(emit, VTYPE_PYOBJ, REG_RET);
    }
}

static void emit_native_for_iter(emit_t *emit, mp_uint_t label) {
    emit_native_pre(emit);
    emit_get_stack_pointer_to_reg_for_pop(emit, REG_ARG_1, MP_OBJ_ITER_BUF_NSLOTS);
    adjust_stack(emit, MP_OBJ_ITER_BUF_NSLOTS);
    emit_call(emit, MP_F_NATIVE_ITERNEXT);
    #if MICROPY_DEBUG_MP_OBJ_SENTINELS
    ASM_MOV_REG_IMM(emit->as, REG_TEMP1, (mp_uint_t)MP_OBJ_STOP_ITERATION);
    ASM_JUMP_IF_REG_EQ(emit->as, REG_RET, REG_TEMP1, label);
    #else
    MP_STATIC_ASSERT(MP_OBJ_STOP_ITERATION == 0);
    ASM_JUMP_IF_REG_ZERO(emit->as, REG_RET, label, false);
    #endif
    emit_post_push_reg(emit, VTYPE_PYOBJ, REG_RET);
}

static void emit_native_for_iter_end(emit_t *emit) {
    
    emit_native_pre(emit);
    adjust_stack(emit, -MP_OBJ_ITER_BUF_NSLOTS);
    emit_post(emit);
}

static void emit_native_pop_except_jump(emit_t *emit, mp_uint_t label, bool within_exc_handler) {
    if (within_exc_handler) {
        
        ASM_MOV_REG_IMM(emit->as, REG_TEMP0, (mp_uint_t)MP_OBJ_NULL);
        ASM_MOV_LOCAL_REG(emit->as, LOCAL_IDX_EXC_VAL(emit), REG_TEMP0);
    } else {
        emit_native_leave_exc_stack(emit, false);
    }
    emit_native_jump(emit, label);
}

static void emit_native_unary_op(emit_t *emit, mp_unary_op_t op) {
    vtype_kind_t vtype = peek_vtype(emit, 0);
    if (vtype == VTYPE_INT || vtype == VTYPE_UINT) {
        if (op == MP_UNARY_OP_POSITIVE) {
            
        } else if (op == MP_UNARY_OP_NEGATIVE) {
            int reg = REG_RET;
            emit_pre_pop_reg_flexible(emit, &vtype, &reg, reg, reg);
            ASM_NEG_REG(emit->as, reg);
            emit_post_push_reg(emit, vtype, reg);
        } else if (op == MP_UNARY_OP_INVERT) {
            #ifdef ASM_NOT_REG
            int reg = REG_RET;
            emit_pre_pop_reg_flexible(emit, &vtype, &reg, reg, reg);
            ASM_NOT_REG(emit->as, reg);
            #else
            int reg = REG_RET;
            emit_pre_pop_reg_flexible(emit, &vtype, &reg, REG_ARG_1, reg);
            ASM_MOV_REG_IMM(emit->as, REG_ARG_1, -1);
            ASM_XOR_REG_REG(emit->as, reg, REG_ARG_1);
            #endif
            emit_post_push_reg(emit, vtype, reg);
        } else {
            EMIT_NATIVE_VIPER_TYPE_ERROR(emit,
                MP_ERROR_TEXT("'not' not implemented"), mp_binary_op_method_name[op]);
        }
    } else if (vtype == VTYPE_PYOBJ) {
        emit_pre_pop_reg(emit, &vtype, REG_ARG_2);
        emit_call_with_imm_arg(emit, MP_F_UNARY_OP, op, REG_ARG_1);
        emit_post_push_reg(emit, VTYPE_PYOBJ, REG_RET);
    } else {
        EMIT_NATIVE_VIPER_TYPE_ERROR(emit,
            MP_ERROR_TEXT("can't do unary op of '%q'"), vtype_to_qstr(vtype));
    }
}

static void emit_native_binary_op(emit_t *emit, mp_binary_op_t op) {
    DEBUG_printf("binary_op(" UINT_FMT ")\n", op);
    vtype_kind_t vtype_lhs = peek_vtype(emit, 1);
    vtype_kind_t vtype_rhs = peek_vtype(emit, 0);
    if ((vtype_lhs == VTYPE_INT || vtype_lhs == VTYPE_UINT)
        && (vtype_rhs == VTYPE_INT || vtype_rhs == VTYPE_UINT)) {
        
        if (MP_BINARY_OP_INPLACE_OR <= op && op <= MP_BINARY_OP_INPLACE_POWER) {
            op += MP_BINARY_OP_OR - MP_BINARY_OP_INPLACE_OR;
        }

        #if N_X64 || N_X86
        
        if (op == MP_BINARY_OP_LSHIFT || op == MP_BINARY_OP_RSHIFT) {
            #if N_X64
            emit_pre_pop_reg_reg(emit, &vtype_rhs, ASM_X64_REG_RCX, &vtype_lhs, REG_RET);
            #else
            emit_pre_pop_reg_reg(emit, &vtype_rhs, ASM_X86_REG_ECX, &vtype_lhs, REG_RET);
            #endif
            if (op == MP_BINARY_OP_LSHIFT) {
                ASM_LSL_REG(emit->as, REG_RET);
            } else {
                if (vtype_lhs == VTYPE_UINT) {
                    ASM_LSR_REG(emit->as, REG_RET);
                } else {
                    ASM_ASR_REG(emit->as, REG_RET);
                }
            }
            emit_post_push_reg(emit, vtype_lhs, REG_RET);
            return;
        }
        #endif

        
        if (op == MP_BINARY_OP_FLOOR_DIVIDE || op == MP_BINARY_OP_MODULO) {
            emit_pre_pop_reg_reg(emit, &vtype_rhs, REG_ARG_2, &vtype_lhs, REG_ARG_1);
            if (vtype_lhs != VTYPE_INT) {
                EMIT_NATIVE_VIPER_TYPE_ERROR(emit,
                    MP_ERROR_TEXT("div/mod not implemented for uint"), mp_binary_op_method_name[op]);
            }
            if (op == MP_BINARY_OP_FLOOR_DIVIDE) {
                emit_call(emit, MP_F_SMALL_INT_FLOOR_DIVIDE);
            } else {
                emit_call(emit, MP_F_SMALL_INT_MODULO);
            }
            emit_post_push_reg(emit, VTYPE_INT, REG_RET);
            return;
        }

        int reg_rhs = REG_ARG_3;
        emit_pre_pop_reg_flexible(emit, &vtype_rhs, &reg_rhs, REG_RET, REG_ARG_2);
        emit_pre_pop_reg(emit, &vtype_lhs, REG_ARG_2);

        #if !(N_X64 || N_X86)
        if (op == MP_BINARY_OP_LSHIFT || op == MP_BINARY_OP_RSHIFT) {
            if (op == MP_BINARY_OP_LSHIFT) {
                ASM_LSL_REG_REG(emit->as, REG_ARG_2, reg_rhs);
            } else {
                if (vtype_lhs == VTYPE_UINT) {
                    ASM_LSR_REG_REG(emit->as, REG_ARG_2, reg_rhs);
                } else {
                    ASM_ASR_REG_REG(emit->as, REG_ARG_2, reg_rhs);
                }
            }
            emit_post_push_reg(emit, vtype_lhs, REG_ARG_2);
            return;
        }
        #endif

        if (op == MP_BINARY_OP_OR) {
            ASM_OR_REG_REG(emit->as, REG_ARG_2, reg_rhs);
            emit_post_push_reg(emit, vtype_lhs, REG_ARG_2);
        } else if (op == MP_BINARY_OP_XOR) {
            ASM_XOR_REG_REG(emit->as, REG_ARG_2, reg_rhs);
            emit_post_push_reg(emit, vtype_lhs, REG_ARG_2);
        } else if (op == MP_BINARY_OP_AND) {
            ASM_AND_REG_REG(emit->as, REG_ARG_2, reg_rhs);
            emit_post_push_reg(emit, vtype_lhs, REG_ARG_2);
        } else if (op == MP_BINARY_OP_ADD) {
            ASM_ADD_REG_REG(emit->as, REG_ARG_2, reg_rhs);
            emit_post_push_reg(emit, vtype_lhs, REG_ARG_2);
        } else if (op == MP_BINARY_OP_SUBTRACT) {
            ASM_SUB_REG_REG(emit->as, REG_ARG_2, reg_rhs);
            emit_post_push_reg(emit, vtype_lhs, REG_ARG_2);
        } else if (op == MP_BINARY_OP_MULTIPLY) {
            ASM_MUL_REG_REG(emit->as, REG_ARG_2, reg_rhs);
            emit_post_push_reg(emit, vtype_lhs, REG_ARG_2);
        } else if (op == MP_BINARY_OP_LESS
                   || op == MP_BINARY_OP_MORE
                   || op == MP_BINARY_OP_EQUAL
                   || op == MP_BINARY_OP_LESS_EQUAL
                   || op == MP_BINARY_OP_MORE_EQUAL
                   || op == MP_BINARY_OP_NOT_EQUAL) {
            

            if (vtype_lhs != vtype_rhs) {
                EMIT_NATIVE_VIPER_TYPE_ERROR(emit, MP_ERROR_TEXT("comparison of int and uint"));
            }

            size_t op_idx = op - MP_BINARY_OP_LESS + (vtype_lhs == VTYPE_UINT ? 0 : 6);

            need_reg_single(emit, REG_RET, 0);
            #if N_X64
            asm_x64_xor_r64_r64(emit->as, REG_RET, REG_RET);
            asm_x64_cmp_r64_with_r64(emit->as, reg_rhs, REG_ARG_2);
            static byte ops[6 + 6] = {
                
                ASM_X64_CC_JB,
                ASM_X64_CC_JA,
                ASM_X64_CC_JE,
                ASM_X64_CC_JBE,
                ASM_X64_CC_JAE,
                ASM_X64_CC_JNE,
                
                ASM_X64_CC_JL,
                ASM_X64_CC_JG,
                ASM_X64_CC_JE,
                ASM_X64_CC_JLE,
                ASM_X64_CC_JGE,
                ASM_X64_CC_JNE,
            };
            asm_x64_setcc_r8(emit->as, ops[op_idx], REG_RET);
            #elif N_X86
            asm_x86_xor_r32_r32(emit->as, REG_RET, REG_RET);
            asm_x86_cmp_r32_with_r32(emit->as, reg_rhs, REG_ARG_2);
            static byte ops[6 + 6] = {
                
                ASM_X86_CC_JB,
                ASM_X86_CC_JA,
                ASM_X86_CC_JE,
                ASM_X86_CC_JBE,
                ASM_X86_CC_JAE,
                ASM_X86_CC_JNE,
                
                ASM_X86_CC_JL,
                ASM_X86_CC_JG,
                ASM_X86_CC_JE,
                ASM_X86_CC_JLE,
                ASM_X86_CC_JGE,
                ASM_X86_CC_JNE,
            };
            asm_x86_setcc_r8(emit->as, ops[op_idx], REG_RET);
            #elif N_THUMB
            asm_thumb_cmp_rlo_rlo(emit->as, REG_ARG_2, reg_rhs);
            if (asm_thumb_allow_armv7m(emit->as)) {
                static uint16_t ops[6 + 6] = {
                    
                    ASM_THUMB_OP_ITE_CC,
                    ASM_THUMB_OP_ITE_HI,
                    ASM_THUMB_OP_ITE_EQ,
                    ASM_THUMB_OP_ITE_LS,
                    ASM_THUMB_OP_ITE_CS,
                    ASM_THUMB_OP_ITE_NE,
                    
                    ASM_THUMB_OP_ITE_LT,
                    ASM_THUMB_OP_ITE_GT,
                    ASM_THUMB_OP_ITE_EQ,
                    ASM_THUMB_OP_ITE_LE,
                    ASM_THUMB_OP_ITE_GE,
                    ASM_THUMB_OP_ITE_NE,
                };
                asm_thumb_op16(emit->as, ops[op_idx]);
                asm_thumb_mov_rlo_i8(emit->as, REG_RET, 1);
                asm_thumb_mov_rlo_i8(emit->as, REG_RET, 0);
            } else {
                static uint16_t ops[6 + 6] = {
                    
                    ASM_THUMB_CC_CC,
                    ASM_THUMB_CC_HI,
                    ASM_THUMB_CC_EQ,
                    ASM_THUMB_CC_LS,
                    ASM_THUMB_CC_CS,
                    ASM_THUMB_CC_NE,
                    
                    ASM_THUMB_CC_LT,
                    ASM_THUMB_CC_GT,
                    ASM_THUMB_CC_EQ,
                    ASM_THUMB_CC_LE,
                    ASM_THUMB_CC_GE,
                    ASM_THUMB_CC_NE,
                };
                asm_thumb_bcc_rel9(emit->as, ops[op_idx], 6);
                asm_thumb_mov_rlo_i8(emit->as, REG_RET, 0);
                asm_thumb_b_rel12(emit->as, 4);
                asm_thumb_mov_rlo_i8(emit->as, REG_RET, 1);
            }
            #elif N_ARM
            asm_arm_cmp_reg_reg(emit->as, REG_ARG_2, reg_rhs);
            static uint ccs[6 + 6] = {
                
                ASM_ARM_CC_CC,
                ASM_ARM_CC_HI,
                ASM_ARM_CC_EQ,
                ASM_ARM_CC_LS,
                ASM_ARM_CC_CS,
                ASM_ARM_CC_NE,
                
                ASM_ARM_CC_LT,
                ASM_ARM_CC_GT,
                ASM_ARM_CC_EQ,
                ASM_ARM_CC_LE,
                ASM_ARM_CC_GE,
                ASM_ARM_CC_NE,
            };
            asm_arm_setcc_reg(emit->as, REG_RET, ccs[op_idx]);
            #elif N_XTENSA || N_XTENSAWIN
            static uint8_t ccs[6 + 6] = {
                
                ASM_XTENSA_CC_LTU,
                0x80 | ASM_XTENSA_CC_LTU, 
                ASM_XTENSA_CC_EQ,
                0x80 | ASM_XTENSA_CC_GEU, 
                ASM_XTENSA_CC_GEU,
                ASM_XTENSA_CC_NE,
                
                ASM_XTENSA_CC_LT,
                0x80 | ASM_XTENSA_CC_LT, 
                ASM_XTENSA_CC_EQ,
                0x80 | ASM_XTENSA_CC_GE, 
                ASM_XTENSA_CC_GE,
                ASM_XTENSA_CC_NE,
            };
            uint8_t cc = ccs[op_idx];
            if ((cc & 0x80) == 0) {
                asm_xtensa_setcc_reg_reg_reg(emit->as, cc, REG_RET, REG_ARG_2, reg_rhs);
            } else {
                asm_xtensa_setcc_reg_reg_reg(emit->as, cc & ~0x80, REG_RET, reg_rhs, REG_ARG_2);
            }
            #else
            #error not implemented
            #endif
            emit_post_push_reg(emit, VTYPE_BOOL, REG_RET);
        } else {
            
            adjust_stack(emit, 1);
            EMIT_NATIVE_VIPER_TYPE_ERROR(emit,
                MP_ERROR_TEXT("binary op %q not implemented"), mp_binary_op_method_name[op]);
        }
    } else if (vtype_lhs == VTYPE_PYOBJ && vtype_rhs == VTYPE_PYOBJ) {
        emit_pre_pop_reg_reg(emit, &vtype_rhs, REG_ARG_3, &vtype_lhs, REG_ARG_2);
        bool invert = false;
        if (op == MP_BINARY_OP_NOT_IN) {
            invert = true;
            op = MP_BINARY_OP_IN;
        } else if (op == MP_BINARY_OP_IS_NOT) {
            invert = true;
            op = MP_BINARY_OP_IS;
        }
        emit_call_with_imm_arg(emit, MP_F_BINARY_OP, op, REG_ARG_1);
        if (invert) {
            ASM_MOV_REG_REG(emit->as, REG_ARG_2, REG_RET);
            emit_call_with_imm_arg(emit, MP_F_UNARY_OP, MP_UNARY_OP_NOT, REG_ARG_1);
        }
        emit_post_push_reg(emit, VTYPE_PYOBJ, REG_RET);
    } else {
        adjust_stack(emit, -1);
        EMIT_NATIVE_VIPER_TYPE_ERROR(emit,
            MP_ERROR_TEXT("can't do binary op between '%q' and '%q'"),
            vtype_to_qstr(vtype_lhs), vtype_to_qstr(vtype_rhs));
    }
}

#if MICROPY_PY_BUILTINS_SLICE
static void emit_native_build_slice(emit_t *emit, mp_uint_t n_args);
#endif

static void emit_native_build(emit_t *emit, mp_uint_t n_args, int kind) {
    
    
    MP_STATIC_ASSERT(MP_F_BUILD_TUPLE + MP_EMIT_BUILD_TUPLE == MP_F_BUILD_TUPLE);
    MP_STATIC_ASSERT(MP_F_BUILD_TUPLE + MP_EMIT_BUILD_LIST == MP_F_BUILD_LIST);
    MP_STATIC_ASSERT(MP_F_BUILD_TUPLE + MP_EMIT_BUILD_MAP == MP_F_BUILD_MAP);
    MP_STATIC_ASSERT(MP_F_BUILD_TUPLE + MP_EMIT_BUILD_SET == MP_F_BUILD_SET);
    #if MICROPY_PY_BUILTINS_SLICE
    if (kind == MP_EMIT_BUILD_SLICE) {
        emit_native_build_slice(emit, n_args);
        return;
    }
    #endif
    emit_native_pre(emit);
    if (kind == MP_EMIT_BUILD_TUPLE || kind == MP_EMIT_BUILD_LIST || kind == MP_EMIT_BUILD_SET) {
        emit_get_stack_pointer_to_reg_for_pop(emit, REG_ARG_2, n_args); 
    }
    emit_call_with_imm_arg(emit, MP_F_BUILD_TUPLE + kind, n_args, REG_ARG_1);
    emit_post_push_reg(emit, VTYPE_PYOBJ, REG_RET); 
}

static void emit_native_store_map(emit_t *emit) {
    vtype_kind_t vtype_key, vtype_value, vtype_map;
    emit_pre_pop_reg_reg_reg(emit, &vtype_key, REG_ARG_2, &vtype_value, REG_ARG_3, &vtype_map, REG_ARG_1); 
    assert(vtype_key == VTYPE_PYOBJ);
    assert(vtype_value == VTYPE_PYOBJ);
    assert(vtype_map == VTYPE_PYOBJ);
    emit_call(emit, MP_F_STORE_MAP);
    emit_post_push_reg(emit, VTYPE_PYOBJ, REG_RET); 
}

#if MICROPY_PY_BUILTINS_SLICE
static void emit_native_build_slice(emit_t *emit, mp_uint_t n_args) {
    DEBUG_printf("build_slice %d\n", n_args);
    if (n_args == 2) {
        vtype_kind_t vtype_start, vtype_stop;
        emit_pre_pop_reg_reg(emit, &vtype_stop, REG_ARG_2, &vtype_start, REG_ARG_1); 
        assert(vtype_start == VTYPE_PYOBJ);
        assert(vtype_stop == VTYPE_PYOBJ);
        emit_native_mov_reg_const(emit, REG_ARG_3, MP_F_CONST_NONE_OBJ); 
    } else {
        assert(n_args == 3);
        vtype_kind_t vtype_start, vtype_stop, vtype_step;
        emit_pre_pop_reg_reg_reg(emit, &vtype_step, REG_ARG_3, &vtype_stop, REG_ARG_2, &vtype_start, REG_ARG_1); 
        assert(vtype_start == VTYPE_PYOBJ);
        assert(vtype_stop == VTYPE_PYOBJ);
        assert(vtype_step == VTYPE_PYOBJ);
    }
    emit_call(emit, MP_F_NEW_SLICE);
    emit_post_push_reg(emit, VTYPE_PYOBJ, REG_RET);
}
#endif

static void emit_native_store_comp(emit_t *emit, scope_kind_t kind, mp_uint_t collection_index) {
    mp_fun_kind_t f;
    if (kind == SCOPE_LIST_COMP) {
        vtype_kind_t vtype_item;
        emit_pre_pop_reg(emit, &vtype_item, REG_ARG_2);
        assert(vtype_item == VTYPE_PYOBJ);
        f = MP_F_LIST_APPEND;
    #if MICROPY_PY_BUILTINS_SET
    } else if (kind == SCOPE_SET_COMP) {
        vtype_kind_t vtype_item;
        emit_pre_pop_reg(emit, &vtype_item, REG_ARG_2);
        assert(vtype_item == VTYPE_PYOBJ);
        f = MP_F_STORE_SET;
    #endif
    } else {
        
        vtype_kind_t vtype_key, vtype_value;
        emit_pre_pop_reg_reg(emit, &vtype_key, REG_ARG_2, &vtype_value, REG_ARG_3);
        assert(vtype_key == VTYPE_PYOBJ);
        assert(vtype_value == VTYPE_PYOBJ);
        f = MP_F_STORE_MAP;
    }
    vtype_kind_t vtype_collection;
    emit_access_stack(emit, collection_index, &vtype_collection, REG_ARG_1);
    assert(vtype_collection == VTYPE_PYOBJ);
    emit_call(emit, f);
    emit_post(emit);
}

static void emit_native_unpack_sequence(emit_t *emit, mp_uint_t n_args) {
    DEBUG_printf("unpack_sequence %d\n", n_args);
    vtype_kind_t vtype_base;
    emit_pre_pop_reg(emit, &vtype_base, REG_ARG_1); 
    assert(vtype_base == VTYPE_PYOBJ);
    emit_get_stack_pointer_to_reg_for_push(emit, REG_ARG_3, n_args); 
    emit_call_with_imm_arg(emit, MP_F_UNPACK_SEQUENCE, n_args, REG_ARG_2); 
}

static void emit_native_unpack_ex(emit_t *emit, mp_uint_t n_left, mp_uint_t n_right) {
    DEBUG_printf("unpack_ex %d %d\n", n_left, n_right);
    vtype_kind_t vtype_base;
    emit_pre_pop_reg(emit, &vtype_base, REG_ARG_1); 
    assert(vtype_base == VTYPE_PYOBJ);
    emit_get_stack_pointer_to_reg_for_push(emit, REG_ARG_3, n_left + n_right + 1); 
    emit_call_with_imm_arg(emit, MP_F_UNPACK_EX, n_left | (n_right << 8), REG_ARG_2); 
}

static void emit_native_make_function(emit_t *emit, scope_t *scope, mp_uint_t n_pos_defaults, mp_uint_t n_kw_defaults) {
    
    emit_native_pre(emit);
    emit_native_mov_reg_state(emit, REG_ARG_2, LOCAL_IDX_FUN_OBJ(emit));
    ASM_LOAD_REG_REG_OFFSET(emit->as, REG_ARG_2, REG_ARG_2, OFFSETOF_OBJ_FUN_BC_CONTEXT);
    if (n_pos_defaults == 0 && n_kw_defaults == 0) {
        need_reg_all(emit);
        ASM_MOV_REG_IMM(emit->as, REG_ARG_3, 0);
    } else {
        emit_get_stack_pointer_to_reg_for_pop(emit, REG_ARG_3, 2);
        need_reg_all(emit);
    }
    emit_load_reg_with_child(emit, REG_ARG_1, scope->raw_code);
    ASM_CALL_IND(emit->as, MP_F_MAKE_FUNCTION_FROM_PROTO_FUN);
    emit_post_push_reg(emit, VTYPE_PYOBJ, REG_RET);
}

static void emit_native_make_closure(emit_t *emit, scope_t *scope, mp_uint_t n_closed_over, mp_uint_t n_pos_defaults, mp_uint_t n_kw_defaults) {
    
    emit_native_pre(emit);
    emit_native_mov_reg_state(emit, REG_ARG_2, LOCAL_IDX_FUN_OBJ(emit));
    ASM_LOAD_REG_REG_OFFSET(emit->as, REG_ARG_2, REG_ARG_2, OFFSETOF_OBJ_FUN_BC_CONTEXT);
    if (n_pos_defaults == 0 && n_kw_defaults == 0) {
        need_reg_all(emit);
        ASM_MOV_REG_IMM(emit->as, REG_ARG_3, 0);
    } else {
        emit_get_stack_pointer_to_reg_for_pop(emit, REG_ARG_3, 2 + n_closed_over);
        adjust_stack(emit, 2 + n_closed_over);
        need_reg_all(emit);
    }
    emit_load_reg_with_child(emit, REG_ARG_1, scope->raw_code);
    ASM_CALL_IND(emit->as, MP_F_MAKE_FUNCTION_FROM_PROTO_FUN);

    
    #if REG_ARG_1 != REG_RET
    ASM_MOV_REG_REG(emit->as, REG_ARG_1, REG_RET);
    #endif
    ASM_MOV_REG_IMM(emit->as, REG_ARG_2, n_closed_over);
    emit_get_stack_pointer_to_reg_for_pop(emit, REG_ARG_3, n_closed_over);
    if (n_pos_defaults != 0 || n_kw_defaults != 0) {
        adjust_stack(emit, -2);
    }
    ASM_CALL_IND(emit->as, MP_F_NEW_CLOSURE);
    emit_post_push_reg(emit, VTYPE_PYOBJ, REG_RET);
}

static void emit_native_call_function(emit_t *emit, mp_uint_t n_positional, mp_uint_t n_keyword, mp_uint_t star_flags) {
    DEBUG_printf("call_function(n_pos=" UINT_FMT ", n_kw=" UINT_FMT ", star_flags=" UINT_FMT ")\n", n_positional, n_keyword, star_flags);

    
    

    emit_native_pre(emit);
    vtype_kind_t vtype_fun = peek_vtype(emit, n_positional + 2 * n_keyword);
    if (vtype_fun == VTYPE_BUILTIN_CAST) {
        
        assert(n_positional == 1 && n_keyword == 0);
        assert(!star_flags);
        DEBUG_printf("  cast to %d\n", vtype_fun);
        vtype_kind_t vtype_cast = peek_stack(emit, 1)->data.u_imm;
        switch (peek_vtype(emit, 0)) {
            case VTYPE_PYOBJ: {
                vtype_kind_t vtype;
                emit_pre_pop_reg(emit, &vtype, REG_ARG_1);
                emit_pre_pop_discard(emit);
                emit_call_with_imm_arg(emit, MP_F_CONVERT_OBJ_TO_NATIVE, vtype_cast, REG_ARG_2); 
                emit_post_push_reg(emit, vtype_cast, REG_RET);
                break;
            }
            case VTYPE_BOOL:
            case VTYPE_INT:
            case VTYPE_UINT:
            case VTYPE_PTR:
            case VTYPE_PTR8:
            case VTYPE_PTR16:
            case VTYPE_PTR32:
            case VTYPE_PTR_NONE:
                emit_fold_stack_top(emit, REG_ARG_1);
                emit_post_top_set_vtype(emit, vtype_cast);
                break;
            default:
                
                mp_raise_NotImplementedError(MP_ERROR_TEXT("casting"));
        }
    } else {
        assert(vtype_fun == VTYPE_PYOBJ);
        if (star_flags) {
            emit_get_stack_pointer_to_reg_for_pop(emit, REG_ARG_3, n_positional + 2 * n_keyword + 2); 
            emit_call_with_2_imm_args(emit, MP_F_CALL_METHOD_N_KW_VAR, 0, REG_ARG_1, n_positional | (n_keyword << 8), REG_ARG_2);
            emit_post_push_reg(emit, VTYPE_PYOBJ, REG_RET);
        } else {
            if (n_positional != 0 || n_keyword != 0) {
                emit_get_stack_pointer_to_reg_for_pop(emit, REG_ARG_3, n_positional + 2 * n_keyword); 
            }
            emit_pre_pop_reg(emit, &vtype_fun, REG_ARG_1); 
            emit_call_with_imm_arg(emit, MP_F_NATIVE_CALL_FUNCTION_N_KW, n_positional | (n_keyword << 8), REG_ARG_2);
            emit_post_push_reg(emit, VTYPE_PYOBJ, REG_RET);
        }
    }
}

static void emit_native_call_method(emit_t *emit, mp_uint_t n_positional, mp_uint_t n_keyword, mp_uint_t star_flags) {
    if (star_flags) {
        emit_get_stack_pointer_to_reg_for_pop(emit, REG_ARG_3, n_positional + 2 * n_keyword + 3); 
        emit_call_with_2_imm_args(emit, MP_F_CALL_METHOD_N_KW_VAR, 1, REG_ARG_1, n_positional | (n_keyword << 8), REG_ARG_2);
        emit_post_push_reg(emit, VTYPE_PYOBJ, REG_RET);
    } else {
        emit_native_pre(emit);
        emit_get_stack_pointer_to_reg_for_pop(emit, REG_ARG_3, 2 + n_positional + 2 * n_keyword); 
        emit_call_with_2_imm_args(emit, MP_F_CALL_METHOD_N_KW, n_positional, REG_ARG_1, n_keyword, REG_ARG_2);
        emit_post_push_reg(emit, VTYPE_PYOBJ, REG_RET);
    }
}

static void emit_native_return_value(emit_t *emit) {
    DEBUG_printf("return_value\n");

    if (emit->scope->scope_flags & MP_SCOPE_FLAG_GENERATOR) {
        
        emit_get_stack_pointer_to_reg_for_pop(emit, REG_TEMP0, 1);
        emit_native_mov_state_reg(emit, OFFSETOF_CODE_STATE_SP, REG_TEMP0);

        
        ASM_MOV_REG_IMM(emit->as, REG_TEMP0, MP_VM_RETURN_NORMAL);
        ASM_MOV_LOCAL_REG(emit->as, LOCAL_IDX_RET_VAL(emit), REG_TEMP0);

        
        emit_native_unwind_jump(emit, emit->exit_label, emit->exc_stack_size);
        return;
    }

    if (emit->do_viper_types) {
        vtype_kind_t return_vtype = emit->scope->scope_flags >> MP_SCOPE_FLAG_VIPERRET_POS;
        if (peek_vtype(emit, 0) == VTYPE_PTR_NONE) {
            emit_pre_pop_discard(emit);
            if (return_vtype == VTYPE_PYOBJ) {
                emit_native_mov_reg_const(emit, REG_PARENT_RET, MP_F_CONST_NONE_OBJ);
            } else {
                ASM_MOV_REG_IMM(emit->as, REG_ARG_1, 0);
            }
        } else {
            vtype_kind_t vtype;
            emit_pre_pop_reg(emit, &vtype, return_vtype == VTYPE_PYOBJ ? REG_PARENT_RET : REG_ARG_1);
            if (vtype != return_vtype) {
                EMIT_NATIVE_VIPER_TYPE_ERROR(emit,
                    MP_ERROR_TEXT("return expected '%q' but got '%q'"),
                    vtype_to_qstr(return_vtype), vtype_to_qstr(vtype));
            }
        }
        if (return_vtype != VTYPE_PYOBJ) {
            emit_call_with_imm_arg(emit, MP_F_CONVERT_NATIVE_TO_OBJ, return_vtype, REG_ARG_2);
            #if REG_RET != REG_PARENT_RET
            ASM_MOV_REG_REG(emit->as, REG_PARENT_RET, REG_RET);
            #endif
        }
    } else {
        vtype_kind_t vtype;
        emit_pre_pop_reg(emit, &vtype, REG_PARENT_RET);
        assert(vtype == VTYPE_PYOBJ);
    }
    if (NEED_GLOBAL_EXC_HANDLER(emit)) {
        
        ASM_MOV_LOCAL_REG(emit->as, LOCAL_IDX_RET_VAL(emit), REG_PARENT_RET);
    }
    emit_native_unwind_jump(emit, emit->exit_label, emit->exc_stack_size);
}

static void emit_native_raise_varargs(emit_t *emit, mp_uint_t n_args) {
    (void)n_args;
    assert(n_args == 1);
    vtype_kind_t vtype_exc;
    emit_pre_pop_reg(emit, &vtype_exc, REG_ARG_1); 
    if (vtype_exc != VTYPE_PYOBJ) {
        EMIT_NATIVE_VIPER_TYPE_ERROR(emit, MP_ERROR_TEXT("must raise an object"));
    }
    
    emit_call(emit, MP_F_NATIVE_RAISE);
    mp_asm_base_suppress_code(&emit->as->base);
}

static void emit_native_yield(emit_t *emit, int kind) {
    

    if (emit->do_viper_types) {
        mp_raise_NotImplementedError(MP_ERROR_TEXT("native yield"));
    }
    emit->scope->scope_flags |= MP_SCOPE_FLAG_GENERATOR;

    need_stack_settled(emit);

    if (kind == MP_EMIT_YIELD_FROM) {

        
        
        

        
        emit_native_jump(emit, *emit->label_slot + 2);

        
        emit_native_label_assign(emit, *emit->label_slot + 1);
    }

    
    emit_get_stack_pointer_to_reg_for_pop(emit, REG_TEMP0, 1);
    emit_native_mov_state_reg(emit, OFFSETOF_CODE_STATE_SP, REG_TEMP0);

    
    ASM_MOV_REG_IMM(emit->as, REG_TEMP0, MP_VM_RETURN_YIELD);
    ASM_MOV_LOCAL_REG(emit->as, LOCAL_IDX_RET_VAL(emit), REG_TEMP0);

    
    ASM_MOV_REG_PCREL(emit->as, REG_TEMP0, *emit->label_slot);
    emit_native_mov_state_reg(emit, LOCAL_IDX_GEN_PC(emit), REG_TEMP0);

    
    ASM_JUMP(emit->as, emit->exit_label);

    
    mp_asm_base_label_assign(&emit->as->base, *emit->label_slot);

    
    if (emit->exc_stack_size > 0) {
        
        exc_stack_entry_t *e = &emit->exc_stack[emit->exc_stack_size - 1];
        for (; e >= emit->exc_stack; --e) {
            if (e->is_active) {
                
                ASM_MOV_REG_PCREL(emit->as, REG_RET, e->label);
                ASM_MOV_LOCAL_REG(emit->as, LOCAL_IDX_EXC_HANDLER_PC(emit), REG_RET);
                break;
            }
        }
    }

    emit_native_adjust_stack_size(emit, 1); 

    if (kind == MP_EMIT_YIELD_VALUE) {
        
        ASM_MOV_REG_LOCAL(emit->as, REG_ARG_1, LOCAL_IDX_EXC_VAL(emit));
        emit_call(emit, MP_F_NATIVE_RAISE);
    } else {
        
        emit_native_label_assign(emit, *emit->label_slot + 2);

        
        vtype_kind_t vtype;
        emit_pre_pop_reg(emit, &vtype, REG_ARG_2); 
        emit_access_stack(emit, 1, &vtype, REG_ARG_1); 
        ASM_MOV_REG_LOCAL(emit->as, REG_ARG_3, LOCAL_IDX_EXC_VAL(emit)); 
        emit_post_push_reg(emit, VTYPE_PYOBJ, REG_ARG_3);
        emit_get_stack_pointer_to_reg_for_pop(emit, REG_ARG_3, 1); 
        emit_call(emit, MP_F_NATIVE_YIELD_FROM);

        
        ASM_JUMP_IF_REG_NONZERO(emit->as, REG_RET, *emit->label_slot + 1, true);

        
        emit_native_adjust_stack_size(emit, 1); 
        emit_fold_stack_top(emit, REG_ARG_1);
    }
}

static void emit_native_start_except_handler(emit_t *emit) {
    
    emit_native_leave_exc_stack(emit, true);

    
    ASM_MOV_REG_LOCAL(emit->as, REG_TEMP0, LOCAL_IDX_EXC_VAL(emit));
    emit_post_push_reg(emit, VTYPE_PYOBJ, REG_TEMP0);
}

static void emit_native_end_except_handler(emit_t *emit) {
    adjust_stack(emit, -1); 
}

const emit_method_table_t EXPORT_FUN(method_table) = {
    #if MICROPY_DYNAMIC_COMPILER
    EXPORT_FUN(new),
    EXPORT_FUN(free),
    #endif

    emit_native_start_pass,
    emit_native_end_pass,
    emit_native_adjust_stack_size,
    emit_native_set_source_line,

    {
        emit_native_load_local,
        emit_native_load_global,
    },
    {
        emit_native_store_local,
        emit_native_store_global,
    },
    {
        emit_native_delete_local,
        emit_native_delete_global,
    },

    emit_native_label_assign,
    emit_native_import,
    emit_native_load_const_tok,
    emit_native_load_const_small_int,
    emit_native_load_const_str,
    emit_native_load_const_obj,
    emit_native_load_null,
    emit_native_load_method,
    emit_native_load_build_class,
    emit_native_subscr,
    emit_native_attr,
    emit_native_dup_top,
    emit_native_dup_top_two,
    emit_native_pop_top,
    emit_native_rot_two,
    emit_native_rot_three,
    emit_native_jump,
    emit_native_pop_jump_if,
    emit_native_jump_if_or_pop,
    emit_native_unwind_jump,
    emit_native_setup_block,
    emit_native_with_cleanup,
    emit_native_end_finally,
    emit_native_get_iter,
    emit_native_for_iter,
    emit_native_for_iter_end,
    emit_native_pop_except_jump,
    emit_native_unary_op,
    emit_native_binary_op,
    emit_native_build,
    emit_native_store_map,
    emit_native_store_comp,
    emit_native_unpack_sequence,
    emit_native_unpack_ex,
    emit_native_make_function,
    emit_native_make_closure,
    emit_native_call_function,
    emit_native_call_method,
    emit_native_return_value,
    emit_native_raise_varargs,
    emit_native_yield,

    emit_native_start_except_handler,
    emit_native_end_except_handler,
};

#endif
