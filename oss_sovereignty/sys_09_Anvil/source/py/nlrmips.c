 

#include "py/mpstate.h"

#if MICROPY_NLR_MIPS

__attribute__((used)) unsigned int nlr_push_tail(nlr_buf_t *nlr);

__asm(
    ".globl  nlr_push         \n"
    "nlr_push:                \n"
    ".ent    nlr_push         \n"
    ".frame  $29, 0, $31      \n"
    ".set    noreorder        \n"
    ".cpload $25              \n"
    ".set    reorder          \n"
    "sw $31, 8($4)            \n"     
    "sw $30, 12($4)           \n"
    "sw $29, 16($4)           \n"
    "sw $28, 20($4)           \n"
    "sw $23, 24($4)           \n"
    "sw $22, 28($4)           \n"
    "sw $21, 32($4)           \n"
    "sw $20, 36($4)           \n"
    "sw $19, 40($4)           \n"
    "sw $18, 44($4)           \n"
    "sw $17, 48($4)           \n"
    "sw $16, 52($4)           \n"
#ifdef __pic__
    "la $25, nlr_push_tail    \n"
#endif
    "j nlr_push_tail          \n"
    ".end nlr_push            \n"
    );

NORETURN void nlr_jump(void *val) {
    MP_NLR_JUMP_HEAD(val, top)
    __asm(
        "move $4, %0    \n"
        "lw $31, 8($4)  \n"
        "lw $30, 12($4) \n"
        "lw $29, 16($4) \n"
        "lw $28, 20($4) \n"
        "lw $23, 24($4) \n"
        "lw $22, 28($4) \n"
        "lw $21, 32($4) \n"
        "lw $20, 36($4) \n"
        "lw $19, 40($4) \n"
        "lw $18, 44($4) \n"
        "lw $17, 48($4) \n"
        "lw $16, 52($4) \n"
        "lui $2,1       \n"   
        "j $31          \n"
        "nop            \n"
        :
        : "r" (top)
        : "memory"
        );
    MP_UNREACHABLE
}

#endif 
