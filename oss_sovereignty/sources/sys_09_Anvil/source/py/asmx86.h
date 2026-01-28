
#ifndef MICROPY_INCLUDED_PY_ASMX86_H
#define MICROPY_INCLUDED_PY_ASMX86_H

#include "py/mpconfig.h"
#include "py/misc.h"
#include "py/asmbase.h"














#define ASM_X86_REG_EAX (0)
#define ASM_X86_REG_ECX (1)
#define ASM_X86_REG_EDX (2)
#define ASM_X86_REG_EBX (3)
#define ASM_X86_REG_ESP (4)
#define ASM_X86_REG_EBP (5)
#define ASM_X86_REG_ESI (6)
#define ASM_X86_REG_EDI (7)





#define ASM_X86_REG_ARG_1 ASM_X86_REG_EAX
#define ASM_X86_REG_ARG_2 ASM_X86_REG_ECX
#define ASM_X86_REG_ARG_3 ASM_X86_REG_EDX
#define ASM_X86_REG_ARG_4 ASM_X86_REG_EBX


#define ASM_X86_CC_JB  (0x2) 
#define ASM_X86_CC_JAE (0x3) 
#define ASM_X86_CC_JZ  (0x4)
#define ASM_X86_CC_JE  (0x4)
#define ASM_X86_CC_JNZ (0x5)
#define ASM_X86_CC_JNE (0x5)
#define ASM_X86_CC_JBE (0x6) 
#define ASM_X86_CC_JA  (0x7) 
#define ASM_X86_CC_JL  (0xc) 
#define ASM_X86_CC_JGE (0xd) 
#define ASM_X86_CC_JLE (0xe) 
#define ASM_X86_CC_JG  (0xf) 

typedef struct _asm_x86_t {
    mp_asm_base_t base;
    int num_locals;
} asm_x86_t;

static inline void asm_x86_end_pass(asm_x86_t *as) {
    (void)as;
}

void asm_x86_mov_r32_r32(asm_x86_t *as, int dest_r32, int src_r32);
size_t asm_x86_mov_i32_to_r32(asm_x86_t *as, int32_t src_i32, int dest_r32);
void asm_x86_mov_r8_to_mem8(asm_x86_t *as, int src_r32, int dest_r32, int dest_disp);
void asm_x86_mov_r16_to_mem16(asm_x86_t *as, int src_r32, int dest_r32, int dest_disp);
void asm_x86_mov_r32_to_mem32(asm_x86_t *as, int src_r32, int dest_r32, int dest_disp);
void asm_x86_mov_mem8_to_r32zx(asm_x86_t *as, int src_r32, int src_disp, int dest_r32);
void asm_x86_mov_mem16_to_r32zx(asm_x86_t *as, int src_r32, int src_disp, int dest_r32);
void asm_x86_mov_mem32_to_r32(asm_x86_t *as, int src_r32, int src_disp, int dest_r32);
void asm_x86_not_r32(asm_x86_t *as, int dest_r32);
void asm_x86_neg_r32(asm_x86_t *as, int dest_r32);
void asm_x86_and_r32_r32(asm_x86_t *as, int dest_r32, int src_r32);
void asm_x86_or_r32_r32(asm_x86_t *as, int dest_r32, int src_r32);
void asm_x86_xor_r32_r32(asm_x86_t *as, int dest_r32, int src_r32);
void asm_x86_shl_r32_cl(asm_x86_t *as, int dest_r32);
void asm_x86_shr_r32_cl(asm_x86_t *as, int dest_r32);
void asm_x86_sar_r32_cl(asm_x86_t *as, int dest_r32);
void asm_x86_add_r32_r32(asm_x86_t *as, int dest_r32, int src_r32);
void asm_x86_sub_r32_r32(asm_x86_t *as, int dest_r32, int src_r32);
void asm_x86_mul_r32_r32(asm_x86_t *as, int dest_r32, int src_r32);
void asm_x86_cmp_r32_with_r32(asm_x86_t *as, int src_r32_a, int src_r32_b);
void asm_x86_test_r8_with_r8(asm_x86_t *as, int src_r32_a, int src_r32_b);
void asm_x86_test_r32_with_r32(asm_x86_t *as, int src_r32_a, int src_r32_b);
void asm_x86_setcc_r8(asm_x86_t *as, mp_uint_t jcc_type, int dest_r8);
void asm_x86_jmp_reg(asm_x86_t *as, int src_r86);
void asm_x86_jmp_label(asm_x86_t *as, mp_uint_t label);
void asm_x86_jcc_label(asm_x86_t *as, mp_uint_t jcc_type, mp_uint_t label);
void asm_x86_entry(asm_x86_t *as, int num_locals);
void asm_x86_exit(asm_x86_t *as);
void asm_x86_mov_arg_to_r32(asm_x86_t *as, int src_arg_num, int dest_r32);
void asm_x86_mov_local_to_r32(asm_x86_t *as, int src_local_num, int dest_r32);
void asm_x86_mov_r32_to_local(asm_x86_t *as, int src_r32, int dest_local_num);
void asm_x86_mov_local_addr_to_r32(asm_x86_t *as, int local_num, int dest_r32);
void asm_x86_mov_reg_pcrel(asm_x86_t *as, int dest_r64, mp_uint_t label);
void asm_x86_call_ind(asm_x86_t *as, size_t fun_id, mp_uint_t n_args, int temp_r32);


#define ASM_X86_REG_FUN_TABLE ASM_X86_REG_EBP

#if GENERIC_ASM_API




#define ASM_WORD_SIZE (4)

#define REG_RET ASM_X86_REG_EAX
#define REG_ARG_1 ASM_X86_REG_ARG_1
#define REG_ARG_2 ASM_X86_REG_ARG_2
#define REG_ARG_3 ASM_X86_REG_ARG_3
#define REG_ARG_4 ASM_X86_REG_ARG_4


#define REG_TEMP0 ASM_X86_REG_EAX
#define REG_TEMP1 ASM_X86_REG_ECX
#define REG_TEMP2 ASM_X86_REG_EDX


#define REG_LOCAL_1 ASM_X86_REG_EBX
#define REG_LOCAL_2 ASM_X86_REG_ESI
#define REG_LOCAL_3 ASM_X86_REG_EDI
#define REG_LOCAL_NUM (3)


#define REG_FUN_TABLE ASM_X86_REG_FUN_TABLE

#define ASM_T               asm_x86_t
#define ASM_END_PASS        asm_x86_end_pass
#define ASM_ENTRY           asm_x86_entry
#define ASM_EXIT            asm_x86_exit

#define ASM_JUMP            asm_x86_jmp_label
#define ASM_JUMP_IF_REG_ZERO(as, reg, label, bool_test) \
    do { \
        if (bool_test) { \
            asm_x86_test_r8_with_r8(as, reg, reg); \
        } else { \
            asm_x86_test_r32_with_r32(as, reg, reg); \
        } \
        asm_x86_jcc_label(as, ASM_X86_CC_JZ, label); \
    } while (0)
#define ASM_JUMP_IF_REG_NONZERO(as, reg, label, bool_test) \
    do { \
        if (bool_test) { \
            asm_x86_test_r8_with_r8(as, reg, reg); \
        } else { \
            asm_x86_test_r32_with_r32(as, reg, reg); \
        } \
        asm_x86_jcc_label(as, ASM_X86_CC_JNZ, label); \
    } while (0)
#define ASM_JUMP_IF_REG_EQ(as, reg1, reg2, label) \
    do { \
        asm_x86_cmp_r32_with_r32(as, reg1, reg2); \
        asm_x86_jcc_label(as, ASM_X86_CC_JE, label); \
    } while (0)
#define ASM_JUMP_REG(as, reg) asm_x86_jmp_reg((as), (reg))
#define ASM_CALL_IND(as, idx) asm_x86_call_ind(as, idx, mp_f_n_args[idx], ASM_X86_REG_EAX)

#define ASM_MOV_LOCAL_REG(as, local_num, reg_src) asm_x86_mov_r32_to_local((as), (reg_src), (local_num))
#define ASM_MOV_REG_IMM(as, reg_dest, imm) asm_x86_mov_i32_to_r32((as), (imm), (reg_dest))
#define ASM_MOV_REG_LOCAL(as, reg_dest, local_num) asm_x86_mov_local_to_r32((as), (local_num), (reg_dest))
#define ASM_MOV_REG_REG(as, reg_dest, reg_src) asm_x86_mov_r32_r32((as), (reg_dest), (reg_src))
#define ASM_MOV_REG_LOCAL_ADDR(as, reg_dest, local_num) asm_x86_mov_local_addr_to_r32((as), (local_num), (reg_dest))
#define ASM_MOV_REG_PCREL(as, reg_dest, label) asm_x86_mov_reg_pcrel((as), (reg_dest), (label))

#define ASM_NOT_REG(as, reg) asm_x86_not_r32((as), (reg))
#define ASM_NEG_REG(as, reg) asm_x86_neg_r32((as), (reg))
#define ASM_LSL_REG(as, reg) asm_x86_shl_r32_cl((as), (reg))
#define ASM_LSR_REG(as, reg) asm_x86_shr_r32_cl((as), (reg))
#define ASM_ASR_REG(as, reg) asm_x86_sar_r32_cl((as), (reg))
#define ASM_OR_REG_REG(as, reg_dest, reg_src) asm_x86_or_r32_r32((as), (reg_dest), (reg_src))
#define ASM_XOR_REG_REG(as, reg_dest, reg_src) asm_x86_xor_r32_r32((as), (reg_dest), (reg_src))
#define ASM_AND_REG_REG(as, reg_dest, reg_src) asm_x86_and_r32_r32((as), (reg_dest), (reg_src))
#define ASM_ADD_REG_REG(as, reg_dest, reg_src) asm_x86_add_r32_r32((as), (reg_dest), (reg_src))
#define ASM_SUB_REG_REG(as, reg_dest, reg_src) asm_x86_sub_r32_r32((as), (reg_dest), (reg_src))
#define ASM_MUL_REG_REG(as, reg_dest, reg_src) asm_x86_mul_r32_r32((as), (reg_dest), (reg_src))

#define ASM_LOAD_REG_REG(as, reg_dest, reg_base) asm_x86_mov_mem32_to_r32((as), (reg_base), 0, (reg_dest))
#define ASM_LOAD_REG_REG_OFFSET(as, reg_dest, reg_base, word_offset) asm_x86_mov_mem32_to_r32((as), (reg_base), 4 * (word_offset), (reg_dest))
#define ASM_LOAD8_REG_REG(as, reg_dest, reg_base) asm_x86_mov_mem8_to_r32zx((as), (reg_base), 0, (reg_dest))
#define ASM_LOAD16_REG_REG(as, reg_dest, reg_base) asm_x86_mov_mem16_to_r32zx((as), (reg_base), 0, (reg_dest))
#define ASM_LOAD16_REG_REG_OFFSET(as, reg_dest, reg_base, uint16_offset) asm_x86_mov_mem16_to_r32zx((as), (reg_base), 2 * (uint16_offset), (reg_dest))
#define ASM_LOAD32_REG_REG(as, reg_dest, reg_base) asm_x86_mov_mem32_to_r32((as), (reg_base), 0, (reg_dest))

#define ASM_STORE_REG_REG(as, reg_src, reg_base) asm_x86_mov_r32_to_mem32((as), (reg_src), (reg_base), 0)
#define ASM_STORE_REG_REG_OFFSET(as, reg_src, reg_base, word_offset) asm_x86_mov_r32_to_mem32((as), (reg_src), (reg_base), 4 * (word_offset))
#define ASM_STORE8_REG_REG(as, reg_src, reg_base) asm_x86_mov_r8_to_mem8((as), (reg_src), (reg_base), 0)
#define ASM_STORE16_REG_REG(as, reg_src, reg_base) asm_x86_mov_r16_to_mem16((as), (reg_src), (reg_base), 0)
#define ASM_STORE32_REG_REG(as, reg_src, reg_base) asm_x86_mov_r32_to_mem32((as), (reg_src), (reg_base), 0)

#endif 

#endif 
