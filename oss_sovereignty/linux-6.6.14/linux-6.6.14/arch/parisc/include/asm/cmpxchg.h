#ifndef _ASM_PARISC_CMPXCHG_H_
#define _ASM_PARISC_CMPXCHG_H_
extern void __xchg_called_with_bad_pointer(void);
extern unsigned long __xchg8(char, volatile char *);
extern unsigned long __xchg32(int, volatile int *);
#ifdef CONFIG_64BIT
extern unsigned long __xchg64(unsigned long, volatile unsigned long *);
#endif
static inline unsigned long
__arch_xchg(unsigned long x, volatile void *ptr, int size)
{
	switch (size) {
#ifdef CONFIG_64BIT
	case 8: return __xchg64(x, (volatile unsigned long *) ptr);
#endif
	case 4: return __xchg32((int) x, (volatile int *) ptr);
	case 1: return __xchg8((char) x, (volatile char *) ptr);
	}
	__xchg_called_with_bad_pointer();
	return x;
}
#define arch_xchg(ptr, x)						\
({									\
	__typeof__(*(ptr)) __ret;					\
	__typeof__(*(ptr)) _x_ = (x);					\
	__ret = (__typeof__(*(ptr)))					\
		__arch_xchg((unsigned long)_x_, (ptr), sizeof(*(ptr)));	\
	__ret;								\
})
extern void __cmpxchg_called_with_bad_pointer(void);
extern unsigned long __cmpxchg_u32(volatile unsigned int *m, unsigned int old,
				   unsigned int new_);
extern u64 __cmpxchg_u64(volatile u64 *ptr, u64 old, u64 new_);
extern u8 __cmpxchg_u8(volatile u8 *ptr, u8 old, u8 new_);
static inline unsigned long
__cmpxchg(volatile void *ptr, unsigned long old, unsigned long new_, int size)
{
	switch (size) {
#ifdef CONFIG_64BIT
	case 8: return __cmpxchg_u64((u64 *)ptr, old, new_);
#endif
	case 4: return __cmpxchg_u32((unsigned int *)ptr,
				     (unsigned int)old, (unsigned int)new_);
	case 1: return __cmpxchg_u8((u8 *)ptr, old & 0xff, new_ & 0xff);
	}
	__cmpxchg_called_with_bad_pointer();
	return old;
}
#define arch_cmpxchg(ptr, o, n)						 \
({									 \
	__typeof__(*(ptr)) _o_ = (o);					 \
	__typeof__(*(ptr)) _n_ = (n);					 \
	(__typeof__(*(ptr))) __cmpxchg((ptr), (unsigned long)_o_,	 \
				    (unsigned long)_n_, sizeof(*(ptr))); \
})
#include <asm-generic/cmpxchg-local.h>
static inline unsigned long __cmpxchg_local(volatile void *ptr,
				      unsigned long old,
				      unsigned long new_, int size)
{
	switch (size) {
#ifdef CONFIG_64BIT
	case 8:	return __cmpxchg_u64((u64 *)ptr, old, new_);
#endif
	case 4:	return __cmpxchg_u32(ptr, old, new_);
	default:
		return __generic_cmpxchg_local(ptr, old, new_, size);
	}
}
#define arch_cmpxchg_local(ptr, o, n)					\
	((__typeof__(*(ptr)))__cmpxchg_local((ptr), (unsigned long)(o),	\
			(unsigned long)(n), sizeof(*(ptr))))
#ifdef CONFIG_64BIT
#define arch_cmpxchg64_local(ptr, o, n)					\
({									\
	BUILD_BUG_ON(sizeof(*(ptr)) != 8);				\
	cmpxchg_local((ptr), (o), (n));					\
})
#else
#define arch_cmpxchg64_local(ptr, o, n) __generic_cmpxchg64_local((ptr), (o), (n))
#endif
#define arch_cmpxchg64(ptr, o, n) __cmpxchg_u64(ptr, o, n)
#endif  
