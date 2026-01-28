#ifndef _PARISC_BITOPS_H
#define _PARISC_BITOPS_H
#ifndef _LINUX_BITOPS_H
#error only <linux/bitops.h> can be included directly
#endif
#include <linux/compiler.h>
#include <asm/types.h>
#include <asm/byteorder.h>
#include <asm/barrier.h>
#include <linux/atomic.h>
static __inline__ void set_bit(int nr, volatile unsigned long * addr)
{
	unsigned long mask = BIT_MASK(nr);
	unsigned long flags;
	addr += BIT_WORD(nr);
	_atomic_spin_lock_irqsave(addr, flags);
	*addr |= mask;
	_atomic_spin_unlock_irqrestore(addr, flags);
}
static __inline__ void clear_bit(int nr, volatile unsigned long * addr)
{
	unsigned long mask = BIT_MASK(nr);
	unsigned long flags;
	addr += BIT_WORD(nr);
	_atomic_spin_lock_irqsave(addr, flags);
	*addr &= ~mask;
	_atomic_spin_unlock_irqrestore(addr, flags);
}
static __inline__ void change_bit(int nr, volatile unsigned long * addr)
{
	unsigned long mask = BIT_MASK(nr);
	unsigned long flags;
	addr += BIT_WORD(nr);
	_atomic_spin_lock_irqsave(addr, flags);
	*addr ^= mask;
	_atomic_spin_unlock_irqrestore(addr, flags);
}
static __inline__ int test_and_set_bit(int nr, volatile unsigned long * addr)
{
	unsigned long mask = BIT_MASK(nr);
	unsigned long old;
	unsigned long flags;
	int set;
	addr += BIT_WORD(nr);
	_atomic_spin_lock_irqsave(addr, flags);
	old = *addr;
	set = (old & mask) ? 1 : 0;
	if (!set)
		*addr = old | mask;
	_atomic_spin_unlock_irqrestore(addr, flags);
	return set;
}
static __inline__ int test_and_clear_bit(int nr, volatile unsigned long * addr)
{
	unsigned long mask = BIT_MASK(nr);
	unsigned long old;
	unsigned long flags;
	int set;
	addr += BIT_WORD(nr);
	_atomic_spin_lock_irqsave(addr, flags);
	old = *addr;
	set = (old & mask) ? 1 : 0;
	if (set)
		*addr = old & ~mask;
	_atomic_spin_unlock_irqrestore(addr, flags);
	return set;
}
static __inline__ int test_and_change_bit(int nr, volatile unsigned long * addr)
{
	unsigned long mask = BIT_MASK(nr);
	unsigned long oldbit;
	unsigned long flags;
	addr += BIT_WORD(nr);
	_atomic_spin_lock_irqsave(addr, flags);
	oldbit = *addr;
	*addr = oldbit ^ mask;
	_atomic_spin_unlock_irqrestore(addr, flags);
	return (oldbit & mask) ? 1 : 0;
}
#include <asm-generic/bitops/non-atomic.h>
static __inline__ unsigned long __ffs(unsigned long x)
{
	unsigned long ret;
	__asm__(
#ifdef CONFIG_64BIT
		" ldi       63,%1\n"
		" extrd,u,*<>  %0,63,32,%%r0\n"
		" extrd,u,*TR  %0,31,32,%0\n"	 
		" addi    -32,%1,%1\n"
#else
		" ldi       31,%1\n"
#endif
		" extru,<>  %0,31,16,%%r0\n"
		" extru,TR  %0,15,16,%0\n"	 
		" addi    -16,%1,%1\n"
		" extru,<>  %0,31,8,%%r0\n"
		" extru,TR  %0,23,8,%0\n"	 
		" addi    -8,%1,%1\n"
		" extru,<>  %0,31,4,%%r0\n"
		" extru,TR  %0,27,4,%0\n"	 
		" addi    -4,%1,%1\n"
		" extru,<>  %0,31,2,%%r0\n"
		" extru,TR  %0,29,2,%0\n"	 
		" addi    -2,%1,%1\n"
		" extru,=  %0,31,1,%%r0\n"	 
		" addi    -1,%1,%1\n"
			: "+r" (x), "=r" (ret) );
	return ret;
}
#include <asm-generic/bitops/ffz.h>
static __inline__ int ffs(int x)
{
	return x ? (__ffs((unsigned long)x) + 1) : 0;
}
static __inline__ int fls(unsigned int x)
{
	int ret;
	if (!x)
		return 0;
	__asm__(
	"	ldi		1,%1\n"
	"	extru,<>	%0,15,16,%%r0\n"
	"	zdep,TR		%0,15,16,%0\n"		 
	"	addi		16,%1,%1\n"
	"	extru,<>	%0,7,8,%%r0\n"
	"	zdep,TR		%0,23,24,%0\n"		 
	"	addi		8,%1,%1\n"
	"	extru,<>	%0,3,4,%%r0\n"
	"	zdep,TR		%0,27,28,%0\n"		 
	"	addi		4,%1,%1\n"
	"	extru,<>	%0,1,2,%%r0\n"
	"	zdep,TR		%0,29,30,%0\n"		 
	"	addi		2,%1,%1\n"
	"	extru,=		%0,0,1,%%r0\n"
	"	addi		1,%1,%1\n"		 
		: "+r" (x), "=r" (ret) );
	return ret;
}
#include <asm-generic/bitops/__fls.h>
#include <asm-generic/bitops/fls64.h>
#include <asm-generic/bitops/hweight.h>
#include <asm-generic/bitops/lock.h>
#include <asm-generic/bitops/sched.h>
#include <asm-generic/bitops/le.h>
#include <asm-generic/bitops/ext2-atomic-setbit.h>
#endif  
