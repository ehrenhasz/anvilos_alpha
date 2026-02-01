 

#include <stdio.h>
#include <assert.h>
#include <string.h>

#include "py/mpconfig.h"


#if MICROPY_EMIT_THUMB || MICROPY_EMIT_INLINE_THUMB

#include "py/mpstate.h"
#include "py/asmthumb.h"

#ifdef _MSC_VER
#include <intrin.h>

static uint32_t mp_clz(uint32_t x) {
    unsigned long lz = 0;
    return _BitScanReverse(&lz, x) ? (sizeof(x) * 8 - 1) - lz : 0;
}

static uint32_t mp_ctz(uint32_t x) {
    unsigned long tz = 0;
    return _BitScanForward(&tz, x) ? tz : 0;
}
#else
#define mp_clz(x) __builtin_clz(x)
#define mp_ctz(x) __builtin_ctz(x)
#endif

#define UNSIGNED_FIT5(x) ((uint32_t)(x) < 32)
#define UNSIGNED_FIT7(x) ((uint32_t)(x) < 128)
#define UNSIGNED_FIT8(x) (((x) & 0xffffff00) == 0)
#define UNSIGNED_FIT16(x) (((x) & 0xffff0000) == 0)
#define SIGNED_FIT8(x) (((x) & 0xffffff80) == 0) || (((x) & 0xffffff80) == 0xffffff80)
#define SIGNED_FIT9(x) (((x) & 0xffffff00) == 0) || (((x) & 0xffffff00) == 0xffffff00)
#define SIGNED_FIT12(x) (((x) & 0xfffff800) == 0) || (((x) & 0xfffff800) == 0xfffff800)
#define SIGNED_FIT23(x) (((x) & 0xffc00000) == 0) || (((x) & 0xffc00000) == 0xffc00000)


#define OP_ADD_W_RRI_HI(reg_src) (0xf200 | (reg_src))
#define OP_ADD_W_RRI_LO(reg_dest, imm11) ((imm11 << 4 & 0x7000) | reg_dest << 8 | (imm11 & 0xff))
#define OP_SUB_W_RRI_HI(reg_src) (0xf2a0 | (reg_src))
#define OP_SUB_W_RRI_LO(reg_dest, imm11) ((imm11 << 4 & 0x7000) | reg_dest << 8 | (imm11 & 0xff))

#define OP_LDR_W_HI(reg_base) (0xf8d0 | (reg_base))
#define OP_LDR_W_LO(reg_dest, imm12) ((reg_dest) << 12 | (imm12))

#define OP_LDRH_W_HI(reg_base) (0xf8b0 | (reg_base))
#define OP_LDRH_W_LO(reg_dest, imm12) ((reg_dest) << 12 | (imm12))

static inline byte *asm_thumb_get_cur_to_write_bytes(asm_thumb_t *as, int n) {
    return mp_asm_base_get_cur_to_write_bytes(&as->base, n);
}

 

 


#define OP_PUSH_RLIST(rlolist)      (0xb400 | (rlolist))
#define OP_PUSH_RLIST_LR(rlolist)   (0xb400 | 0x0100 | (rlolist))
#define OP_POP_RLIST(rlolist)       (0xbc00 | (rlolist))
#define OP_POP_RLIST_PC(rlolist)    (0xbc00 | 0x0100 | (rlolist))


#define OP_ADD_SP(num_words) (0xb000 | (num_words))
#define OP_SUB_SP(num_words) (0xb080 | (num_words))












void asm_thumb_entry(asm_thumb_t *as, int num_locals) {
    assert(num_locals >= 0);

    
    
    #if MICROPY_DYNAMIC_COMPILER || MICROPY_EMIT_ARM || (defined(__arm__) && !defined(__thumb2__) && !defined(__thumb__))
    #if MICROPY_DYNAMIC_COMPILER
    if (mp_dynamic_compiler.native_arch == MP_NATIVE_ARCH_ARMV6)
    #endif
    {
        asm_thumb_op32(as, 0x4010, 0xe92d); 
        asm_thumb_op32(as, 0xe009, 0xe28f); 
        asm_thumb_op32(as, 0xff3e, 0xe12f); 
        asm_thumb_op32(as, 0x4010, 0xe8bd); 
        asm_thumb_op32(as, 0xff1e, 0xe12f); 
    }
    #endif

    
    
    
    
    
    uint reglist;
    uint stack_adjust;
    
    switch (num_locals) {
        case 0:
            reglist = 0xf2;
            stack_adjust = 0;
            break;

        case 1:
            reglist = 0xf2;
            stack_adjust = 0;
            break;

        case 2:
            reglist = 0xfe;
            stack_adjust = 0;
            break;

        case 3:
            reglist = 0xfe;
            stack_adjust = 0;
            break;

        default:
            reglist = 0xfe;
            stack_adjust = ((num_locals - 3) + 1) & (~1);
            break;
    }
    asm_thumb_op16(as, OP_PUSH_RLIST_LR(reglist));
    if (stack_adjust > 0) {
        if (asm_thumb_allow_armv7m(as)) {
            if (UNSIGNED_FIT7(stack_adjust)) {
                asm_thumb_op16(as, OP_SUB_SP(stack_adjust));
            } else {
                asm_thumb_op32(as, OP_SUB_W_RRI_HI(ASM_THUMB_REG_SP), OP_SUB_W_RRI_LO(ASM_THUMB_REG_SP, stack_adjust * 4));
            }
        } else {
            int adj = stack_adjust;
            
            while (!UNSIGNED_FIT7(adj)) {
                asm_thumb_op16(as, OP_SUB_SP(127));
                adj -= 127;
            }
            asm_thumb_op16(as, OP_SUB_SP(adj));
        }
    }
    as->push_reglist = reglist;
    as->stack_adjust = stack_adjust;
}

void asm_thumb_exit(asm_thumb_t *as) {
    if (as->stack_adjust > 0) {
        if (asm_thumb_allow_armv7m(as)) {
            if (UNSIGNED_FIT7(as->stack_adjust)) {
                asm_thumb_op16(as, OP_ADD_SP(as->stack_adjust));
            } else {
                asm_thumb_op32(as, OP_ADD_W_RRI_HI(ASM_THUMB_REG_SP), OP_ADD_W_RRI_LO(ASM_THUMB_REG_SP, as->stack_adjust * 4));
            }
        } else {
            int adj = as->stack_adjust;
            
            while (!UNSIGNED_FIT7(adj)) {
                asm_thumb_op16(as, OP_ADD_SP(127));
                adj -= 127;
            }
            asm_thumb_op16(as, OP_ADD_SP(adj));
        }
    }
    asm_thumb_op16(as, OP_POP_RLIST_PC(as->push_reglist));
}

static mp_uint_t get_label_dest(asm_thumb_t *as, uint label) {
    assert(label < as->base.max_num_labels);
    return as->base.label_offsets[label];
}

void asm_thumb_op16(asm_thumb_t *as, uint op) {
    byte *c = asm_thumb_get_cur_to_write_bytes(as, 2);
    if (c != NULL) {
        
        c[0] = op;
        c[1] = op >> 8;
    }
}

void asm_thumb_op32(asm_thumb_t *as, uint op1, uint op2) {
    byte *c = asm_thumb_get_cur_to_write_bytes(as, 4);
    if (c != NULL) {
        
        c[0] = op1;
        c[1] = op1 >> 8;
        c[2] = op2;
        c[3] = op2 >> 8;
    }
}

#define OP_FORMAT_4(op, rlo_dest, rlo_src) ((op) | ((rlo_src) << 3) | (rlo_dest))

void asm_thumb_format_4(asm_thumb_t *as, uint op, uint rlo_dest, uint rlo_src) {
    assert(rlo_dest < ASM_THUMB_REG_R8);
    assert(rlo_src < ASM_THUMB_REG_R8);
    asm_thumb_op16(as, OP_FORMAT_4(op, rlo_dest, rlo_src));
}

void asm_thumb_mov_reg_reg(asm_thumb_t *as, uint reg_dest, uint reg_src) {
    uint op_lo;
    if (reg_src < 8) {
        op_lo = reg_src << 3;
    } else {
        op_lo = 0x40 | ((reg_src - 8) << 3);
    }
    if (reg_dest < 8) {
        op_lo |= reg_dest;
    } else {
        op_lo |= 0x80 | (reg_dest - 8);
    }
    
    asm_thumb_op16(as, 0x4600 | op_lo);
}


void asm_thumb_mov_reg_i16(asm_thumb_t *as, uint mov_op, uint reg_dest, int i16_src) {
    assert(reg_dest < ASM_THUMB_REG_R15);
    
    asm_thumb_op32(as, mov_op | ((i16_src >> 1) & 0x0400) | ((i16_src >> 12) & 0xf), ((i16_src << 4) & 0x7000) | (reg_dest << 8) | (i16_src & 0xff));
}

static void asm_thumb_mov_rlo_i16(asm_thumb_t *as, uint rlo_dest, int i16_src) {
    asm_thumb_mov_rlo_i8(as, rlo_dest, (i16_src >> 8) & 0xff);
    asm_thumb_lsl_rlo_rlo_i5(as, rlo_dest, rlo_dest, 8);
    asm_thumb_add_rlo_i8(as, rlo_dest, i16_src & 0xff);
}

#define OP_B_N(byte_offset) (0xe000 | (((byte_offset) >> 1) & 0x07ff))

bool asm_thumb_b_n_label(asm_thumb_t *as, uint label) {
    mp_uint_t dest = get_label_dest(as, label);
    mp_int_t rel = dest - as->base.code_offset;
    rel -= 4; 
    asm_thumb_op16(as, OP_B_N(rel));
    return as->base.pass != MP_ASM_PASS_EMIT || SIGNED_FIT12(rel);
}

#define OP_BCC_N(cond, byte_offset) (0xd000 | ((cond) << 8) | (((byte_offset) >> 1) & 0x00ff))


#define OP_BCC_W_HI(cond, byte_offset) (0xf000 | ((cond) << 6) | (((byte_offset) >> 10) & 0x0400) | (((byte_offset) >> 14) & 0x003f))
#define OP_BCC_W_LO(byte_offset) (0x8000 | ((byte_offset) & 0x2000) | (((byte_offset) >> 1) & 0x0fff))

bool asm_thumb_bcc_nw_label(asm_thumb_t *as, int cond, uint label, bool wide) {
    mp_uint_t dest = get_label_dest(as, label);
    mp_int_t rel = dest - as->base.code_offset;
    rel -= 4; 
    if (!wide) {
        asm_thumb_op16(as, OP_BCC_N(cond, rel));
        return as->base.pass != MP_ASM_PASS_EMIT || SIGNED_FIT9(rel);
    } else if (asm_thumb_allow_armv7m(as)) {
        asm_thumb_op32(as, OP_BCC_W_HI(cond, rel), OP_BCC_W_LO(rel));
        return true;
    } else {
        
        return false;
    }
}

#define OP_BL_HI(byte_offset) (0xf000 | (((byte_offset) >> 12) & 0x07ff))
#define OP_BL_LO(byte_offset) (0xf800 | (((byte_offset) >> 1) & 0x07ff))

bool asm_thumb_bl_label(asm_thumb_t *as, uint label) {
    mp_uint_t dest = get_label_dest(as, label);
    mp_int_t rel = dest - as->base.code_offset;
    rel -= 4; 
    asm_thumb_op32(as, OP_BL_HI(rel), OP_BL_LO(rel));
    return as->base.pass != MP_ASM_PASS_EMIT || SIGNED_FIT23(rel);
}

size_t asm_thumb_mov_reg_i32(asm_thumb_t *as, uint reg_dest, mp_uint_t i32) {
    
    

    size_t loc = mp_asm_base_get_code_pos(&as->base);

    if (asm_thumb_allow_armv7m(as)) {
        asm_thumb_mov_reg_i16(as, ASM_THUMB_OP_MOVW, reg_dest, i32);
        asm_thumb_mov_reg_i16(as, ASM_THUMB_OP_MOVT, reg_dest, i32 >> 16);
    } else {
        
        assert(reg_dest < ASM_THUMB_REG_R8);

        
        assert(!as->base.code_base || !(3u & (uintptr_t)as->base.code_base));

        
        
        
        
        
        
        if (as->base.code_offset & 2u) {
            asm_thumb_op16(as, ASM_THUMB_OP_NOP);
        }
        asm_thumb_ldr_rlo_pcrel_i8(as, reg_dest, 0);
        asm_thumb_op16(as, OP_B_N(2));
        asm_thumb_op16(as, i32 & 0xffff);
        asm_thumb_op16(as, i32 >> 16);
    }

    return loc;
}

void asm_thumb_mov_reg_i32_optimised(asm_thumb_t *as, uint reg_dest, int i32) {
    if (reg_dest < 8 && UNSIGNED_FIT8(i32)) {
        asm_thumb_mov_rlo_i8(as, reg_dest, i32);
    } else if (asm_thumb_allow_armv7m(as)) {
        if (UNSIGNED_FIT16(i32)) {
            asm_thumb_mov_reg_i16(as, ASM_THUMB_OP_MOVW, reg_dest, i32);
        } else {
            asm_thumb_mov_reg_i32(as, reg_dest, i32);
        }
    } else {
        uint rlo_dest = reg_dest;
        assert(rlo_dest < ASM_THUMB_REG_R8); 

        bool negate = i32 < 0 && ((i32 + i32) & 0xffffffffu); 
        if (negate) {
            i32 = -i32;
        }

        uint clz = mp_clz(i32);
        uint ctz = i32 ? mp_ctz(i32) : 0;
        assert(clz + ctz <= 32);
        if (clz + ctz >= 24) {
            asm_thumb_mov_rlo_i8(as, rlo_dest, (i32 >> ctz) & 0xff);
            asm_thumb_lsl_rlo_rlo_i5(as, rlo_dest, rlo_dest, ctz);
        } else if (UNSIGNED_FIT16(i32)) {
            asm_thumb_mov_rlo_i16(as, rlo_dest, i32);
        } else {
            if (negate) {
                
                negate = false;
                i32 = -i32;
            }
            asm_thumb_mov_reg_i32(as, rlo_dest, i32);
        }
        if (negate) {
            asm_thumb_neg_rlo_rlo(as, rlo_dest, rlo_dest);
        }
    }
}

#define OP_STR_TO_SP_OFFSET(rlo_dest, word_offset) (0x9000 | ((rlo_dest) << 8) | ((word_offset) & 0x00ff))
#define OP_LDR_FROM_SP_OFFSET(rlo_dest, word_offset) (0x9800 | ((rlo_dest) << 8) | ((word_offset) & 0x00ff))

static void asm_thumb_mov_local_check(asm_thumb_t *as, int word_offset) {
    if (as->base.pass >= MP_ASM_PASS_EMIT) {
        assert(word_offset >= 0);
        if (!UNSIGNED_FIT8(word_offset)) {
            mp_raise_NotImplementedError(MP_ERROR_TEXT("too many locals for native method"));
        }
    }
}

void asm_thumb_mov_local_reg(asm_thumb_t *as, int local_num, uint rlo_src) {
    assert(rlo_src < ASM_THUMB_REG_R8);
    int word_offset = local_num;
    asm_thumb_mov_local_check(as, word_offset);
    asm_thumb_op16(as, OP_STR_TO_SP_OFFSET(rlo_src, word_offset));
}

void asm_thumb_mov_reg_local(asm_thumb_t *as, uint rlo_dest, int local_num) {
    assert(rlo_dest < ASM_THUMB_REG_R8);
    int word_offset = local_num;
    asm_thumb_mov_local_check(as, word_offset);
    asm_thumb_op16(as, OP_LDR_FROM_SP_OFFSET(rlo_dest, word_offset));
}

#define OP_ADD_REG_SP_OFFSET(rlo_dest, word_offset) (0xa800 | ((rlo_dest) << 8) | ((word_offset) & 0x00ff))

void asm_thumb_mov_reg_local_addr(asm_thumb_t *as, uint rlo_dest, int local_num) {
    assert(rlo_dest < ASM_THUMB_REG_R8);
    int word_offset = local_num;
    assert(as->base.pass < MP_ASM_PASS_EMIT || word_offset >= 0);
    asm_thumb_op16(as, OP_ADD_REG_SP_OFFSET(rlo_dest, word_offset));
}

void asm_thumb_mov_reg_pcrel(asm_thumb_t *as, uint rlo_dest, uint label) {
    mp_uint_t dest = get_label_dest(as, label);
    mp_int_t rel = dest - as->base.code_offset;
    rel |= 1; 
    if (asm_thumb_allow_armv7m(as)) {
        rel -= 6 + 4; 
        asm_thumb_mov_reg_i16(as, ASM_THUMB_OP_MOVW, rlo_dest, rel); 
        asm_thumb_sxth_rlo_rlo(as, rlo_dest, rlo_dest); 
    } else {
        rel -= 8 + 4; 
        
        asm_thumb_mov_rlo_i16(as, rlo_dest, rel);
        
        asm_thumb_sxth_rlo_rlo(as, rlo_dest, rlo_dest);
    }
    asm_thumb_add_reg_reg(as, rlo_dest, ASM_THUMB_REG_R15); 
}


static inline void asm_thumb_ldr_reg_reg_i12(asm_thumb_t *as, uint reg_dest, uint reg_base, uint word_offset) {
    asm_thumb_op32(as, OP_LDR_W_HI(reg_base), OP_LDR_W_LO(reg_dest, word_offset * 4));
}


static void asm_thumb_add_reg_reg_offset(asm_thumb_t *as, uint reg_dest, uint reg_base, uint offset, uint offset_shift) {
    if (reg_dest < ASM_THUMB_REG_R8 && reg_base < ASM_THUMB_REG_R8) {
        if (offset << offset_shift < 256) {
            if (reg_dest != reg_base) {
                asm_thumb_mov_reg_reg(as, reg_dest, reg_base);
            }
            asm_thumb_add_rlo_i8(as, reg_dest, offset << offset_shift);
        } else if (UNSIGNED_FIT8(offset) && reg_dest != reg_base) {
            asm_thumb_mov_rlo_i8(as, reg_dest, offset);
            asm_thumb_lsl_rlo_rlo_i5(as, reg_dest, reg_dest, offset_shift);
            asm_thumb_add_rlo_rlo_rlo(as, reg_dest, reg_dest, reg_base);
        } else if (reg_dest != reg_base) {
            asm_thumb_mov_rlo_i16(as, reg_dest, offset << offset_shift);
            asm_thumb_add_rlo_rlo_rlo(as, reg_dest, reg_dest, reg_dest);
        } else {
            uint reg_other = reg_dest ^ 7;
            asm_thumb_op16(as, OP_PUSH_RLIST((1 << reg_other)));
            asm_thumb_mov_rlo_i16(as, reg_other, offset << offset_shift);
            asm_thumb_add_rlo_rlo_rlo(as, reg_dest, reg_dest, reg_other);
            asm_thumb_op16(as, OP_POP_RLIST((1 << reg_other)));
        }
    } else {
        assert(0); 
    }
}

void asm_thumb_ldr_reg_reg_i12_optimised(asm_thumb_t *as, uint reg_dest, uint reg_base, uint word_offset) {
    if (reg_dest < ASM_THUMB_REG_R8 && reg_base < ASM_THUMB_REG_R8 && UNSIGNED_FIT5(word_offset)) {
        asm_thumb_ldr_rlo_rlo_i5(as, reg_dest, reg_base, word_offset);
    } else if (asm_thumb_allow_armv7m(as)) {
        asm_thumb_ldr_reg_reg_i12(as, reg_dest, reg_base, word_offset);
    } else {
        asm_thumb_add_reg_reg_offset(as, reg_dest, reg_base, word_offset - 31, 2);
        asm_thumb_ldr_rlo_rlo_i5(as, reg_dest, reg_dest, 31);
    }
}


static inline void asm_thumb_ldrh_reg_reg_i12(asm_thumb_t *as, uint reg_dest, uint reg_base, uint uint16_offset) {
    asm_thumb_op32(as, OP_LDRH_W_HI(reg_base), OP_LDRH_W_LO(reg_dest, uint16_offset * 2));
}

void asm_thumb_ldrh_reg_reg_i12_optimised(asm_thumb_t *as, uint reg_dest, uint reg_base, uint uint16_offset) {
    if (reg_dest < ASM_THUMB_REG_R8 && reg_base < ASM_THUMB_REG_R8 && UNSIGNED_FIT5(uint16_offset)) {
        asm_thumb_ldrh_rlo_rlo_i5(as, reg_dest, reg_base, uint16_offset);
    } else if (asm_thumb_allow_armv7m(as)) {
        asm_thumb_ldrh_reg_reg_i12(as, reg_dest, reg_base, uint16_offset);
    } else {
        asm_thumb_add_reg_reg_offset(as, reg_dest, reg_base, uint16_offset - 31, 1);
        asm_thumb_ldrh_rlo_rlo_i5(as, reg_dest, reg_dest, 31);
    }
}


#define OP_BW_HI(byte_offset) (0xf000 | (((byte_offset) >> 12) & 0x07ff))
#define OP_BW_LO(byte_offset) (0xb800 | (((byte_offset) >> 1) & 0x07ff))

void asm_thumb_b_label(asm_thumb_t *as, uint label) {
    mp_uint_t dest = get_label_dest(as, label);
    mp_int_t rel = dest - as->base.code_offset;
    rel -= 4; 

    if (dest != (mp_uint_t)-1 && rel <= -4) {
        
        
        if (SIGNED_FIT12(rel)) {
            asm_thumb_op16(as, OP_B_N(rel));
            return;
        }
    }

    

    if (asm_thumb_allow_armv7m(as)) {
        asm_thumb_op32(as, OP_BW_HI(rel), OP_BW_LO(rel));
    } else {
        if (SIGNED_FIT12(rel)) {
            
            asm_thumb_op16(as, OP_B_N(rel));
        } else {
            asm_thumb_op16(as, ASM_THUMB_OP_NOP);
            if (dest != (mp_uint_t)-1) {
                
                mp_raise_NotImplementedError(MP_ERROR_TEXT("native method too big"));
            }
        }
    }
}

void asm_thumb_bcc_label(asm_thumb_t *as, int cond, uint label) {
    mp_uint_t dest = get_label_dest(as, label);
    mp_int_t rel = dest - as->base.code_offset;
    rel -= 4; 

    if (dest != (mp_uint_t)-1 && rel <= -4) {
        
        
        if (SIGNED_FIT9(rel)) {
            asm_thumb_op16(as, OP_BCC_N(cond, rel));
            return;
        }
    }

    

    if (asm_thumb_allow_armv7m(as)) {
        asm_thumb_op32(as, OP_BCC_W_HI(cond, rel), OP_BCC_W_LO(rel));
    } else {
        
        asm_thumb_op16(as, OP_BCC_N(cond ^ 1, 0));
        asm_thumb_b_label(as, label);
    }
}

void asm_thumb_bcc_rel9(asm_thumb_t *as, int cond, int rel) {
    rel -= 4; 
    assert(SIGNED_FIT9(rel));
    asm_thumb_op16(as, OP_BCC_N(cond, rel));
}

void asm_thumb_b_rel12(asm_thumb_t *as, int rel) {
    rel -= 4; 
    assert(SIGNED_FIT12(rel));
    asm_thumb_op16(as, OP_B_N(rel));
}

#define OP_BLX(reg) (0x4780 | ((reg) << 3))
#define OP_SVC(arg) (0xdf00 | (arg))

void asm_thumb_bl_ind(asm_thumb_t *as, uint fun_id, uint reg_temp) {
    
    asm_thumb_ldr_reg_reg_i12_optimised(as, reg_temp, ASM_THUMB_REG_FUN_TABLE, fun_id);
    asm_thumb_op16(as, OP_BLX(reg_temp));
}

#endif 
