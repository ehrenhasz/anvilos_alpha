#ifndef _ASM_BITOPS_H
#define _ASM_BITOPS_H
#ifndef _LINUX_BITOPS_H
#error only <linux/bitops.h> can be included directly
#endif
#include <linux/bits.h>
#include <linux/compiler.h>
#include <linux/types.h>
#include <asm/asm.h>
#include <asm/barrier.h>
#include <asm/byteorder.h>		 
#include <asm/compiler.h>
#include <asm/cpu-features.h>
#include <asm/sgidefs.h>
#define __bit_op(mem, insn, inputs...) do {			\
	unsigned long __temp;					\
								\
	asm volatile(						\
	"	.set		push			\n"	\
	"	.set		" MIPS_ISA_LEVEL "	\n"	\
	"	" __SYNC(full, loongson3_war) "		\n"	\
	"1:	" __stringify(LONG_LL)	"	%0, %1	\n"	\
	"	" insn		"			\n"	\
	"	" __stringify(LONG_SC)	"	%0, %1	\n"	\
	"	" __stringify(SC_BEQZ)	"	%0, 1b	\n"	\
	"	.set		pop			\n"	\
	: "=&r"(__temp), "+" GCC_OFF_SMALL_ASM()(mem)		\
	: inputs						\
	: __LLSC_CLOBBER);					\
} while (0)
#define __test_bit_op(mem, ll_dst, insn, inputs...) ({		\
	unsigned long __orig, __temp;				\
								\
	asm volatile(						\
	"	.set		push			\n"	\
	"	.set		" MIPS_ISA_LEVEL "	\n"	\
	"	" __SYNC(full, loongson3_war) "		\n"	\
	"1:	" __stringify(LONG_LL) " "	ll_dst ", %2\n"	\
	"	" insn		"			\n"	\
	"	" __stringify(LONG_SC)	"	%1, %2	\n"	\
	"	" __stringify(SC_BEQZ)	"	%1, 1b	\n"	\
	"	.set		pop			\n"	\
	: "=&r"(__orig), "=&r"(__temp),				\
	  "+" GCC_OFF_SMALL_ASM()(mem)				\
	: inputs						\
	: __LLSC_CLOBBER);					\
								\
	__orig;							\
})
void __mips_set_bit(unsigned long nr, volatile unsigned long *addr);
void __mips_clear_bit(unsigned long nr, volatile unsigned long *addr);
void __mips_change_bit(unsigned long nr, volatile unsigned long *addr);
int __mips_test_and_set_bit_lock(unsigned long nr,
				 volatile unsigned long *addr);
int __mips_test_and_clear_bit(unsigned long nr,
			      volatile unsigned long *addr);
int __mips_test_and_change_bit(unsigned long nr,
			       volatile unsigned long *addr);
static inline void set_bit(unsigned long nr, volatile unsigned long *addr)
{
	volatile unsigned long *m = &addr[BIT_WORD(nr)];
	int bit = nr % BITS_PER_LONG;
	if (!kernel_uses_llsc) {
		__mips_set_bit(nr, addr);
		return;
	}
	if ((MIPS_ISA_REV >= 2) && __builtin_constant_p(bit) && (bit >= 16)) {
		__bit_op(*m, __stringify(LONG_INS) " %0, %3, %2, 1", "i"(bit), "r"(~0));
		return;
	}
	__bit_op(*m, "or\t%0, %2", "ir"(BIT(bit)));
}
static inline void clear_bit(unsigned long nr, volatile unsigned long *addr)
{
	volatile unsigned long *m = &addr[BIT_WORD(nr)];
	int bit = nr % BITS_PER_LONG;
	if (!kernel_uses_llsc) {
		__mips_clear_bit(nr, addr);
		return;
	}
	if ((MIPS_ISA_REV >= 2) && __builtin_constant_p(bit)) {
		__bit_op(*m, __stringify(LONG_INS) " %0, $0, %2, 1", "i"(bit));
		return;
	}
	__bit_op(*m, "and\t%0, %2", "ir"(~BIT(bit)));
}
static inline void clear_bit_unlock(unsigned long nr, volatile unsigned long *addr)
{
	smp_mb__before_atomic();
	clear_bit(nr, addr);
}
static inline void change_bit(unsigned long nr, volatile unsigned long *addr)
{
	volatile unsigned long *m = &addr[BIT_WORD(nr)];
	int bit = nr % BITS_PER_LONG;
	if (!kernel_uses_llsc) {
		__mips_change_bit(nr, addr);
		return;
	}
	__bit_op(*m, "xor\t%0, %2", "ir"(BIT(bit)));
}
static inline int test_and_set_bit_lock(unsigned long nr,
	volatile unsigned long *addr)
{
	volatile unsigned long *m = &addr[BIT_WORD(nr)];
	int bit = nr % BITS_PER_LONG;
	unsigned long res, orig;
	if (!kernel_uses_llsc) {
		res = __mips_test_and_set_bit_lock(nr, addr);
	} else {
		orig = __test_bit_op(*m, "%0",
				     "or\t%1, %0, %3",
				     "ir"(BIT(bit)));
		res = (orig & BIT(bit)) != 0;
	}
	smp_llsc_mb();
	return res;
}
static inline int test_and_set_bit(unsigned long nr,
	volatile unsigned long *addr)
{
	smp_mb__before_atomic();
	return test_and_set_bit_lock(nr, addr);
}
static inline int test_and_clear_bit(unsigned long nr,
	volatile unsigned long *addr)
{
	volatile unsigned long *m = &addr[BIT_WORD(nr)];
	int bit = nr % BITS_PER_LONG;
	unsigned long res, orig;
	smp_mb__before_atomic();
	if (!kernel_uses_llsc) {
		res = __mips_test_and_clear_bit(nr, addr);
	} else if ((MIPS_ISA_REV >= 2) && __builtin_constant_p(nr)) {
		res = __test_bit_op(*m, "%1",
				    __stringify(LONG_EXT) " %0, %1, %3, 1;"
				    __stringify(LONG_INS) " %1, $0, %3, 1",
				    "i"(bit));
	} else {
		orig = __test_bit_op(*m, "%0",
				     "or\t%1, %0, %3;"
				     "xor\t%1, %1, %3",
				     "ir"(BIT(bit)));
		res = (orig & BIT(bit)) != 0;
	}
	smp_llsc_mb();
	return res;
}
static inline int test_and_change_bit(unsigned long nr,
	volatile unsigned long *addr)
{
	volatile unsigned long *m = &addr[BIT_WORD(nr)];
	int bit = nr % BITS_PER_LONG;
	unsigned long res, orig;
	smp_mb__before_atomic();
	if (!kernel_uses_llsc) {
		res = __mips_test_and_change_bit(nr, addr);
	} else {
		orig = __test_bit_op(*m, "%0",
				     "xor\t%1, %0, %3",
				     "ir"(BIT(bit)));
		res = (orig & BIT(bit)) != 0;
	}
	smp_llsc_mb();
	return res;
}
#undef __bit_op
#undef __test_bit_op
#include <asm-generic/bitops/non-atomic.h>
static inline void __clear_bit_unlock(unsigned long nr, volatile unsigned long *addr)
{
	smp_mb__before_llsc();
	__clear_bit(nr, addr);
	nudge_writes();
}
static __always_inline unsigned long __fls(unsigned long word)
{
	int num;
	if (BITS_PER_LONG == 32 && !__builtin_constant_p(word) &&
	    __builtin_constant_p(cpu_has_clo_clz) && cpu_has_clo_clz) {
		__asm__(
		"	.set	push					\n"
		"	.set	"MIPS_ISA_LEVEL"			\n"
		"	clz	%0, %1					\n"
		"	.set	pop					\n"
		: "=r" (num)
		: "r" (word));
		return 31 - num;
	}
	if (BITS_PER_LONG == 64 && !__builtin_constant_p(word) &&
	    __builtin_constant_p(cpu_has_mips64) && cpu_has_mips64) {
		__asm__(
		"	.set	push					\n"
		"	.set	"MIPS_ISA_LEVEL"			\n"
		"	dclz	%0, %1					\n"
		"	.set	pop					\n"
		: "=r" (num)
		: "r" (word));
		return 63 - num;
	}
	num = BITS_PER_LONG - 1;
#if BITS_PER_LONG == 64
	if (!(word & (~0ul << 32))) {
		num -= 32;
		word <<= 32;
	}
#endif
	if (!(word & (~0ul << (BITS_PER_LONG-16)))) {
		num -= 16;
		word <<= 16;
	}
	if (!(word & (~0ul << (BITS_PER_LONG-8)))) {
		num -= 8;
		word <<= 8;
	}
	if (!(word & (~0ul << (BITS_PER_LONG-4)))) {
		num -= 4;
		word <<= 4;
	}
	if (!(word & (~0ul << (BITS_PER_LONG-2)))) {
		num -= 2;
		word <<= 2;
	}
	if (!(word & (~0ul << (BITS_PER_LONG-1))))
		num -= 1;
	return num;
}
static __always_inline unsigned long __ffs(unsigned long word)
{
	return __fls(word & -word);
}
static inline int fls(unsigned int x)
{
	int r;
	if (!__builtin_constant_p(x) &&
	    __builtin_constant_p(cpu_has_clo_clz) && cpu_has_clo_clz) {
		__asm__(
		"	.set	push					\n"
		"	.set	"MIPS_ISA_LEVEL"			\n"
		"	clz	%0, %1					\n"
		"	.set	pop					\n"
		: "=r" (x)
		: "r" (x));
		return 32 - x;
	}
	r = 32;
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
	if (!(x & 0x80000000u)) {
		x <<= 1;
		r -= 1;
	}
	return r;
}
#include <asm-generic/bitops/fls64.h>
static inline int ffs(int word)
{
	if (!word)
		return 0;
	return fls(word & -word);
}
#include <asm-generic/bitops/ffz.h>
#ifdef __KERNEL__
#include <asm-generic/bitops/sched.h>
#include <asm/arch_hweight.h>
#include <asm-generic/bitops/const_hweight.h>
#include <asm-generic/bitops/le.h>
#include <asm-generic/bitops/ext2-atomic.h>
#endif  
#endif  
