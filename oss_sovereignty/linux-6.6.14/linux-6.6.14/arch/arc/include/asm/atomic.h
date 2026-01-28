#ifndef _ASM_ARC_ATOMIC_H
#define _ASM_ARC_ATOMIC_H
#ifndef __ASSEMBLY__
#include <linux/types.h>
#include <linux/compiler.h>
#include <asm/cmpxchg.h>
#include <asm/barrier.h>
#include <asm/smp.h>
#define arch_atomic_read(v)  READ_ONCE((v)->counter)
#ifdef CONFIG_ARC_HAS_LLSC
#include <asm/atomic-llsc.h>
#else
#include <asm/atomic-spinlock.h>
#endif
#ifdef CONFIG_GENERIC_ATOMIC64
#include <asm-generic/atomic64.h>
#else
#include <asm/atomic64-arcv2.h>
#endif
#endif	 
#endif
