#ifndef __ASM_SH_BARRIER_H
#define __ASM_SH_BARRIER_H
#if defined(CONFIG_CPU_SH4A)
#include <asm/cache_insns.h>
#endif
#if defined(CONFIG_CPU_SH4A)
#define mb()		__asm__ __volatile__ ("synco": : :"memory")
#define rmb()		mb()
#define wmb()		mb()
#define ctrl_barrier()	__icbi(PAGE_OFFSET)
#else
#if defined(CONFIG_CPU_J2) && defined(CONFIG_SMP)
#define __smp_mb()	do { int tmp = 0; __asm__ __volatile__ ("cas.l %0,%0,@%1" : "+r"(tmp) : "z"(&tmp) : "memory", "t"); } while(0)
#define __smp_rmb()	__smp_mb()
#define __smp_wmb()	__smp_mb()
#endif
#define ctrl_barrier()	__asm__ __volatile__ ("nop;nop;nop;nop;nop;nop;nop;nop")
#endif
#define __smp_store_mb(var, value) do { (void)xchg(&var, value); } while (0)
#include <asm-generic/barrier.h>
#endif  
