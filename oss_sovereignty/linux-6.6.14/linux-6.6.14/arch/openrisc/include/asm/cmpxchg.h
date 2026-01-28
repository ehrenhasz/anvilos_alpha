#ifndef __ASM_OPENRISC_CMPXCHG_H
#define __ASM_OPENRISC_CMPXCHG_H
#include  <linux/bits.h>
#include  <linux/compiler.h>
#include  <linux/types.h>
#define __HAVE_ARCH_CMPXCHG 1
static inline unsigned long cmpxchg_u32(volatile void *ptr,
		unsigned long old, unsigned long new)
{
	__asm__ __volatile__(
		"1:	l.lwa %0, 0(%1)		\n"
		"	l.sfeq %0, %2		\n"
		"	l.bnf 2f		\n"
		"	 l.nop			\n"
		"	l.swa 0(%1), %3		\n"
		"	l.bnf 1b		\n"
		"	 l.nop			\n"
		"2:				\n"
		: "=&r"(old)
		: "r"(ptr), "r"(old), "r"(new)
		: "cc", "memory");
	return old;
}
static inline unsigned long xchg_u32(volatile void *ptr,
		unsigned long val)
{
	__asm__ __volatile__(
		"1:	l.lwa %0, 0(%1)		\n"
		"	l.swa 0(%1), %2		\n"
		"	l.bnf 1b		\n"
		"	 l.nop			\n"
		: "=&r"(val)
		: "r"(ptr), "r"(val)
		: "cc", "memory");
	return val;
}
static inline u32 cmpxchg_small(volatile void *ptr, u32 old, u32 new,
				int size)
{
	int off = (unsigned long)ptr % sizeof(u32);
	volatile u32 *p = ptr - off;
#ifdef __BIG_ENDIAN
	int bitoff = (sizeof(u32) - size - off) * BITS_PER_BYTE;
#else
	int bitoff = off * BITS_PER_BYTE;
#endif
	u32 bitmask = ((0x1 << size * BITS_PER_BYTE) - 1) << bitoff;
	u32 load32, old32, new32;
	u32 ret;
	load32 = READ_ONCE(*p);
	while (true) {
		ret = (load32 & bitmask) >> bitoff;
		if (old != ret)
			return ret;
		old32 = (load32 & ~bitmask) | (old << bitoff);
		new32 = (load32 & ~bitmask) | (new << bitoff);
		load32 = cmpxchg_u32(p, old32, new32);
		if (load32 == old32)
			return old;
	}
}
static inline u32 xchg_small(volatile void *ptr, u32 x, int size)
{
	int off = (unsigned long)ptr % sizeof(u32);
	volatile u32 *p = ptr - off;
#ifdef __BIG_ENDIAN
	int bitoff = (sizeof(u32) - size - off) * BITS_PER_BYTE;
#else
	int bitoff = off * BITS_PER_BYTE;
#endif
	u32 bitmask = ((0x1 << size * BITS_PER_BYTE) - 1) << bitoff;
	u32 oldv, newv;
	u32 ret;
	do {
		oldv = READ_ONCE(*p);
		ret = (oldv & bitmask) >> bitoff;
		newv = (oldv & ~bitmask) | (x << bitoff);
	} while (cmpxchg_u32(p, oldv, newv) != oldv);
	return ret;
}
extern unsigned long __cmpxchg_called_with_bad_pointer(void)
	__compiletime_error("Bad argument size for cmpxchg");
static inline unsigned long __cmpxchg(volatile void *ptr, unsigned long old,
		unsigned long new, int size)
{
	switch (size) {
	case 1:
	case 2:
		return cmpxchg_small(ptr, old, new, size);
	case 4:
		return cmpxchg_u32(ptr, old, new);
	default:
		return __cmpxchg_called_with_bad_pointer();
	}
}
#define arch_cmpxchg(ptr, o, n)						\
	({								\
		(__typeof__(*(ptr))) __cmpxchg((ptr),			\
					       (unsigned long)(o),	\
					       (unsigned long)(n),	\
					       sizeof(*(ptr)));		\
	})
extern unsigned long __xchg_called_with_bad_pointer(void)
	__compiletime_error("Bad argument size for xchg");
static inline unsigned long
__arch_xchg(volatile void *ptr, unsigned long with, int size)
{
	switch (size) {
	case 1:
	case 2:
		return xchg_small(ptr, with, size);
	case 4:
		return xchg_u32(ptr, with);
	default:
		return __xchg_called_with_bad_pointer();
	}
}
#define arch_xchg(ptr, with) 						\
	({								\
		(__typeof__(*(ptr))) __arch_xchg((ptr),			\
						 (unsigned long)(with),	\
						 sizeof(*(ptr)));	\
	})
#endif  
