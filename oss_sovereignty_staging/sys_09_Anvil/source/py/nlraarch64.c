 

#include "py/mpstate.h" 

#if MICROPY_NLR_AARCH64






__asm(
    #if defined(__APPLE__) && defined(__MACH__)
    "_nlr_push:              \n"
    ".global _nlr_push       \n"
    #else
    "nlr_push:               \n"
    ".global nlr_push        \n"
    #endif
    "mov x9, sp              \n"
    "stp lr,  x9,  [x0,  #16]\n" 
    "stp x19, x20, [x0,  #32]\n"
    "stp x21, x22, [x0,  #48]\n"
    "stp x23, x24, [x0,  #64]\n"
    "stp x25, x26, [x0,  #80]\n"
    "stp x27, x28, [x0,  #96]\n"
    "str x29,      [x0, #112]\n"
    #if defined(__APPLE__) && defined(__MACH__)
    "b _nlr_push_tail        \n" 
    #else
    "b nlr_push_tail         \n" 
    #endif
    );

NORETURN void nlr_jump(void *val) {
    MP_NLR_JUMP_HEAD(val, top)

    MP_STATIC_ASSERT(offsetof(nlr_buf_t, regs) == 16); 

    __asm volatile (
        "mov x0, %0              \n"
        "ldr x29,      [x0, #112]\n"
        "ldp x27, x28, [x0,  #96]\n"
        "ldp x25, x26, [x0,  #80]\n"
        "ldp x23, x24, [x0,  #64]\n"
        "ldp x21, x22, [x0,  #48]\n"
        "ldp x19, x20, [x0,  #32]\n"
        "ldp lr,  x9,  [x0,  #16]\n" 
        "mov sp, x9              \n"
        "mov x0, #1              \n"  
        "ret                     \n"
        :
        : "r" (top)
        : "memory"
        );

    MP_UNREACHABLE
}

#endif 
