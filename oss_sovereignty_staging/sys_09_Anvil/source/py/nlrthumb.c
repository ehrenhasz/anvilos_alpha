 

#include "py/mpstate.h"

#if MICROPY_NLR_THUMB

#undef nlr_push







__attribute__((naked)) unsigned int nlr_push(nlr_buf_t *nlr) {

    
    
    
    
    
    
    
    
    __asm volatile (
        "str    r4, [r0, #12]       \n" 
        "str    r5, [r0, #16]       \n" 
        "str    r6, [r0, #20]       \n" 
        "str    r7, [r0, #24]       \n" 

        #if !defined(__thumb2__)
        "mov    r1, r8              \n"
        "str    r1, [r0, #28]       \n" 
        "mov    r1, r9              \n"
        "str    r1, [r0, #32]       \n" 
        "mov    r1, r10             \n"
        "str    r1, [r0, #36]       \n" 
        "mov    r1, r11             \n"
        "str    r1, [r0, #40]       \n" 
        "mov    r1, r13             \n"
        "str    r1, [r0, #44]       \n" 
        "mov    r1, lr              \n"
        "str    r1, [r0, #8]        \n" 
        #else
        "str    r8, [r0, #28]       \n" 
        "str    r9, [r0, #32]       \n" 
        "str    r10, [r0, #36]      \n" 
        "str    r11, [r0, #40]      \n" 
        "str    r13, [r0, #44]      \n" 
        #if MICROPY_NLR_NUM_REGS == 16
        "vstr   d8, [r0, #48]       \n" 
        "vstr   d9, [r0, #56]       \n" 
        "vstr   d10, [r0, #64]      \n" 
        #endif
        "str    lr, [r0, #8]        \n" 
        #endif

        #if MICROPY_NLR_THUMB_USE_LONG_JUMP
        "ldr    r1, nlr_push_tail_var \n"
        "bx     r1                  \n" 
        ".align 2                   \n"
        "nlr_push_tail_var: .word nlr_push_tail \n"
        #else
        #if defined(__APPLE__) || defined(__MACH__)
        "b      _nlr_push_tail      \n" 
        #else
        "b      nlr_push_tail       \n" 
        #endif
        #endif
        );

    #if !defined(__clang__) && defined(__GNUC__) && (__GNUC__ < 4 || (__GNUC__ == 4 && __GNUC_MINOR__ < 8))
    
    
    return 0;
    #endif
}

NORETURN void nlr_jump(void *val) {
    MP_NLR_JUMP_HEAD(val, top)

    __asm volatile (
        "mov    r0, %0              \n" 
        "ldr    r4, [r0, #12]       \n" 
        "ldr    r5, [r0, #16]       \n" 
        "ldr    r6, [r0, #20]       \n" 
        "ldr    r7, [r0, #24]       \n" 

        #if !defined(__thumb2__)
        "ldr    r1, [r0, #28]       \n" 
        "mov    r8, r1              \n"
        "ldr    r1, [r0, #32]       \n" 
        "mov    r9, r1              \n"
        "ldr    r1, [r0, #36]       \n" 
        "mov    r10, r1             \n"
        "ldr    r1, [r0, #40]       \n" 
        "mov    r11, r1             \n"
        "ldr    r1, [r0, #44]       \n" 
        "mov    r13, r1             \n"
        "ldr    r1, [r0, #8]        \n" 
        "mov    lr, r1              \n"
        #else
        "ldr    r8, [r0, #28]       \n" 
        "ldr    r9, [r0, #32]       \n" 
        "ldr    r10, [r0, #36]      \n" 
        "ldr    r11, [r0, #40]      \n" 
        "ldr    r13, [r0, #44]      \n" 
        #if MICROPY_NLR_NUM_REGS == 16
        "vldr   d8, [r0, #48]       \n" 
        "vldr   d9, [r0, #56]       \n" 
        "vldr   d10, [r0, #64]      \n" 
        #endif
        "ldr    lr, [r0, #8]        \n" 
        #endif
        "movs   r0, #1              \n" 
        "bx     lr                  \n" 
        :                           
        : "r" (top)                 
        : "memory"                  
        );

    MP_UNREACHABLE
}

#endif 
