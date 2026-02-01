 

#include <stdio.h>
#include <assert.h>

#include "py/runtime.h"


#if MICROPY_EMIT_XTENSA || MICROPY_EMIT_INLINE_XTENSA || MICROPY_EMIT_XTENSAWIN

#include "py/asmxtensa.h"

#define WORD_SIZE (4)
#define SIGNED_FIT8(x) ((((x) & 0xffffff80) == 0) || (((x) & 0xffffff80) == 0xffffff80))
#define SIGNED_FIT12(x) ((((x) & 0xfffff800) == 0) || (((x) & 0xfffff800) == 0xfffff800))

void asm_xtensa_end_pass(asm_xtensa_t *as) {
    as->num_const = as->cur_const;
    as->cur_const = 0;

    #if 0
    
    if (as->base.pass == MP_ASM_PASS_EMIT) {
        uint8_t *d = as->base.code_base;
        printf("XTENSA ASM:");
        for (int i = 0; i < ((as->base.code_size + 15) & ~15); ++i) {
            if (i % 16 == 0) {
                printf("\n%08x:", (uint32_t)&d[i]);
            }
            if (i % 2 == 0) {
                printf(" ");
            }
            printf("%02x", d[i]);
        }
        printf("\n");
    }
    #endif
}

void asm_xtensa_entry(asm_xtensa_t *as, int num_locals) {
    
    asm_xtensa_op_j(as, as->num_const * WORD_SIZE + 4 - 4);
    mp_asm_base_get_cur_to_write_bytes(&as->base, 1); 
    as->const_table = (uint32_t *)mp_asm_base_get_cur_to_write_bytes(&as->base, as->num_const * 4);

    
    as->stack_adjust = (((ASM_XTENSA_NUM_REGS_SAVED + num_locals) * WORD_SIZE) + 15) & ~15;
    if (SIGNED_FIT8(-as->stack_adjust)) {
        asm_xtensa_op_addi(as, ASM_XTENSA_REG_A1, ASM_XTENSA_REG_A1, -as->stack_adjust);
    } else {
        asm_xtensa_op_movi(as, ASM_XTENSA_REG_A9, as->stack_adjust);
        asm_xtensa_op_sub(as, ASM_XTENSA_REG_A1, ASM_XTENSA_REG_A1, ASM_XTENSA_REG_A9);
    }

    
    asm_xtensa_op_s32i_n(as, ASM_XTENSA_REG_A0, ASM_XTENSA_REG_A1, 0);
    for (int i = 1; i < ASM_XTENSA_NUM_REGS_SAVED; ++i) {
        asm_xtensa_op_s32i_n(as, ASM_XTENSA_REG_A11 + i, ASM_XTENSA_REG_A1, i);
    }
}

void asm_xtensa_exit(asm_xtensa_t *as) {
    
    for (int i = ASM_XTENSA_NUM_REGS_SAVED - 1; i >= 1; --i) {
        asm_xtensa_op_l32i_n(as, ASM_XTENSA_REG_A11 + i, ASM_XTENSA_REG_A1, i);
    }
    asm_xtensa_op_l32i_n(as, ASM_XTENSA_REG_A0, ASM_XTENSA_REG_A1, 0);

    
    if (SIGNED_FIT8(as->stack_adjust)) {
        asm_xtensa_op_addi(as, ASM_XTENSA_REG_A1, ASM_XTENSA_REG_A1, as->stack_adjust);
    } else {
        asm_xtensa_op_movi(as, ASM_XTENSA_REG_A9, as->stack_adjust);
        asm_xtensa_op_add_n(as, ASM_XTENSA_REG_A1, ASM_XTENSA_REG_A1, ASM_XTENSA_REG_A9);
    }

    asm_xtensa_op_ret_n(as);
}

void asm_xtensa_entry_win(asm_xtensa_t *as, int num_locals) {
    
    asm_xtensa_op_j(as, as->num_const * WORD_SIZE + 4 - 4);
    mp_asm_base_get_cur_to_write_bytes(&as->base, 1); 
    as->const_table = (uint32_t *)mp_asm_base_get_cur_to_write_bytes(&as->base, as->num_const * 4);

    as->stack_adjust = 32 + ((((ASM_XTENSA_NUM_REGS_SAVED_WIN + num_locals) * WORD_SIZE) + 15) & ~15);
    asm_xtensa_op_entry(as, ASM_XTENSA_REG_A1, as->stack_adjust);
    asm_xtensa_op_s32i_n(as, ASM_XTENSA_REG_A0, ASM_XTENSA_REG_A1, 0);
}

void asm_xtensa_exit_win(asm_xtensa_t *as) {
    asm_xtensa_op_l32i_n(as, ASM_XTENSA_REG_A0, ASM_XTENSA_REG_A1, 0);
    asm_xtensa_op_retw_n(as);
}

static uint32_t get_label_dest(asm_xtensa_t *as, uint label) {
    assert(label < as->base.max_num_labels);
    return as->base.label_offsets[label];
}

void asm_xtensa_op16(asm_xtensa_t *as, uint16_t op) {
    uint8_t *c = mp_asm_base_get_cur_to_write_bytes(&as->base, 2);
    if (c != NULL) {
        c[0] = op;
        c[1] = op >> 8;
    }
}

void asm_xtensa_op24(asm_xtensa_t *as, uint32_t op) {
    uint8_t *c = mp_asm_base_get_cur_to_write_bytes(&as->base, 3);
    if (c != NULL) {
        c[0] = op;
        c[1] = op >> 8;
        c[2] = op >> 16;
    }
}

void asm_xtensa_j_label(asm_xtensa_t *as, uint label) {
    uint32_t dest = get_label_dest(as, label);
    int32_t rel = dest - as->base.code_offset - 4;
    
    asm_xtensa_op_j(as, rel);
}

void asm_xtensa_bccz_reg_label(asm_xtensa_t *as, uint cond, uint reg, uint label) {
    uint32_t dest = get_label_dest(as, label);
    int32_t rel = dest - as->base.code_offset - 4;
    if (as->base.pass == MP_ASM_PASS_EMIT && !SIGNED_FIT12(rel)) {
        printf("ERROR: xtensa bccz out of range\n");
    }
    asm_xtensa_op_bccz(as, cond, reg, rel);
}

void asm_xtensa_bcc_reg_reg_label(asm_xtensa_t *as, uint cond, uint reg1, uint reg2, uint label) {
    uint32_t dest = get_label_dest(as, label);
    int32_t rel = dest - as->base.code_offset - 4;
    if (as->base.pass == MP_ASM_PASS_EMIT && !SIGNED_FIT8(rel)) {
        printf("ERROR: xtensa bcc out of range\n");
    }
    asm_xtensa_op_bcc(as, cond, reg1, reg2, rel);
}


void asm_xtensa_setcc_reg_reg_reg(asm_xtensa_t *as, uint cond, uint reg_dest, uint reg_src1, uint reg_src2) {
    asm_xtensa_op_movi_n(as, reg_dest, 1);
    asm_xtensa_op_bcc(as, cond, reg_src1, reg_src2, 1);
    asm_xtensa_op_movi_n(as, reg_dest, 0);
}

size_t asm_xtensa_mov_reg_i32(asm_xtensa_t *as, uint reg_dest, uint32_t i32) {
    
    uint32_t const_table_offset = (uint8_t *)as->const_table - as->base.code_base;
    size_t loc = const_table_offset + as->cur_const * WORD_SIZE;
    asm_xtensa_op_l32r(as, reg_dest, as->base.code_offset, loc);
    
    if (as->const_table != NULL) {
        as->const_table[as->cur_const] = i32;
    }
    ++as->cur_const;
    return loc;
}

void asm_xtensa_mov_reg_i32_optimised(asm_xtensa_t *as, uint reg_dest, uint32_t i32) {
    if (-32 <= (int)i32 && (int)i32 <= 95) {
        asm_xtensa_op_movi_n(as, reg_dest, i32);
    } else if (SIGNED_FIT12(i32)) {
        asm_xtensa_op_movi(as, reg_dest, i32);
    } else {
        asm_xtensa_mov_reg_i32(as, reg_dest, i32);
    }
}

void asm_xtensa_mov_local_reg(asm_xtensa_t *as, int local_num, uint reg_src) {
    asm_xtensa_op_s32i(as, reg_src, ASM_XTENSA_REG_A1, local_num);
}

void asm_xtensa_mov_reg_local(asm_xtensa_t *as, uint reg_dest, int local_num) {
    asm_xtensa_op_l32i(as, reg_dest, ASM_XTENSA_REG_A1, local_num);
}

void asm_xtensa_mov_reg_local_addr(asm_xtensa_t *as, uint reg_dest, int local_num) {
    uint off = local_num * WORD_SIZE;
    if (SIGNED_FIT8(off)) {
        asm_xtensa_op_addi(as, reg_dest, ASM_XTENSA_REG_A1, off);
    } else {
        asm_xtensa_op_movi(as, reg_dest, off);
        asm_xtensa_op_add_n(as, reg_dest, reg_dest, ASM_XTENSA_REG_A1);
    }
}

void asm_xtensa_mov_reg_pcrel(asm_xtensa_t *as, uint reg_dest, uint label) {
    
    uint32_t dest = get_label_dest(as, label);
    int32_t rel = dest - as->base.code_offset;
    rel -= 3 + 3; 
    asm_xtensa_op_movi(as, reg_dest, rel); 

    
    
    
    
    
    
    uint32_t off = as->base.code_offset >> 1 & 1;
    uint32_t pad = (5 - as->base.code_offset) & 3;
    asm_xtensa_op_call0(as, off);
    mp_asm_base_get_cur_to_write_bytes(&as->base, pad);

    
    asm_xtensa_op_add_n(as, reg_dest, reg_dest, ASM_XTENSA_REG_A0);
}

void asm_xtensa_l32i_optimised(asm_xtensa_t *as, uint reg_dest, uint reg_base, uint word_offset) {
    if (word_offset < 16) {
        asm_xtensa_op_l32i_n(as, reg_dest, reg_base, word_offset);
    } else if (word_offset < 256) {
        asm_xtensa_op_l32i(as, reg_dest, reg_base, word_offset);
    } else {
        mp_raise_msg(&mp_type_RuntimeError, MP_ERROR_TEXT("asm overflow"));
    }
}

void asm_xtensa_s32i_optimised(asm_xtensa_t *as, uint reg_src, uint reg_base, uint word_offset) {
    if (word_offset < 16) {
        asm_xtensa_op_s32i_n(as, reg_src, reg_base, word_offset);
    } else if (word_offset < 256) {
        asm_xtensa_op_s32i(as, reg_src, reg_base, word_offset);
    } else {
        mp_raise_msg(&mp_type_RuntimeError, MP_ERROR_TEXT("asm overflow"));
    }
}

void asm_xtensa_call_ind(asm_xtensa_t *as, uint idx) {
    asm_xtensa_l32i_optimised(as, ASM_XTENSA_REG_A0, ASM_XTENSA_REG_FUN_TABLE, idx);
    asm_xtensa_op_callx0(as, ASM_XTENSA_REG_A0);
}

void asm_xtensa_call_ind_win(asm_xtensa_t *as, uint idx) {
    asm_xtensa_l32i_optimised(as, ASM_XTENSA_REG_A8, ASM_XTENSA_REG_FUN_TABLE_WIN, idx);
    asm_xtensa_op_callx8(as, ASM_XTENSA_REG_A8);
}

#endif 
