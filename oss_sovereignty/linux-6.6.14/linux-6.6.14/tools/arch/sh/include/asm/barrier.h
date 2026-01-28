#ifndef __TOOLS_LINUX_ASM_SH_BARRIER_H
#define __TOOLS_LINUX_ASM_SH_BARRIER_H
#if defined(__SH4A__)
#define mb()		__asm__ __volatile__ ("synco": : :"memory")
#define rmb()		mb()
#define wmb()		mb()
#endif
#include <asm-generic/barrier.h>
#endif  
