#ifndef __ASM_CMPXCHG_H
#define __ASM_CMPXCHG_H
#include <linux/bug.h>
#include <linux/irqflags.h>
#include <asm/asm.h>
#include <asm/compiler.h>
#include <asm/sync.h>
extern unsigned long __cmpxchg_called_with_bad_pointer(void)
	__compiletime_error("Bad argument size for cmpxchg");
extern unsigned long __cmpxchg64_unsupported(void)
	__compiletime_error("cmpxchg64 not available; cpu_has_64bits may be false");
extern unsigned long __xchg_called_with_bad_pointer(void)
	__compiletime_error("Bad argument size for xchg");
#define __xchg_asm(ld, st, m, val)					\
({									\
	__typeof(*(m)) __ret;						\
									\
	if (kernel_uses_llsc) {						\
		__asm__ __volatile__(					\
		"	.set	push				\n"	\
		"	.set	noat				\n"	\
		"	.set	push				\n"	\
		"	.set	" MIPS_ISA_ARCH_LEVEL "		\n"	\
		"	" __SYNC(full, loongson3_war) "		\n"	\
		"1:	" ld "	%0, %2		# __xchg_asm	\n"	\
		"	.set	pop				\n"	\
		"	move	$1, %z3				\n"	\
		"	.set	" MIPS_ISA_ARCH_LEVEL "		\n"	\
		"	" st "	$1, %1				\n"	\
		"\t" __stringify(SC_BEQZ)	"	$1, 1b	\n"	\
		"	.set	pop				\n"	\
		: "=&r" (__ret), "=" GCC_OFF_SMALL_ASM() (*m)		\
		: GCC_OFF_SMALL_ASM() (*m), "Jr" (val)			\
		: __LLSC_CLOBBER);					\
	} else {							\
		unsigned long __flags;					\
									\
		raw_local_irq_save(__flags);				\
		__ret = *m;						\
		*m = val;						\
		raw_local_irq_restore(__flags);				\
	}								\
									\
	__ret;								\
})
extern unsigned long __xchg_small(volatile void *ptr, unsigned long val,
				  unsigned int size);
static __always_inline
unsigned long __arch_xchg(volatile void *ptr, unsigned long x, int size)
{
	switch (size) {
	case 1:
	case 2:
		return __xchg_small(ptr, x, size);
	case 4:
		return __xchg_asm("ll", "sc", (volatile u32 *)ptr, x);
	case 8:
		if (!IS_ENABLED(CONFIG_64BIT))
			return __xchg_called_with_bad_pointer();
		return __xchg_asm("lld", "scd", (volatile u64 *)ptr, x);
	default:
		return __xchg_called_with_bad_pointer();
	}
}
#define arch_xchg(ptr, x)						\
({									\
	__typeof__(*(ptr)) __res;					\
									\
	 								\
	if (__SYNC_loongson3_war == 0)					\
		smp_mb__before_llsc();					\
									\
	__res = (__typeof__(*(ptr)))					\
		__arch_xchg((ptr), (unsigned long)(x), sizeof(*(ptr)));	\
									\
	smp_llsc_mb();							\
									\
	__res;								\
})
#define __cmpxchg_asm(ld, st, m, old, new)				\
({									\
	__typeof(*(m)) __ret;						\
									\
	if (kernel_uses_llsc) {						\
		__asm__ __volatile__(					\
		"	.set	push				\n"	\
		"	.set	noat				\n"	\
		"	.set	push				\n"	\
		"	.set	"MIPS_ISA_ARCH_LEVEL"		\n"	\
		"	" __SYNC(full, loongson3_war) "		\n"	\
		"1:	" ld "	%0, %2		# __cmpxchg_asm \n"	\
		"	bne	%0, %z3, 2f			\n"	\
		"	.set	pop				\n"	\
		"	move	$1, %z4				\n"	\
		"	.set	"MIPS_ISA_ARCH_LEVEL"		\n"	\
		"	" st "	$1, %1				\n"	\
		"\t" __stringify(SC_BEQZ)	"	$1, 1b	\n"	\
		"	.set	pop				\n"	\
		"2:	" __SYNC(full, loongson3_war) "		\n"	\
		: "=&r" (__ret), "=" GCC_OFF_SMALL_ASM() (*m)		\
		: GCC_OFF_SMALL_ASM() (*m), "Jr" (old), "Jr" (new)	\
		: __LLSC_CLOBBER);					\
	} else {							\
		unsigned long __flags;					\
									\
		raw_local_irq_save(__flags);				\
		__ret = *m;						\
		if (__ret == old)					\
			*m = new;					\
		raw_local_irq_restore(__flags);				\
	}								\
									\
	__ret;								\
})
extern unsigned long __cmpxchg_small(volatile void *ptr, unsigned long old,
				     unsigned long new, unsigned int size);
static __always_inline
unsigned long __cmpxchg(volatile void *ptr, unsigned long old,
			unsigned long new, unsigned int size)
{
	switch (size) {
	case 1:
	case 2:
		return __cmpxchg_small(ptr, old, new, size);
	case 4:
		return __cmpxchg_asm("ll", "sc", (volatile u32 *)ptr,
				     (u32)old, new);
	case 8:
		if (!IS_ENABLED(CONFIG_64BIT))
			return __cmpxchg_called_with_bad_pointer();
		return __cmpxchg_asm("lld", "scd", (volatile u64 *)ptr,
				     (u64)old, new);
	default:
		return __cmpxchg_called_with_bad_pointer();
	}
}
#define arch_cmpxchg_local(ptr, old, new)				\
	((__typeof__(*(ptr)))						\
		__cmpxchg((ptr),					\
			  (unsigned long)(__typeof__(*(ptr)))(old),	\
			  (unsigned long)(__typeof__(*(ptr)))(new),	\
			  sizeof(*(ptr))))
#define arch_cmpxchg(ptr, old, new)					\
({									\
	__typeof__(*(ptr)) __res;					\
									\
	 								\
	if (__SYNC_loongson3_war == 0)					\
		smp_mb__before_llsc();					\
									\
	__res = arch_cmpxchg_local((ptr), (old), (new));		\
									\
	 								\
	if (__SYNC_loongson3_war == 0)					\
		smp_llsc_mb();						\
									\
	__res;								\
})
#ifdef CONFIG_64BIT
#define arch_cmpxchg64_local(ptr, o, n)					\
  ({									\
	BUILD_BUG_ON(sizeof(*(ptr)) != 8);				\
	arch_cmpxchg_local((ptr), (o), (n));				\
  })
#define arch_cmpxchg64(ptr, o, n)					\
  ({									\
	BUILD_BUG_ON(sizeof(*(ptr)) != 8);				\
	arch_cmpxchg((ptr), (o), (n));					\
  })
#else
# include <asm-generic/cmpxchg-local.h>
# define arch_cmpxchg64_local(ptr, o, n) __generic_cmpxchg64_local((ptr), (o), (n))
# ifdef CONFIG_SMP
static inline unsigned long __cmpxchg64(volatile void *ptr,
					unsigned long long old,
					unsigned long long new)
{
	unsigned long long tmp, ret;
	unsigned long flags;
	local_irq_save(flags);
	asm volatile(
	"	.set	push				\n"
	"	.set	" MIPS_ISA_ARCH_LEVEL "		\n"
	"	" __SYNC(full, loongson3_war) "		\n"
	"1:	lld	%L0, %3		# __cmpxchg64	\n"
	"	.set	pop				\n"
	"	dsra	%M0, %L0, 32			\n"
	"	sll	%L0, %L0, 0			\n"
	"	bne	%M0, %M4, 2f			\n"
	"	bne	%L0, %L4, 2f			\n"
#  if MIPS_ISA_REV >= 2
	"	move	%L1, %L5			\n"
	"	dins	%L1, %M5, 32, 32		\n"
#  else
	"	dsll	%L1, %L5, 32			\n"
	"	dsrl	%L1, %L1, 32			\n"
	"	.set	noat				\n"
	"	dsll	$at, %M5, 32			\n"
	"	or	%L1, %L1, $at			\n"
	"	.set	at				\n"
#  endif
	"	.set	push				\n"
	"	.set	" MIPS_ISA_ARCH_LEVEL "		\n"
	"	scd	%L1, %2				\n"
	"\t" __stringify(SC_BEQZ) "	%L1, 1b		\n"
	"2:	" __SYNC(full, loongson3_war) "		\n"
	"	.set	pop				\n"
	: "=&r"(ret),
	  "=&r"(tmp),
	  "=" GCC_OFF_SMALL_ASM() (*(unsigned long long *)ptr)
	: GCC_OFF_SMALL_ASM() (*(unsigned long long *)ptr),
	  "r" (old),
	  "r" (new)
	: "memory");
	local_irq_restore(flags);
	return ret;
}
#  define arch_cmpxchg64(ptr, o, n) ({					\
	unsigned long long __old = (__typeof__(*(ptr)))(o);		\
	unsigned long long __new = (__typeof__(*(ptr)))(n);		\
	__typeof__(*(ptr)) __res;					\
									\
	 								\
	if (cpu_has_64bits && kernel_uses_llsc) {			\
		smp_mb__before_llsc();					\
		__res = __cmpxchg64((ptr), __old, __new);		\
		smp_llsc_mb();						\
	} else {							\
		__res = __cmpxchg64_unsupported();			\
	}								\
									\
	__res;								\
})
# else  
#  define arch_cmpxchg64(ptr, o, n) arch_cmpxchg64_local((ptr), (o), (n))
# endif  
#endif  
#endif  
