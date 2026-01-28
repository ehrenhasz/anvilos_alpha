#ifndef _ASM_POWERPC_BARRIER_H
#define _ASM_POWERPC_BARRIER_H
#include <asm/asm-const.h>
#ifndef __ASSEMBLY__
#include <asm/ppc-opcode.h>
#endif
#define __mb()   __asm__ __volatile__ ("sync" : : : "memory")
#define __rmb()  __asm__ __volatile__ ("sync" : : : "memory")
#define __wmb()  __asm__ __volatile__ ("sync" : : : "memory")
#if defined(CONFIG_PPC64) || defined(CONFIG_PPC_E500MC)
#    define SMPWMB      LWSYNC
#elif defined(CONFIG_BOOKE)
#    define SMPWMB      mbar
#else
#    define SMPWMB      eieio
#endif
#undef __lwsync
#define __lwsync()	__asm__ __volatile__ (stringify_in_c(LWSYNC) : : :"memory")
#define __dma_rmb()	__lwsync()
#define __dma_wmb()	__asm__ __volatile__ (stringify_in_c(SMPWMB) : : :"memory")
#define __smp_lwsync()	__lwsync()
#define __smp_mb()	__mb()
#define __smp_rmb()	__lwsync()
#define __smp_wmb()	__asm__ __volatile__ (stringify_in_c(SMPWMB) : : :"memory")
#define data_barrier(x)	\
	asm volatile("twi 0,%0,0; isync" : : "r" (x) : "memory");
#define __smp_store_release(p, v)						\
do {									\
	compiletime_assert_atomic_type(*p);				\
	__smp_lwsync();							\
	WRITE_ONCE(*p, v);						\
} while (0)
#define __smp_load_acquire(p)						\
({									\
	typeof(*p) ___p1 = READ_ONCE(*p);				\
	compiletime_assert_atomic_type(*p);				\
	__smp_lwsync();							\
	___p1;								\
})
#ifdef CONFIG_PPC_BOOK3S_64
#define NOSPEC_BARRIER_SLOT   nop
#elif defined(CONFIG_PPC_E500)
#define NOSPEC_BARRIER_SLOT   nop; nop
#endif
#ifdef CONFIG_PPC_BARRIER_NOSPEC
#define barrier_nospec_asm NOSPEC_BARRIER_FIXUP_SECTION; NOSPEC_BARRIER_SLOT
#define barrier_nospec() asm (stringify_in_c(barrier_nospec_asm) ::: "memory")
#else  
#define barrier_nospec_asm
#define barrier_nospec()
#endif  
#define pmem_wmb() __asm__ __volatile__(PPC_PHWSYNC ::: "memory")
#include <asm-generic/barrier.h>
#endif  
