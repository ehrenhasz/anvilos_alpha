#ifndef __ASM_BARRIER_H
#define __ASM_BARRIER_H
#ifdef CONFIG_ISA_ARCV2
#define mb()	asm volatile("dmb 3\n" : : : "memory")
#define rmb()	asm volatile("dmb 1\n" : : : "memory")
#define wmb()	asm volatile("dmb 2\n" : : : "memory")
#else
#define mb()	asm volatile("sync\n" : : : "memory")
#endif
#include <asm-generic/barrier.h>
#endif
