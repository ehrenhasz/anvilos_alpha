#ifndef _S390_BITOPS_H
#define _S390_BITOPS_H
#ifndef _LINUX_BITOPS_H
#error only <linux/bitops.h> can be included directly
#endif
#include <linux/typecheck.h>
#include <linux/compiler.h>
#include <linux/types.h>
#include <asm/atomic_ops.h>
#include <asm/barrier.h>
#define __BITOPS_WORDS(bits) (((bits) + BITS_PER_LONG - 1) / BITS_PER_LONG)
static inline unsigned long *
__bitops_word(unsigned long nr, const volatile unsigned long *ptr)
{
	unsigned long addr;
	addr = (unsigned long)ptr + ((nr ^ (nr & (BITS_PER_LONG - 1))) >> 3);
	return (unsigned long *)addr;
}
static inline unsigned long __bitops_mask(unsigned long nr)
{
	return 1UL << (nr & (BITS_PER_LONG - 1));
}
static __always_inline void arch_set_bit(unsigned long nr, volatile unsigned long *ptr)
{
	unsigned long *addr = __bitops_word(nr, ptr);
	unsigned long mask = __bitops_mask(nr);
	__atomic64_or(mask, (long *)addr);
}
static __always_inline void arch_clear_bit(unsigned long nr, volatile unsigned long *ptr)
{
	unsigned long *addr = __bitops_word(nr, ptr);
	unsigned long mask = __bitops_mask(nr);
	__atomic64_and(~mask, (long *)addr);
}
static __always_inline void arch_change_bit(unsigned long nr,
					    volatile unsigned long *ptr)
{
	unsigned long *addr = __bitops_word(nr, ptr);
	unsigned long mask = __bitops_mask(nr);
	__atomic64_xor(mask, (long *)addr);
}
static inline bool arch_test_and_set_bit(unsigned long nr,
					 volatile unsigned long *ptr)
{
	unsigned long *addr = __bitops_word(nr, ptr);
	unsigned long mask = __bitops_mask(nr);
	unsigned long old;
	old = __atomic64_or_barrier(mask, (long *)addr);
	return old & mask;
}
static inline bool arch_test_and_clear_bit(unsigned long nr,
					   volatile unsigned long *ptr)
{
	unsigned long *addr = __bitops_word(nr, ptr);
	unsigned long mask = __bitops_mask(nr);
	unsigned long old;
	old = __atomic64_and_barrier(~mask, (long *)addr);
	return old & mask;
}
static inline bool arch_test_and_change_bit(unsigned long nr,
					    volatile unsigned long *ptr)
{
	unsigned long *addr = __bitops_word(nr, ptr);
	unsigned long mask = __bitops_mask(nr);
	unsigned long old;
	old = __atomic64_xor_barrier(mask, (long *)addr);
	return old & mask;
}
static __always_inline void
arch___set_bit(unsigned long nr, volatile unsigned long *addr)
{
	unsigned long *p = __bitops_word(nr, addr);
	unsigned long mask = __bitops_mask(nr);
	*p |= mask;
}
static __always_inline void
arch___clear_bit(unsigned long nr, volatile unsigned long *addr)
{
	unsigned long *p = __bitops_word(nr, addr);
	unsigned long mask = __bitops_mask(nr);
	*p &= ~mask;
}
static __always_inline void
arch___change_bit(unsigned long nr, volatile unsigned long *addr)
{
	unsigned long *p = __bitops_word(nr, addr);
	unsigned long mask = __bitops_mask(nr);
	*p ^= mask;
}
static __always_inline bool
arch___test_and_set_bit(unsigned long nr, volatile unsigned long *addr)
{
	unsigned long *p = __bitops_word(nr, addr);
	unsigned long mask = __bitops_mask(nr);
	unsigned long old;
	old = *p;
	*p |= mask;
	return old & mask;
}
static __always_inline bool
arch___test_and_clear_bit(unsigned long nr, volatile unsigned long *addr)
{
	unsigned long *p = __bitops_word(nr, addr);
	unsigned long mask = __bitops_mask(nr);
	unsigned long old;
	old = *p;
	*p &= ~mask;
	return old & mask;
}
static __always_inline bool
arch___test_and_change_bit(unsigned long nr, volatile unsigned long *addr)
{
	unsigned long *p = __bitops_word(nr, addr);
	unsigned long mask = __bitops_mask(nr);
	unsigned long old;
	old = *p;
	*p ^= mask;
	return old & mask;
}
#define arch_test_bit generic_test_bit
#define arch_test_bit_acquire generic_test_bit_acquire
static inline bool arch_test_and_set_bit_lock(unsigned long nr,
					      volatile unsigned long *ptr)
{
	if (arch_test_bit(nr, ptr))
		return true;
	return arch_test_and_set_bit(nr, ptr);
}
static inline void arch_clear_bit_unlock(unsigned long nr,
					 volatile unsigned long *ptr)
{
	smp_mb__before_atomic();
	arch_clear_bit(nr, ptr);
}
static inline void arch___clear_bit_unlock(unsigned long nr,
					   volatile unsigned long *ptr)
{
	smp_mb();
	arch___clear_bit(nr, ptr);
}
#include <asm-generic/bitops/instrumented-atomic.h>
#include <asm-generic/bitops/instrumented-non-atomic.h>
#include <asm-generic/bitops/instrumented-lock.h>
unsigned long find_first_bit_inv(const unsigned long *addr, unsigned long size);
unsigned long find_next_bit_inv(const unsigned long *addr, unsigned long size,
				unsigned long offset);
#define for_each_set_bit_inv(bit, addr, size)				\
	for ((bit) = find_first_bit_inv((addr), (size));		\
	     (bit) < (size);						\
	     (bit) = find_next_bit_inv((addr), (size), (bit) + 1))
static inline void set_bit_inv(unsigned long nr, volatile unsigned long *ptr)
{
	return set_bit(nr ^ (BITS_PER_LONG - 1), ptr);
}
static inline void clear_bit_inv(unsigned long nr, volatile unsigned long *ptr)
{
	return clear_bit(nr ^ (BITS_PER_LONG - 1), ptr);
}
static inline bool test_and_clear_bit_inv(unsigned long nr,
					  volatile unsigned long *ptr)
{
	return test_and_clear_bit(nr ^ (BITS_PER_LONG - 1), ptr);
}
static inline void __set_bit_inv(unsigned long nr, volatile unsigned long *ptr)
{
	return __set_bit(nr ^ (BITS_PER_LONG - 1), ptr);
}
static inline void __clear_bit_inv(unsigned long nr, volatile unsigned long *ptr)
{
	return __clear_bit(nr ^ (BITS_PER_LONG - 1), ptr);
}
static inline bool test_bit_inv(unsigned long nr,
				const volatile unsigned long *ptr)
{
	return test_bit(nr ^ (BITS_PER_LONG - 1), ptr);
}
static inline unsigned char __flogr(unsigned long word)
{
	if (__builtin_constant_p(word)) {
		unsigned long bit = 0;
		if (!word)
			return 64;
		if (!(word & 0xffffffff00000000UL)) {
			word <<= 32;
			bit += 32;
		}
		if (!(word & 0xffff000000000000UL)) {
			word <<= 16;
			bit += 16;
		}
		if (!(word & 0xff00000000000000UL)) {
			word <<= 8;
			bit += 8;
		}
		if (!(word & 0xf000000000000000UL)) {
			word <<= 4;
			bit += 4;
		}
		if (!(word & 0xc000000000000000UL)) {
			word <<= 2;
			bit += 2;
		}
		if (!(word & 0x8000000000000000UL)) {
			word <<= 1;
			bit += 1;
		}
		return bit;
	} else {
		union register_pair rp;
		rp.even = word;
		asm volatile(
			"       flogr   %[rp],%[rp]\n"
			: [rp] "+d" (rp.pair) : : "cc");
		return rp.even;
	}
}
static inline unsigned long __ffs(unsigned long word)
{
	return __flogr(-word & word) ^ (BITS_PER_LONG - 1);
}
static inline int ffs(int word)
{
	unsigned long mask = 2 * BITS_PER_LONG - 1;
	unsigned int val = (unsigned int)word;
	return (1 + (__flogr(-val & val) ^ (BITS_PER_LONG - 1))) & mask;
}
static inline unsigned long __fls(unsigned long word)
{
	return __flogr(word) ^ (BITS_PER_LONG - 1);
}
static inline int fls64(unsigned long word)
{
	unsigned long mask = 2 * BITS_PER_LONG - 1;
	return (1 + (__flogr(word) ^ (BITS_PER_LONG - 1))) & mask;
}
static inline int fls(unsigned int word)
{
	return fls64(word);
}
#include <asm-generic/bitops/ffz.h>
#include <asm-generic/bitops/hweight.h>
#include <asm-generic/bitops/sched.h>
#include <asm-generic/bitops/le.h>
#include <asm-generic/bitops/ext2-atomic-setbit.h>
#endif  
