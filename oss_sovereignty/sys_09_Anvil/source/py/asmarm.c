 

#include <stdio.h>
#include <assert.h>
#include <string.h>

#include "py/mpconfig.h"


#if MICROPY_EMIT_ARM

#include "py/asmarm.h"

#define SIGNED_FIT24(x) (((x) & 0xff800000) == 0) || (((x) & 0xff000000) == 0xff000000)


static void emit(asm_arm_t *as, uint op) {
    uint8_t *c = mp_asm_base_get_cur_to_write_bytes(&as->base, 4);
    if (c != NULL) {
        *(uint32_t *)c = op;
    }
}


static void emit_al(asm_arm_t *as, uint op) {
    emit(as, op | ASM_ARM_CC_AL);
}


static uint asm_arm_op_push(uint reglist) {
    
    return 0x92d0000 | (reglist & 0xFFFF);
}

static uint asm_arm_op_pop(uint reglist) {
    
    return 0x8bd0000 | (reglist & 0xFFFF);
}

static uint asm_arm_op_mov_reg(uint rd, uint rn) {
    
    return 0x1a00000 | (rd << 12) | rn;
}

static uint asm_arm_op_mov_imm(uint rd, uint imm) {
    
    return 0x3a00000 | (rd << 12) | imm;
}

static uint asm_arm_op_mvn_imm(uint rd, uint imm) {
    
    return 0x3e00000 | (rd << 12) | imm;
}

static uint asm_arm_op_mvn_reg(uint rd, uint rm) {
    
    return 0x1e00000 | (rd << 12) | rm;
}

static uint asm_arm_op_add_imm(uint rd, uint rn, uint imm) {
    
    return 0x2800000 | (rn << 16) | (rd << 12) | (imm & 0xFF);
}

static uint asm_arm_op_add_reg(uint rd, uint rn, uint rm) {
    
    return 0x0800000 | (rn << 16) | (rd << 12) | rm;
}

static uint asm_arm_op_sub_imm(uint rd, uint rn, uint imm) {
    
    return 0x2400000 | (rn << 16) | (rd << 12) | (imm & 0xFF);
}

static uint asm_arm_op_sub_reg(uint rd, uint rn, uint rm) {
    
    return 0x0400000 | (rn << 16) | (rd << 12) | rm;
}

static uint asm_arm_op_rsb_imm(uint rd, uint rn, uint imm) {
    
    return 0x2600000 | (rn << 16) | (rd << 12) | (imm & 0xFF);
}

static uint asm_arm_op_mul_reg(uint rd, uint rm, uint rs) {
    
    assert(rd != rm);
    return 0x0000090 | (rd << 16) | (rs << 8) | rm;
}

static uint asm_arm_op_and_reg(uint rd, uint rn, uint rm) {
    
    return 0x0000000 | (rn << 16) | (rd << 12) | rm;
}

static uint asm_arm_op_eor_reg(uint rd, uint rn, uint rm) {
    
    return 0x0200000 | (rn << 16) | (rd << 12) | rm;
}

static uint asm_arm_op_orr_reg(uint rd, uint rn, uint rm) {
    
    return 0x1800000 | (rn << 16) | (rd << 12) | rm;
}

void asm_arm_bkpt(asm_arm_t *as) {
    
    emit_al(as, 0x1200070);
}












void asm_arm_entry(asm_arm_t *as, int num_locals) {
    assert(num_locals >= 0);

    as->stack_adjust = 0;
    as->push_reglist = 1 << ASM_ARM_REG_R1
        | 1 << ASM_ARM_REG_R2
        | 1 << ASM_ARM_REG_R3
        | 1 << ASM_ARM_REG_R4
        | 1 << ASM_ARM_REG_R5
        | 1 << ASM_ARM_REG_R6
        | 1 << ASM_ARM_REG_R7
        | 1 << ASM_ARM_REG_R8;

    
    if (num_locals > 3) {
        as->stack_adjust = num_locals * 4;
        
        if (num_locals & 1) {
            as->stack_adjust += 4;
        }
    }

    emit_al(as, asm_arm_op_push(as->push_reglist | 1 << ASM_ARM_REG_LR));
    if (as->stack_adjust > 0) {
        emit_al(as, asm_arm_op_sub_imm(ASM_ARM_REG_SP, ASM_ARM_REG_SP, as->stack_adjust));
    }
}

void asm_arm_exit(asm_arm_t *as) {
    if (as->stack_adjust > 0) {
        emit_al(as, asm_arm_op_add_imm(ASM_ARM_REG_SP, ASM_ARM_REG_SP, as->stack_adjust));
    }

    emit_al(as, asm_arm_op_pop(as->push_reglist | (1 << ASM_ARM_REG_PC)));
}

void asm_arm_push(asm_arm_t *as, uint reglist) {
    emit_al(as, asm_arm_op_push(reglist));
}

void asm_arm_pop(asm_arm_t *as, uint reglist) {
    emit_al(as, asm_arm_op_pop(reglist));
}

void asm_arm_mov_reg_reg(asm_arm_t *as, uint reg_dest, uint reg_src) {
    emit_al(as, asm_arm_op_mov_reg(reg_dest, reg_src));
}

size_t asm_arm_mov_reg_i32(asm_arm_t *as, uint rd, int imm) {
    
    emit_al(as, 0x59f0000 | (rd << 12)); 
    emit_al(as, 0xa000000); 
    size_t loc = mp_asm_base_get_code_pos(&as->base);
    emit(as, imm);
    return loc;
}

void asm_arm_mov_reg_i32_optimised(asm_arm_t *as, uint rd, int imm) {
    
    if ((imm & 0xFF) == imm) {
        emit_al(as, asm_arm_op_mov_imm(rd, imm));
    } else if (imm < 0 && imm >= -256) {
        
        emit_al(as, asm_arm_op_mvn_imm(rd, ~imm));
    } else {
        asm_arm_mov_reg_i32(as, rd, imm);
    }
}

void asm_arm_mov_local_reg(asm_arm_t *as, int local_num, uint rd) {
    
    emit_al(as, 0x58d0000 | (rd << 12) | (local_num << 2));
}

void asm_arm_mov_reg_local(asm_arm_t *as, uint rd, int local_num) {
    
    emit_al(as, 0x59d0000 | (rd << 12) | (local_num << 2));
}

void asm_arm_cmp_reg_i8(asm_arm_t *as, uint rd, int imm) {
    
    emit_al(as, 0x3500000 | (rd << 16) | (imm & 0xFF));
}

void asm_arm_cmp_reg_reg(asm_arm_t *as, uint rd, uint rn) {
    
    emit_al(as, 0x1500000 | (rd << 16) | rn);
}

void asm_arm_setcc_reg(asm_arm_t *as, uint rd, uint cond) {
    emit(as, asm_arm_op_mov_imm(rd, 1) | cond); 
    emit(as, asm_arm_op_mov_imm(rd, 0) | (cond ^ (1 << 28))); 
}

void asm_arm_mvn_reg_reg(asm_arm_t *as, uint rd, uint rm) {
    
    
    emit_al(as, asm_arm_op_mvn_reg(rd, rm));
}

void asm_arm_add_reg_reg_reg(asm_arm_t *as, uint rd, uint rn, uint rm) {
    
    emit_al(as, asm_arm_op_add_reg(rd, rn, rm));
}

void asm_arm_rsb_reg_reg_imm(asm_arm_t *as, uint rd, uint rn, uint imm) {
    
    
    emit_al(as, asm_arm_op_rsb_imm(rd, rn, imm));
}

void asm_arm_sub_reg_reg_reg(asm_arm_t *as, uint rd, uint rn, uint rm) {
    
    emit_al(as, asm_arm_op_sub_reg(rd, rn, rm));
}

void asm_arm_mul_reg_reg_reg(asm_arm_t *as, uint rd, uint rs, uint rm) {
    
    
    emit_al(as, asm_arm_op_mul_reg(rd, rm, rs));
}

void asm_arm_and_reg_reg_reg(asm_arm_t *as, uint rd, uint rn, uint rm) {
    
    emit_al(as, asm_arm_op_and_reg(rd, rn, rm));
}

void asm_arm_eor_reg_reg_reg(asm_arm_t *as, uint rd, uint rn, uint rm) {
    
    emit_al(as, asm_arm_op_eor_reg(rd, rn, rm));
}

void asm_arm_orr_reg_reg_reg(asm_arm_t *as, uint rd, uint rn, uint rm) {
    
    emit_al(as, asm_arm_op_orr_reg(rd, rn, rm));
}

void asm_arm_mov_reg_local_addr(asm_arm_t *as, uint rd, int local_num) {
    
    emit_al(as, asm_arm_op_add_imm(rd, ASM_ARM_REG_SP, local_num << 2));
}

void asm_arm_mov_reg_pcrel(asm_arm_t *as, uint reg_dest, uint label) {
    assert(label < as->base.max_num_labels);
    mp_uint_t dest = as->base.label_offsets[label];
    mp_int_t rel = dest - as->base.code_offset;
    rel -= 12 + 8; 

    
    emit_al(as, 0x59f0000 | (reg_dest << 12)); 
    emit_al(as, 0xa000000); 
    emit(as, rel);

    
    asm_arm_add_reg_reg_reg(as, reg_dest, reg_dest, ASM_ARM_REG_PC);
}

void asm_arm_lsl_reg_reg(asm_arm_t *as, uint rd, uint rs) {
    
    emit_al(as, 0x1a00010 | (rd << 12) | (rs << 8) | rd);
}

void asm_arm_lsr_reg_reg(asm_arm_t *as, uint rd, uint rs) {
    
    emit_al(as, 0x1a00030 | (rd << 12) | (rs << 8) | rd);
}

void asm_arm_asr_reg_reg(asm_arm_t *as, uint rd, uint rs) {
    
    emit_al(as, 0x1a00050 | (rd << 12) | (rs << 8) | rd);
}

void asm_arm_ldr_reg_reg(asm_arm_t *as, uint rd, uint rn, uint byte_offset) {
    
    emit_al(as, 0x5900000 | (rn << 16) | (rd << 12) | byte_offset);
}

void asm_arm_ldrh_reg_reg(asm_arm_t *as, uint rd, uint rn) {
    
    emit_al(as, 0x1d000b0 | (rn << 16) | (rd << 12));
}

void asm_arm_ldrh_reg_reg_offset(asm_arm_t *as, uint rd, uint rn, uint byte_offset) {
    
    emit_al(as, 0x1f000b0 | (rn << 16) | (rd << 12) | ((byte_offset & 0xf0) << 4) | (byte_offset & 0xf));
}

void asm_arm_ldrb_reg_reg(asm_arm_t *as, uint rd, uint rn) {
    
    emit_al(as, 0x5d00000 | (rn << 16) | (rd << 12));
}

void asm_arm_str_reg_reg(asm_arm_t *as, uint rd, uint rm, uint byte_offset) {
    
    emit_al(as, 0x5800000 | (rm << 16) | (rd << 12) | byte_offset);
}

void asm_arm_strh_reg_reg(asm_arm_t *as, uint rd, uint rm) {
    
    emit_al(as, 0x1c000b0 | (rm << 16) | (rd << 12));
}

void asm_arm_strb_reg_reg(asm_arm_t *as, uint rd, uint rm) {
    
    emit_al(as, 0x5c00000 | (rm << 16) | (rd << 12));
}

void asm_arm_str_reg_reg_reg(asm_arm_t *as, uint rd, uint rm, uint rn) {
    
    emit_al(as, 0x7800100 | (rm << 16) | (rd << 12) | rn);
}

void asm_arm_strh_reg_reg_reg(asm_arm_t *as, uint rd, uint rm, uint rn) {
    
    emit_al(as, 0x1a00080 | (ASM_ARM_REG_R8 << 12) | rn); 
    emit_al(as, 0x18000b0 | (rm << 16) | (rd << 12) | ASM_ARM_REG_R8); 
}

void asm_arm_strb_reg_reg_reg(asm_arm_t *as, uint rd, uint rm, uint rn) {
    
    emit_al(as, 0x7c00000 | (rm << 16) | (rd << 12) | rn);
}

void asm_arm_bcc_label(asm_arm_t *as, int cond, uint label) {
    assert(label < as->base.max_num_labels);
    mp_uint_t dest = as->base.label_offsets[label];
    mp_int_t rel = dest - as->base.code_offset;
    rel -= 8; 
    rel >>= 2; 

    if (SIGNED_FIT24(rel)) {
        emit(as, cond | 0xa000000 | (rel & 0xffffff));
    } else {
        printf("asm_arm_bcc: branch does not fit in 24 bits\n");
    }
}

void asm_arm_b_label(asm_arm_t *as, uint label) {
    asm_arm_bcc_label(as, ASM_ARM_CC_AL, label);
}

void asm_arm_bl_ind(asm_arm_t *as, uint fun_id, uint reg_temp) {
    
    assert(fun_id < (0x1000 / 4));
    emit_al(as, asm_arm_op_mov_reg(ASM_ARM_REG_LR, ASM_ARM_REG_PC)); 
    emit_al(as, 0x597f000 | (fun_id << 2)); 
}

void asm_arm_bx_reg(asm_arm_t *as, uint reg_src) {
    emit_al(as, 0x012fff10 | reg_src);
}

#endif 
