#ifndef __ASM_SH_ATOMIC_LLSC_H
#define __ASM_SH_ATOMIC_LLSC_H
#define ATOMIC_OP(op)							\
static inline void arch_atomic_##op(int i, atomic_t *v)			\
{									\
	unsigned long tmp;						\
									\
	__asm__ __volatile__ (						\
"1:	movli.l @%2, %0		! atomic_" #op "\n"			\
"	" #op "	%1, %0				\n"			\
"	movco.l	%0, @%2				\n"			\
"	bf	1b				\n"			\
	: "=&z" (tmp)							\
	: "r" (i), "r" (&v->counter)					\
	: "t");								\
}
#define ATOMIC_OP_RETURN(op)						\
static inline int arch_atomic_##op##_return(int i, atomic_t *v)		\
{									\
	unsigned long temp;						\
									\
	__asm__ __volatile__ (						\
"1:	movli.l @%2, %0		! atomic_" #op "_return	\n"		\
"	" #op "	%1, %0					\n"		\
"	movco.l	%0, @%2					\n"		\
"	bf	1b					\n"		\
"	synco						\n"		\
	: "=&z" (temp)							\
	: "r" (i), "r" (&v->counter)					\
	: "t");								\
									\
	return temp;							\
}
#define ATOMIC_FETCH_OP(op)						\
static inline int arch_atomic_fetch_##op(int i, atomic_t *v)		\
{									\
	unsigned long res, temp;					\
									\
	__asm__ __volatile__ (						\
"1:	movli.l @%3, %0		! atomic_fetch_" #op "	\n"		\
"	mov %0, %1					\n"		\
"	" #op "	%2, %0					\n"		\
"	movco.l	%0, @%3					\n"		\
"	bf	1b					\n"		\
"	synco						\n"		\
	: "=&z" (temp), "=&r" (res)					\
	: "r" (i), "r" (&v->counter)					\
	: "t");								\
									\
	return res;							\
}
#define ATOMIC_OPS(op) ATOMIC_OP(op) ATOMIC_OP_RETURN(op) ATOMIC_FETCH_OP(op)
ATOMIC_OPS(add)
ATOMIC_OPS(sub)
#define arch_atomic_add_return	arch_atomic_add_return
#define arch_atomic_sub_return	arch_atomic_sub_return
#define arch_atomic_fetch_add	arch_atomic_fetch_add
#define arch_atomic_fetch_sub	arch_atomic_fetch_sub
#undef ATOMIC_OPS
#define ATOMIC_OPS(op) ATOMIC_OP(op) ATOMIC_FETCH_OP(op)
ATOMIC_OPS(and)
ATOMIC_OPS(or)
ATOMIC_OPS(xor)
#define arch_atomic_fetch_and	arch_atomic_fetch_and
#define arch_atomic_fetch_or	arch_atomic_fetch_or
#define arch_atomic_fetch_xor	arch_atomic_fetch_xor
#undef ATOMIC_OPS
#undef ATOMIC_FETCH_OP
#undef ATOMIC_OP_RETURN
#undef ATOMIC_OP
#endif  
