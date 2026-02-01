 

#include <stdint.h>
#include <stdio.h>
#include <assert.h>
#include <string.h>

#include "py/mpconfig.h"


#if MICROPY_EMIT_X64

#include "py/asmx64.h"

 
#define WORD_SIZE                (8)

#define OPCODE_NOP               (0x90)
#define OPCODE_PUSH_R64          (0x50)  
#define OPCODE_PUSH_I64          (0x68)
#define OPCODE_PUSH_M64          (0xff)  
#define OPCODE_POP_R64           (0x58)  
#define OPCODE_RET               (0xc3)
#define OPCODE_MOV_I8_TO_R8      (0xb0)  
#define OPCODE_MOV_I64_TO_R64    (0xb8)  
#define OPCODE_MOV_I32_TO_RM32   (0xc7)
#define OPCODE_MOV_R8_TO_RM8     (0x88)  
#define OPCODE_MOV_R64_TO_RM64   (0x89)  
#define OPCODE_MOV_RM64_TO_R64   (0x8b)  
#define OPCODE_MOVZX_RM8_TO_R64  (0xb6)  
#define OPCODE_MOVZX_RM16_TO_R64 (0xb7)  
#define OPCODE_LEA_MEM_TO_R64    (0x8d)  
#define OPCODE_NOT_RM64          (0xf7)  
#define OPCODE_NEG_RM64          (0xf7)  
#define OPCODE_AND_R64_TO_RM64   (0x21)  
#define OPCODE_OR_R64_TO_RM64    (0x09)  
#define OPCODE_XOR_R64_TO_RM64   (0x31)  
#define OPCODE_ADD_R64_TO_RM64   (0x01)  
#define OPCODE_ADD_I32_TO_RM32   (0x81)  
#define OPCODE_ADD_I8_TO_RM32    (0x83)  
#define OPCODE_SUB_R64_FROM_RM64 (0x29)
#define OPCODE_SUB_I32_FROM_RM64 (0x81)  
#define OPCODE_SUB_I8_FROM_RM64  (0x83)  



#define OPCODE_SHL_RM64_CL       (0xd3)  
#define OPCODE_SHR_RM64_CL       (0xd3)  
#define OPCODE_SAR_RM64_CL       (0xd3)  


#define OPCODE_CMP_R64_WITH_RM64 (0x39)  

#define OPCODE_TEST_R8_WITH_RM8  (0x84)  
#define OPCODE_TEST_R64_WITH_RM64 (0x85)  
#define OPCODE_JMP_REL8          (0xeb)
#define OPCODE_JMP_REL32         (0xe9)
#define OPCODE_JMP_RM64          (0xff)  
#define OPCODE_JCC_REL8          (0x70)  
#define OPCODE_JCC_REL32_A       (0x0f)
#define OPCODE_JCC_REL32_B       (0x80)  
#define OPCODE_SETCC_RM8_A       (0x0f)
#define OPCODE_SETCC_RM8_B       (0x90)  
#define OPCODE_CALL_REL32        (0xe8)
#define OPCODE_CALL_RM32         (0xff)  
#define OPCODE_LEAVE             (0xc9)

#define MODRM_R64(x)    (((x) & 0x7) << 3)
#define MODRM_RM_DISP0  (0x00)
#define MODRM_RM_DISP8  (0x40)
#define MODRM_RM_DISP32 (0x80)
#define MODRM_RM_REG    (0xc0)
#define MODRM_RM_R64(x) ((x) & 0x7)

#define OP_SIZE_PREFIX (0x66)

#define REX_PREFIX  (0x40)
#define REX_W       (0x08)  
#define REX_R       (0x04)  
#define REX_X       (0x02)  
#define REX_B       (0x01)  
#define REX_W_FROM_R64(r64) ((r64) >> 0 & 0x08)
#define REX_R_FROM_R64(r64) ((r64) >> 1 & 0x04)
#define REX_X_FROM_R64(r64) ((r64) >> 2 & 0x02)
#define REX_B_FROM_R64(r64) ((r64) >> 3 & 0x01)

#define IMM32_L0(x) ((x) & 0xff)
#define IMM32_L1(x) (((x) >> 8) & 0xff)
#define IMM32_L2(x) (((x) >> 16) & 0xff)
#define IMM32_L3(x) (((x) >> 24) & 0xff)
#define IMM64_L4(x) (((x) >> 32) & 0xff)
#define IMM64_L5(x) (((x) >> 40) & 0xff)
#define IMM64_L6(x) (((x) >> 48) & 0xff)
#define IMM64_L7(x) (((x) >> 56) & 0xff)

#define UNSIGNED_FIT8(x) (((x) & 0xffffffffffffff00) == 0)
#define UNSIGNED_FIT32(x) (((x) & 0xffffffff00000000) == 0)
#define SIGNED_FIT8(x) (((x) & 0xffffff80) == 0) || (((x) & 0xffffff80) == 0xffffff80)

static inline byte *asm_x64_get_cur_to_write_bytes(asm_x64_t *as, int n) {
    return mp_asm_base_get_cur_to_write_bytes(&as->base, n);
}

static void asm_x64_write_byte_1(asm_x64_t *as, byte b1) {
    byte *c = asm_x64_get_cur_to_write_bytes(as, 1);
    if (c != NULL) {
        c[0] = b1;
    }
}

static void asm_x64_write_byte_2(asm_x64_t *as, byte b1, byte b2) {
    byte *c = asm_x64_get_cur_to_write_bytes(as, 2);
    if (c != NULL) {
        c[0] = b1;
        c[1] = b2;
    }
}

static void asm_x64_write_byte_3(asm_x64_t *as, byte b1, byte b2, byte b3) {
    byte *c = asm_x64_get_cur_to_write_bytes(as, 3);
    if (c != NULL) {
        c[0] = b1;
        c[1] = b2;
        c[2] = b3;
    }
}

static void asm_x64_write_word32(asm_x64_t *as, int w32) {
    byte *c = asm_x64_get_cur_to_write_bytes(as, 4);
    if (c != NULL) {
        c[0] = IMM32_L0(w32);
        c[1] = IMM32_L1(w32);
        c[2] = IMM32_L2(w32);
        c[3] = IMM32_L3(w32);
    }
}

static void asm_x64_write_word64(asm_x64_t *as, int64_t w64) {
    byte *c = asm_x64_get_cur_to_write_bytes(as, 8);
    if (c != NULL) {
        c[0] = IMM32_L0(w64);
        c[1] = IMM32_L1(w64);
        c[2] = IMM32_L2(w64);
        c[3] = IMM32_L3(w64);
        c[4] = IMM64_L4(w64);
        c[5] = IMM64_L5(w64);
        c[6] = IMM64_L6(w64);
        c[7] = IMM64_L7(w64);
    }
}

 

static void asm_x64_write_r64_disp(asm_x64_t *as, int r64, int disp_r64, int disp_offset) {
    uint8_t rm_disp;
    if (disp_offset == 0 && (disp_r64 & 7) != ASM_X64_REG_RBP) {
        rm_disp = MODRM_RM_DISP0;
    } else if (SIGNED_FIT8(disp_offset)) {
        rm_disp = MODRM_RM_DISP8;
    } else {
        rm_disp = MODRM_RM_DISP32;
    }
    asm_x64_write_byte_1(as, MODRM_R64(r64) | rm_disp | MODRM_RM_R64(disp_r64));
    if ((disp_r64 & 7) == ASM_X64_REG_RSP) {
        
        asm_x64_write_byte_1(as, 0x24);
    }
    if (rm_disp == MODRM_RM_DISP8) {
        asm_x64_write_byte_1(as, IMM32_L0(disp_offset));
    } else if (rm_disp == MODRM_RM_DISP32) {
        asm_x64_write_word32(as, disp_offset);
    }
}

static void asm_x64_generic_r64_r64(asm_x64_t *as, int dest_r64, int src_r64, int op) {
    asm_x64_write_byte_3(as, REX_PREFIX | REX_W | REX_R_FROM_R64(src_r64) | REX_B_FROM_R64(dest_r64), op, MODRM_R64(src_r64) | MODRM_RM_REG | MODRM_RM_R64(dest_r64));
}

void asm_x64_nop(asm_x64_t *as) {
    asm_x64_write_byte_1(as, OPCODE_NOP);
}

void asm_x64_push_r64(asm_x64_t *as, int src_r64) {
    if (src_r64 < 8) {
        asm_x64_write_byte_1(as, OPCODE_PUSH_R64 | src_r64);
    } else {
        asm_x64_write_byte_2(as, REX_PREFIX | REX_B, OPCODE_PUSH_R64 | (src_r64 & 7));
    }
}

 

 

void asm_x64_pop_r64(asm_x64_t *as, int dest_r64) {
    if (dest_r64 < 8) {
        asm_x64_write_byte_1(as, OPCODE_POP_R64 | dest_r64);
    } else {
        asm_x64_write_byte_2(as, REX_PREFIX | REX_B, OPCODE_POP_R64 | (dest_r64 & 7));
    }
}

static void asm_x64_ret(asm_x64_t *as) {
    asm_x64_write_byte_1(as, OPCODE_RET);
}

void asm_x64_mov_r64_r64(asm_x64_t *as, int dest_r64, int src_r64) {
    asm_x64_generic_r64_r64(as, dest_r64, src_r64, OPCODE_MOV_R64_TO_RM64);
}

void asm_x64_mov_r8_to_mem8(asm_x64_t *as, int src_r64, int dest_r64, int dest_disp) {
    if (src_r64 < 8 && dest_r64 < 8) {
        asm_x64_write_byte_1(as, OPCODE_MOV_R8_TO_RM8);
    } else {
        asm_x64_write_byte_2(as, REX_PREFIX | REX_R_FROM_R64(src_r64) | REX_B_FROM_R64(dest_r64), OPCODE_MOV_R8_TO_RM8);
    }
    asm_x64_write_r64_disp(as, src_r64, dest_r64, dest_disp);
}

void asm_x64_mov_r16_to_mem16(asm_x64_t *as, int src_r64, int dest_r64, int dest_disp) {
    if (src_r64 < 8 && dest_r64 < 8) {
        asm_x64_write_byte_2(as, OP_SIZE_PREFIX, OPCODE_MOV_R64_TO_RM64);
    } else {
        asm_x64_write_byte_3(as, OP_SIZE_PREFIX, REX_PREFIX | REX_R_FROM_R64(src_r64) | REX_B_FROM_R64(dest_r64), OPCODE_MOV_R64_TO_RM64);
    }
    asm_x64_write_r64_disp(as, src_r64, dest_r64, dest_disp);
}

void asm_x64_mov_r32_to_mem32(asm_x64_t *as, int src_r64, int dest_r64, int dest_disp) {
    if (src_r64 < 8 && dest_r64 < 8) {
        asm_x64_write_byte_1(as, OPCODE_MOV_R64_TO_RM64);
    } else {
        asm_x64_write_byte_2(as, REX_PREFIX | REX_R_FROM_R64(src_r64) | REX_B_FROM_R64(dest_r64), OPCODE_MOV_R64_TO_RM64);
    }
    asm_x64_write_r64_disp(as, src_r64, dest_r64, dest_disp);
}

void asm_x64_mov_r64_to_mem64(asm_x64_t *as, int src_r64, int dest_r64, int dest_disp) {
    
    asm_x64_write_byte_2(as, REX_PREFIX | REX_W | REX_R_FROM_R64(src_r64) | REX_B_FROM_R64(dest_r64), OPCODE_MOV_R64_TO_RM64);
    asm_x64_write_r64_disp(as, src_r64, dest_r64, dest_disp);
}

void asm_x64_mov_mem8_to_r64zx(asm_x64_t *as, int src_r64, int src_disp, int dest_r64) {
    if (src_r64 < 8 && dest_r64 < 8) {
        asm_x64_write_byte_2(as, 0x0f, OPCODE_MOVZX_RM8_TO_R64);
    } else {
        asm_x64_write_byte_3(as, REX_PREFIX | REX_R_FROM_R64(dest_r64) | REX_B_FROM_R64(src_r64), 0x0f, OPCODE_MOVZX_RM8_TO_R64);
    }
    asm_x64_write_r64_disp(as, dest_r64, src_r64, src_disp);
}

void asm_x64_mov_mem16_to_r64zx(asm_x64_t *as, int src_r64, int src_disp, int dest_r64) {
    if (src_r64 < 8 && dest_r64 < 8) {
        asm_x64_write_byte_2(as, 0x0f, OPCODE_MOVZX_RM16_TO_R64);
    } else {
        asm_x64_write_byte_3(as, REX_PREFIX | REX_R_FROM_R64(dest_r64) | REX_B_FROM_R64(src_r64), 0x0f, OPCODE_MOVZX_RM16_TO_R64);
    }
    asm_x64_write_r64_disp(as, dest_r64, src_r64, src_disp);
}

void asm_x64_mov_mem32_to_r64zx(asm_x64_t *as, int src_r64, int src_disp, int dest_r64) {
    if (src_r64 < 8 && dest_r64 < 8) {
        asm_x64_write_byte_1(as, OPCODE_MOV_RM64_TO_R64);
    } else {
        asm_x64_write_byte_2(as, REX_PREFIX | REX_R_FROM_R64(dest_r64) | REX_B_FROM_R64(src_r64), OPCODE_MOV_RM64_TO_R64);
    }
    asm_x64_write_r64_disp(as, dest_r64, src_r64, src_disp);
}

void asm_x64_mov_mem64_to_r64(asm_x64_t *as, int src_r64, int src_disp, int dest_r64) {
    
    asm_x64_write_byte_2(as, REX_PREFIX | REX_W | REX_R_FROM_R64(dest_r64) | REX_B_FROM_R64(src_r64), OPCODE_MOV_RM64_TO_R64);
    asm_x64_write_r64_disp(as, dest_r64, src_r64, src_disp);
}

static void asm_x64_lea_disp_to_r64(asm_x64_t *as, int src_r64, int src_disp, int dest_r64) {
    
    asm_x64_write_byte_2(as, REX_PREFIX | REX_W | REX_R_FROM_R64(dest_r64) | REX_B_FROM_R64(src_r64), OPCODE_LEA_MEM_TO_R64);
    asm_x64_write_r64_disp(as, dest_r64, src_r64, src_disp);
}

 

size_t asm_x64_mov_i32_to_r64(asm_x64_t *as, int src_i32, int dest_r64) {
    
    if (dest_r64 < 8) {
        asm_x64_write_byte_1(as, OPCODE_MOV_I64_TO_R64 | dest_r64);
    } else {
        asm_x64_write_byte_2(as, REX_PREFIX | REX_B, OPCODE_MOV_I64_TO_R64 | (dest_r64 & 7));
    }
    size_t loc = mp_asm_base_get_code_pos(&as->base);
    asm_x64_write_word32(as, src_i32);
    return loc;
}

void asm_x64_mov_i64_to_r64(asm_x64_t *as, int64_t src_i64, int dest_r64) {
    
    
    asm_x64_write_byte_2(as,
        REX_PREFIX | REX_W | (dest_r64 < 8 ? 0 : REX_B),
        OPCODE_MOV_I64_TO_R64 | (dest_r64 & 7));
    asm_x64_write_word64(as, src_i64);
}

void asm_x64_mov_i64_to_r64_optimised(asm_x64_t *as, int64_t src_i64, int dest_r64) {
    
    if (UNSIGNED_FIT32(src_i64)) {
        
        asm_x64_mov_i32_to_r64(as, src_i64 & 0xffffffff, dest_r64);
    } else {
        
        asm_x64_mov_i64_to_r64(as, src_i64, dest_r64);
    }
}

void asm_x64_not_r64(asm_x64_t *as, int dest_r64) {
    asm_x64_generic_r64_r64(as, dest_r64, 2, OPCODE_NOT_RM64);
}

void asm_x64_neg_r64(asm_x64_t *as, int dest_r64) {
    asm_x64_generic_r64_r64(as, dest_r64, 3, OPCODE_NEG_RM64);
}

void asm_x64_and_r64_r64(asm_x64_t *as, int dest_r64, int src_r64) {
    asm_x64_generic_r64_r64(as, dest_r64, src_r64, OPCODE_AND_R64_TO_RM64);
}

void asm_x64_or_r64_r64(asm_x64_t *as, int dest_r64, int src_r64) {
    asm_x64_generic_r64_r64(as, dest_r64, src_r64, OPCODE_OR_R64_TO_RM64);
}

void asm_x64_xor_r64_r64(asm_x64_t *as, int dest_r64, int src_r64) {
    asm_x64_generic_r64_r64(as, dest_r64, src_r64, OPCODE_XOR_R64_TO_RM64);
}

void asm_x64_shl_r64_cl(asm_x64_t *as, int dest_r64) {
    asm_x64_generic_r64_r64(as, dest_r64, 4, OPCODE_SHL_RM64_CL);
}

void asm_x64_shr_r64_cl(asm_x64_t *as, int dest_r64) {
    asm_x64_generic_r64_r64(as, dest_r64, 5, OPCODE_SHR_RM64_CL);
}

void asm_x64_sar_r64_cl(asm_x64_t *as, int dest_r64) {
    asm_x64_generic_r64_r64(as, dest_r64, 7, OPCODE_SAR_RM64_CL);
}

void asm_x64_add_r64_r64(asm_x64_t *as, int dest_r64, int src_r64) {
    asm_x64_generic_r64_r64(as, dest_r64, src_r64, OPCODE_ADD_R64_TO_RM64);
}

void asm_x64_sub_r64_r64(asm_x64_t *as, int dest_r64, int src_r64) {
    asm_x64_generic_r64_r64(as, dest_r64, src_r64, OPCODE_SUB_R64_FROM_RM64);
}

void asm_x64_mul_r64_r64(asm_x64_t *as, int dest_r64, int src_r64) {
    
    asm_x64_write_byte_1(as, REX_PREFIX | REX_W | REX_R_FROM_R64(dest_r64) | REX_B_FROM_R64(src_r64));
    asm_x64_write_byte_3(as, 0x0f, 0xaf, MODRM_R64(dest_r64) | MODRM_RM_REG | MODRM_RM_R64(src_r64));
}

 

static void asm_x64_sub_r64_i32(asm_x64_t *as, int dest_r64, int src_i32) {
    assert(dest_r64 < 8);
    if (SIGNED_FIT8(src_i32)) {
        
        asm_x64_write_byte_3(as, REX_PREFIX | REX_W, OPCODE_SUB_I8_FROM_RM64, MODRM_R64(5) | MODRM_RM_REG | MODRM_RM_R64(dest_r64));
        asm_x64_write_byte_1(as, src_i32 & 0xff);
    } else {
        
        asm_x64_write_byte_3(as, REX_PREFIX | REX_W, OPCODE_SUB_I32_FROM_RM64, MODRM_R64(5) | MODRM_RM_REG | MODRM_RM_R64(dest_r64));
        asm_x64_write_word32(as, src_i32);
    }
}

 

void asm_x64_cmp_r64_with_r64(asm_x64_t *as, int src_r64_a, int src_r64_b) {
    asm_x64_generic_r64_r64(as, src_r64_b, src_r64_a, OPCODE_CMP_R64_WITH_RM64);
}

 

void asm_x64_test_r8_with_r8(asm_x64_t *as, int src_r64_a, int src_r64_b) {
    assert(src_r64_a < 8);
    assert(src_r64_b < 8);
    asm_x64_write_byte_2(as, OPCODE_TEST_R8_WITH_RM8, MODRM_R64(src_r64_a) | MODRM_RM_REG | MODRM_RM_R64(src_r64_b));
}

void asm_x64_test_r64_with_r64(asm_x64_t *as, int src_r64_a, int src_r64_b) {
    asm_x64_generic_r64_r64(as, src_r64_b, src_r64_a, OPCODE_TEST_R64_WITH_RM64);
}

void asm_x64_setcc_r8(asm_x64_t *as, int jcc_type, int dest_r8) {
    assert(dest_r8 < 8);
    asm_x64_write_byte_3(as, OPCODE_SETCC_RM8_A, OPCODE_SETCC_RM8_B | jcc_type, MODRM_R64(0) | MODRM_RM_REG | MODRM_RM_R64(dest_r8));
}

void asm_x64_jmp_reg(asm_x64_t *as, int src_r64) {
    assert(src_r64 < 8);
    asm_x64_write_byte_2(as, OPCODE_JMP_RM64, MODRM_R64(4) | MODRM_RM_REG | MODRM_RM_R64(src_r64));
}

static mp_uint_t get_label_dest(asm_x64_t *as, mp_uint_t label) {
    assert(label < as->base.max_num_labels);
    return as->base.label_offsets[label];
}

void asm_x64_jmp_label(asm_x64_t *as, mp_uint_t label) {
    mp_uint_t dest = get_label_dest(as, label);
    mp_int_t rel = dest - as->base.code_offset;
    if (dest != (mp_uint_t)-1 && rel < 0) {
        
        
        rel -= 2;
        if (SIGNED_FIT8(rel)) {
            asm_x64_write_byte_2(as, OPCODE_JMP_REL8, rel & 0xff);
        } else {
            rel += 2;
            goto large_jump;
        }
    } else {
        
    large_jump:
        rel -= 5;
        asm_x64_write_byte_1(as, OPCODE_JMP_REL32);
        asm_x64_write_word32(as, rel);
    }
}

void asm_x64_jcc_label(asm_x64_t *as, int jcc_type, mp_uint_t label) {
    mp_uint_t dest = get_label_dest(as, label);
    mp_int_t rel = dest - as->base.code_offset;
    if (dest != (mp_uint_t)-1 && rel < 0) {
        
        
        rel -= 2;
        if (SIGNED_FIT8(rel)) {
            asm_x64_write_byte_2(as, OPCODE_JCC_REL8 | jcc_type, rel & 0xff);
        } else {
            rel += 2;
            goto large_jump;
        }
    } else {
        
    large_jump:
        rel -= 6;
        asm_x64_write_byte_2(as, OPCODE_JCC_REL32_A, OPCODE_JCC_REL32_B | jcc_type);
        asm_x64_write_word32(as, rel);
    }
}

void asm_x64_entry(asm_x64_t *as, int num_locals) {
    assert(num_locals >= 0);
    asm_x64_push_r64(as, ASM_X64_REG_RBP);
    asm_x64_push_r64(as, ASM_X64_REG_RBX);
    asm_x64_push_r64(as, ASM_X64_REG_R12);
    asm_x64_push_r64(as, ASM_X64_REG_R13);
    num_locals |= 1; 
    asm_x64_sub_r64_i32(as, ASM_X64_REG_RSP, num_locals * WORD_SIZE);
    as->num_locals = num_locals;
}

void asm_x64_exit(asm_x64_t *as) {
    asm_x64_sub_r64_i32(as, ASM_X64_REG_RSP, -as->num_locals * WORD_SIZE);
    asm_x64_pop_r64(as, ASM_X64_REG_R13);
    asm_x64_pop_r64(as, ASM_X64_REG_R12);
    asm_x64_pop_r64(as, ASM_X64_REG_RBX);
    asm_x64_pop_r64(as, ASM_X64_REG_RBP);
    asm_x64_ret(as);
}












static int asm_x64_local_offset_from_rsp(asm_x64_t *as, int local_num) {
    (void)as;
    
    return local_num * WORD_SIZE;
}

void asm_x64_mov_local_to_r64(asm_x64_t *as, int src_local_num, int dest_r64) {
    asm_x64_mov_mem64_to_r64(as, ASM_X64_REG_RSP, asm_x64_local_offset_from_rsp(as, src_local_num), dest_r64);
}

void asm_x64_mov_r64_to_local(asm_x64_t *as, int src_r64, int dest_local_num) {
    asm_x64_mov_r64_to_mem64(as, src_r64, ASM_X64_REG_RSP, asm_x64_local_offset_from_rsp(as, dest_local_num));
}

void asm_x64_mov_local_addr_to_r64(asm_x64_t *as, int local_num, int dest_r64) {
    int offset = asm_x64_local_offset_from_rsp(as, local_num);
    if (offset == 0) {
        asm_x64_mov_r64_r64(as, dest_r64, ASM_X64_REG_RSP);
    } else {
        asm_x64_lea_disp_to_r64(as, ASM_X64_REG_RSP, offset, dest_r64);
    }
}

void asm_x64_mov_reg_pcrel(asm_x64_t *as, int dest_r64, mp_uint_t label) {
    mp_uint_t dest = get_label_dest(as, label);
    mp_int_t rel = dest - (as->base.code_offset + 7);
    asm_x64_write_byte_3(as, REX_PREFIX | REX_W | REX_R_FROM_R64(dest_r64), OPCODE_LEA_MEM_TO_R64, MODRM_R64(dest_r64) | MODRM_RM_R64(5));
    asm_x64_write_word32(as, rel);
}

 

 

void asm_x64_call_ind(asm_x64_t *as, size_t fun_id, int temp_r64) {
    assert(temp_r64 < 8);
    asm_x64_mov_mem64_to_r64(as, ASM_X64_REG_FUN_TABLE, fun_id * WORD_SIZE, temp_r64);
    asm_x64_write_byte_2(as, OPCODE_CALL_RM32, MODRM_R64(2) | MODRM_RM_REG | MODRM_RM_R64(temp_r64));
}

#endif 
