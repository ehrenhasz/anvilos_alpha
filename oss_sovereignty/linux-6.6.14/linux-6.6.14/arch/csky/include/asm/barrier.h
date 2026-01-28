#ifndef __ASM_CSKY_BARRIER_H
#define __ASM_CSKY_BARRIER_H
#ifndef __ASSEMBLY__
#define nop()	asm volatile ("nop\n":::"memory")
#ifdef CONFIG_SMP
#define FULL_FENCE		".long 0x842fc000\n"
#define ACQUIRE_FENCE		".long 0x8427c000\n"
#define RELEASE_FENCE		".long 0x842ec000\n"
#define __bar_brw()	asm volatile (".long 0x842cc000\n":::"memory")
#define __bar_br()	asm volatile (".long 0x8424c000\n":::"memory")
#define __bar_bw()	asm volatile (".long 0x8428c000\n":::"memory")
#define __bar_arw()	asm volatile (".long 0x8423c000\n":::"memory")
#define __bar_ar()	asm volatile (".long 0x8421c000\n":::"memory")
#define __bar_aw()	asm volatile (".long 0x8422c000\n":::"memory")
#define __bar_brwarw()	asm volatile (FULL_FENCE:::"memory")
#define __bar_brarw()	asm volatile (ACQUIRE_FENCE:::"memory")
#define __bar_bwarw()	asm volatile (".long 0x842bc000\n":::"memory")
#define __bar_brwar()	asm volatile (".long 0x842dc000\n":::"memory")
#define __bar_brwaw()	asm volatile (RELEASE_FENCE:::"memory")
#define __bar_brar()	asm volatile (".long 0x8425c000\n":::"memory")
#define __bar_brar()	asm volatile (".long 0x8425c000\n":::"memory")
#define __bar_bwaw()	asm volatile (".long 0x842ac000\n":::"memory")
#define __smp_mb()	__bar_brwarw()
#define __smp_rmb()	__bar_brar()
#define __smp_wmb()	__bar_bwaw()
#define __smp_acquire_fence()	__bar_brarw()
#define __smp_release_fence()	__bar_brwaw()
#endif  
#define mb()		asm volatile ("sync\n":::"memory")
#ifdef CONFIG_CPU_HAS_CACHEV2
#define sync_is()	asm volatile ("sync.is\nsync.is\nsync.is\n":::"memory")
#endif
#include <asm-generic/barrier.h>
#endif  
#endif  
