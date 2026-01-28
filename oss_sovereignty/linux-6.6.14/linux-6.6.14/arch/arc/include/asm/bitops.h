#ifndef _ASM_BITOPS_H
#define _ASM_BITOPS_H
#ifndef _LINUX_BITOPS_H
#error only <linux/bitops.h> can be included directly
#endif
#ifndef __ASSEMBLY__
#include <linux/types.h>
#include <linux/compiler.h>
#ifdef CONFIG_ISA_ARCOMPACT
static inline __attribute__ ((const)) int clz(unsigned int x)
{
	unsigned int res;
	__asm__ __volatile__(
	"	norm.f  %0, %1		\n"
	"	mov.n   %0, 0		\n"
	"	add.p   %0, %0, 1	\n"
	: "=r"(res)
	: "r"(x)
	: "cc");
	return res;
}
static inline int constant_fls(unsigned int x)
{
	int r = 32;
	if (!x)
		return 0;
	if (!(x & 0xffff0000u)) {
		x <<= 16;
		r -= 16;
	}
	if (!(x & 0xff000000u)) {
		x <<= 8;
		r -= 8;
	}
	if (!(x & 0xf0000000u)) {
		x <<= 4;
		r -= 4;
	}
	if (!(x & 0xc0000000u)) {
		x <<= 2;
		r -= 2;
	}
	if (!(x & 0x80000000u))
		r -= 1;
	return r;
}
static inline __attribute__ ((const)) int fls(unsigned int x)
{
	if (__builtin_constant_p(x))
	       return constant_fls(x);
	return 32 - clz(x);
}
static inline __attribute__ ((const)) unsigned long __fls(unsigned long x)
{
	if (!x)
		return 0;
	else
		return fls(x) - 1;
}
#define ffs(x)	({ unsigned long __t = (x); fls(__t & -__t); })
static inline __attribute__ ((const)) unsigned long __ffs(unsigned long word)
{
	if (!word)
		return word;
	return ffs(word) - 1;
}
#else	 
static inline __attribute__ ((const)) int fls(unsigned int x)
{
	int n;
	asm volatile(
	"	fls.f	%0, %1		\n"   
	"	add.nz	%0, %0, 1	\n"   
	: "=r"(n)	 
	: "r"(x)
	: "cc");
	return n;
}
static inline __attribute__ ((const)) unsigned long __fls(unsigned long x)
{
	return	__builtin_arc_fls(x);
}
static inline __attribute__ ((const)) int ffs(unsigned int x)
{
	int n;
	asm volatile(
	"	ffs.f	%0, %1		\n"   
	"	add.nz	%0, %0, 1	\n"   
	"	mov.z	%0, 0		\n"   
	: "=r"(n)	 
	: "r"(x)
	: "cc");
	return n;
}
static inline __attribute__ ((const)) unsigned long __ffs(unsigned long x)
{
	unsigned long n;
	asm volatile(
	"	ffs.f	%0, %1		\n"   
	"	mov.z	%0, 0		\n"   
	: "=r"(n)
	: "r"(x)
	: "cc");
	return n;
}
#endif	 
#define ffz(x)	__ffs(~(x))
#include <asm-generic/bitops/hweight.h>
#include <asm-generic/bitops/fls64.h>
#include <asm-generic/bitops/sched.h>
#include <asm-generic/bitops/lock.h>
#include <asm-generic/bitops/atomic.h>
#include <asm-generic/bitops/non-atomic.h>
#include <asm-generic/bitops/le.h>
#include <asm-generic/bitops/ext2-atomic-setbit.h>
#endif  
#endif
