 

#include "py/mpstate.h"

#if MICROPY_NLR_XTENSA

#undef nlr_push







unsigned int nlr_push(nlr_buf_t *nlr) {

    __asm volatile (
        "s32i.n  a0, a2, 8          \n" 
        "s32i.n  a1, a2, 12         \n"
        "s32i.n  a8, a2, 16         \n"
        "s32i.n  a9, a2, 20         \n"
        "s32i.n  a10, a2, 24        \n"
        "s32i.n  a11, a2, 28        \n"
        "s32i.n  a12, a2, 32        \n"
        "s32i.n  a13, a2, 36        \n"
        "s32i.n  a14, a2, 40        \n"
        "s32i.n  a15, a2, 44        \n"
        "j      nlr_push_tail       \n" 
        );

    return 0; 
}

NORETURN void nlr_jump(void *val) {
    MP_NLR_JUMP_HEAD(val, top)

    __asm volatile (
        "mov.n   a2, %0             \n" 
        "l32i.n  a0, a2, 8          \n" 
        "l32i.n  a1, a2, 12         \n"
        "l32i.n  a8, a2, 16         \n"
        "l32i.n  a9, a2, 20         \n"
        "l32i.n  a10, a2, 24        \n"
        "l32i.n  a11, a2, 28        \n"
        "l32i.n  a12, a2, 32        \n"
        "l32i.n  a13, a2, 36        \n"
        "l32i.n  a14, a2, 40        \n"
        "l32i.n  a15, a2, 44        \n"
        "movi.n a2, 1               \n" 
        "ret.n                      \n" 
        :                           
        : "r" (top)                 
        : "memory"                  
        );

    MP_UNREACHABLE
}

#endif 
