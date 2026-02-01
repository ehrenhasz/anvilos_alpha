 

#include "py/mpstate.h"

#if MICROPY_NLR_X64

#undef nlr_push




__attribute__((used)) unsigned int nlr_push_tail(nlr_buf_t *nlr);

#if !MICROPY_NLR_OS_WINDOWS
#if defined(__clang__) || (defined(__GNUC__) && __GNUC__ >= 8)
#define USE_NAKED 1
#else

__attribute__((optimize("omit-frame-pointer")))
#endif
#endif

#if !defined(USE_NAKED)
#define USE_NAKED 0
#endif

#if USE_NAKED

__attribute__((naked))
#endif
unsigned int nlr_push(nlr_buf_t *nlr) {
    #if !USE_NAKED
    (void)nlr;
    #endif

    #if MICROPY_NLR_OS_WINDOWS

    __asm volatile (
        "movq   (%rsp), %rax        \n" 
        "movq   %rax, 16(%rcx)      \n" 
        "movq   %rbp, 24(%rcx)      \n" 
        "movq   %rsp, 32(%rcx)      \n" 
        "movq   %rbx, 40(%rcx)      \n" 
        "movq   %r12, 48(%rcx)      \n" 
        "movq   %r13, 56(%rcx)      \n" 
        "movq   %r14, 64(%rcx)      \n" 
        "movq   %r15, 72(%rcx)      \n" 
        "movq   %rdi, 80(%rcx)      \n" 
        "movq   %rsi, 88(%rcx)      \n" 
        "jmp    nlr_push_tail       \n" 
        );

    #else

    __asm volatile (
        "movq   (%rsp), %rax        \n" 
        "movq   %rax, 16(%rdi)      \n" 
        "movq   %rbp, 24(%rdi)      \n" 
        "movq   %rsp, 32(%rdi)      \n" 
        "movq   %rbx, 40(%rdi)      \n" 
        "movq   %r12, 48(%rdi)      \n" 
        "movq   %r13, 56(%rdi)      \n" 
        "movq   %r14, 64(%rdi)      \n" 
        "movq   %r15, 72(%rdi)      \n" 
        #if defined(__APPLE__) && defined(__MACH__)
        "jmp    _nlr_push_tail      \n" 
        #else
        "jmp    nlr_push_tail       \n" 
        #endif
        );

    #endif

    #if !USE_NAKED
    return 0; 
    #endif
}

NORETURN void nlr_jump(void *val) {
    MP_NLR_JUMP_HEAD(val, top)

    __asm volatile (
        "movq   %0, %%rcx           \n" 
        #if MICROPY_NLR_OS_WINDOWS
        "movq   88(%%rcx), %%rsi    \n" 
        "movq   80(%%rcx), %%rdi    \n" 
        #endif
        "movq   72(%%rcx), %%r15    \n" 
        "movq   64(%%rcx), %%r14    \n" 
        "movq   56(%%rcx), %%r13    \n" 
        "movq   48(%%rcx), %%r12    \n" 
        "movq   40(%%rcx), %%rbx    \n" 
        "movq   32(%%rcx), %%rsp    \n" 
        "movq   24(%%rcx), %%rbp    \n" 
        "movq   16(%%rcx), %%rax    \n" 
        "movq   %%rax, (%%rsp)      \n" 
        "xorq   %%rax, %%rax        \n" 
        "inc    %%al                \n" 
        "ret                        \n" 
        :                           
        : "r" (top)                 
        : "memory"                  
        );

    MP_UNREACHABLE
}

#endif 
