
#ifndef MICROPY_INCLUDED_LIB_UTILS_GCHELPER_H
#define MICROPY_INCLUDED_LIB_UTILS_GCHELPER_H

#include <stdint.h>

#if MICROPY_GCREGS_SETJMP
#include <setjmp.h>
typedef jmp_buf gc_helper_regs_t;
#else

#if defined(__x86_64__)
typedef uintptr_t gc_helper_regs_t[6];
#elif defined(__i386__)
typedef uintptr_t gc_helper_regs_t[4];
#elif defined(__thumb2__) || defined(__thumb__) || defined(__arm__)
typedef uintptr_t gc_helper_regs_t[10];
#elif defined(__aarch64__)
typedef uintptr_t gc_helper_regs_t[11]; 
#endif

#endif

void gc_helper_collect_regs_and_stack(void);

#endif 
