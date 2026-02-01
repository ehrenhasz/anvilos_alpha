 

#include "py/mpstate.h"

#if MICROPY_NLR_X86

#undef nlr_push




#if MICROPY_NLR_OS_WINDOWS
unsigned int nlr_push_tail(nlr_buf_t *nlr) asm ("nlr_push_tail");
#else
__attribute__((used)) unsigned int nlr_push_tail(nlr_buf_t *nlr);
#endif

#if !defined(__clang__) && defined(__GNUC__) && __GNUC__ >= 8

#define USE_NAKED (1)
#define UNDO_PRELUDE (0)
#elif defined(__ZEPHYR__) || defined(__ANDROID__)

#define USE_NAKED (0)
#define UNDO_PRELUDE (0)
#else
#define USE_NAKED (0)
#define UNDO_PRELUDE (1)
#endif

#if USE_NAKED
__attribute__((naked))
#endif
unsigned int nlr_push(nlr_buf_t *nlr) {
    (void)nlr;

    __asm volatile (
        #if UNDO_PRELUDE
        "pop    %ebp                \n" 
        #endif
        "mov    4(%esp), %edx       \n" 
        "mov    (%esp), %eax        \n" 
        "mov    %eax, 8(%edx)       \n" 
        "mov    %ebp, 12(%edx)      \n" 
        "mov    %esp, 16(%edx)      \n" 
        "mov    %ebx, 20(%edx)      \n" 
        "mov    %edi, 24(%edx)      \n" 
        "mov    %esi, 28(%edx)      \n" 
        "jmp    nlr_push_tail       \n" 
        );

    #if !USE_NAKED
    return 0; 
    #endif
}

NORETURN void nlr_jump(void *val) {
    MP_NLR_JUMP_HEAD(val, top)

    __asm volatile (
        "mov    %0, %%edx           \n" 
        "mov    28(%%edx), %%esi    \n" 
        "mov    24(%%edx), %%edi    \n" 
        "mov    20(%%edx), %%ebx    \n" 
        "mov    16(%%edx), %%esp    \n" 
        "mov    12(%%edx), %%ebp    \n" 
        "mov    8(%%edx), %%eax     \n" 
        "mov    %%eax, (%%esp)      \n" 
        "xor    %%eax, %%eax        \n" 
        "inc    %%al                \n" 
        "ret                        \n" 
        :                           
        : "r" (top)                 
        : "memory"                  
        );

    MP_UNREACHABLE
}

#endif 
