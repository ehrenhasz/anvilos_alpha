#ifndef _ASM_RISCV_BITOPS_H
#define _ASM_RISCV_BITOPS_H
#ifndef _LINUX_BITOPS_H
#error "Only <linux/bitops.h> can be included directly"
#endif  
#include <linux/compiler.h>
#include <linux/irqflags.h>
#include <asm/barrier.h>
#include <asm/bitsperlong.h>
#include <asm-generic/bitops/__ffs.h>
#include <asm-generic/bitops/ffz.h>
#include <asm-generic/bitops/fls.h>
#include <asm-generic/bitops/__fls.h>
#include <asm-generic/bitops/fls64.h>
#include <asm-generic/bitops/sched.h>
#include <asm-generic/bitops/ffs.h>
#include <asm-generic/bitops/hweight.h>
#if (BITS_PER_LONG == 64)
#define __AMO(op)	"amo" #op ".d"
#elif (BITS_PER_LONG == 32)
#define __AMO(op)	"amo" #op ".w"
#else
#error "Unexpected BITS_PER_LONG"
#endif
#define __test_and_op_bit_ord(op, mod, nr, addr, ord)		\
({								\
	unsigned long __res, __mask;				\
	__mask = BIT_MASK(nr);					\
	__asm__ __volatile__ (					\
		__AMO(op) #ord " %0, %2, %1"			\
		: "=r" (__res), "+A" (addr[BIT_WORD(nr)])	\
		: "r" (mod(__mask))				\
		: "memory");					\
	((__res & __mask) != 0);				\
})
#define __op_bit_ord(op, mod, nr, addr, ord)			\
	__asm__ __volatile__ (					\
		__AMO(op) #ord " zero, %1, %0"			\
		: "+A" (addr[BIT_WORD(nr)])			\
		: "r" (mod(BIT_MASK(nr)))			\
		: "memory");
#define __test_and_op_bit(op, mod, nr, addr) 			\
	__test_and_op_bit_ord(op, mod, nr, addr, .aqrl)
#define __op_bit(op, mod, nr, addr)				\
	__op_bit_ord(op, mod, nr, addr, )
#define __NOP(x)	(x)
#define __NOT(x)	(~(x))
static inline int test_and_set_bit(int nr, volatile unsigned long *addr)
{
	return __test_and_op_bit(or, __NOP, nr, addr);
}
static inline int test_and_clear_bit(int nr, volatile unsigned long *addr)
{
	return __test_and_op_bit(and, __NOT, nr, addr);
}
static inline int test_and_change_bit(int nr, volatile unsigned long *addr)
{
	return __test_and_op_bit(xor, __NOP, nr, addr);
}
static inline void set_bit(int nr, volatile unsigned long *addr)
{
	__op_bit(or, __NOP, nr, addr);
}
static inline void clear_bit(int nr, volatile unsigned long *addr)
{
	__op_bit(and, __NOT, nr, addr);
}
static inline void change_bit(int nr, volatile unsigned long *addr)
{
	__op_bit(xor, __NOP, nr, addr);
}
static inline int test_and_set_bit_lock(
	unsigned long nr, volatile unsigned long *addr)
{
	return __test_and_op_bit_ord(or, __NOP, nr, addr, .aq);
}
static inline void clear_bit_unlock(
	unsigned long nr, volatile unsigned long *addr)
{
	__op_bit_ord(and, __NOT, nr, addr, .rl);
}
static inline void __clear_bit_unlock(
	unsigned long nr, volatile unsigned long *addr)
{
	clear_bit_unlock(nr, addr);
}
#undef __test_and_op_bit
#undef __op_bit
#undef __NOP
#undef __NOT
#undef __AMO
#include <asm-generic/bitops/non-atomic.h>
#include <asm-generic/bitops/le.h>
#include <asm-generic/bitops/ext2-atomic.h>
#endif  
