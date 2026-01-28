#ifndef _XTENSA_ATOMIC_H
#define _XTENSA_ATOMIC_H
#include <linux/stringify.h>
#include <linux/types.h>
#include <asm/processor.h>
#include <asm/cmpxchg.h>
#include <asm/barrier.h>
#define arch_atomic_read(v)		READ_ONCE((v)->counter)
#define arch_atomic_set(v,i)		WRITE_ONCE((v)->counter, (i))
#if XCHAL_HAVE_EXCLUSIVE
#define ATOMIC_OP(op)							\
static inline void arch_atomic_##op(int i, atomic_t *v)			\
{									\
	unsigned long tmp;						\
	int result;							\
									\
	__asm__ __volatile__(						\
			"1:     l32ex   %[tmp], %[addr]\n"		\
			"       " #op " %[result], %[tmp], %[i]\n"	\
			"       s32ex   %[result], %[addr]\n"		\
			"       getex   %[result]\n"			\
			"       beqz    %[result], 1b\n"		\
			: [result] "=&a" (result), [tmp] "=&a" (tmp)	\
			: [i] "a" (i), [addr] "a" (v)			\
			: "memory"					\
			);						\
}									\
#define ATOMIC_OP_RETURN(op)						\
static inline int arch_atomic_##op##_return(int i, atomic_t *v)		\
{									\
	unsigned long tmp;						\
	int result;							\
									\
	__asm__ __volatile__(						\
			"1:     l32ex   %[tmp], %[addr]\n"		\
			"       " #op " %[result], %[tmp], %[i]\n"	\
			"       s32ex   %[result], %[addr]\n"		\
			"       getex   %[result]\n"			\
			"       beqz    %[result], 1b\n"		\
			"       " #op " %[result], %[tmp], %[i]\n"	\
			: [result] "=&a" (result), [tmp] "=&a" (tmp)	\
			: [i] "a" (i), [addr] "a" (v)			\
			: "memory"					\
			);						\
									\
	return result;							\
}
#define ATOMIC_FETCH_OP(op)						\
static inline int arch_atomic_fetch_##op(int i, atomic_t *v)		\
{									\
	unsigned long tmp;						\
	int result;							\
									\
	__asm__ __volatile__(						\
			"1:     l32ex   %[tmp], %[addr]\n"		\
			"       " #op " %[result], %[tmp], %[i]\n"	\
			"       s32ex   %[result], %[addr]\n"		\
			"       getex   %[result]\n"			\
			"       beqz    %[result], 1b\n"		\
			: [result] "=&a" (result), [tmp] "=&a" (tmp)	\
			: [i] "a" (i), [addr] "a" (v)			\
			: "memory"					\
			);						\
									\
	return tmp;							\
}
#elif XCHAL_HAVE_S32C1I
#define ATOMIC_OP(op)							\
static inline void arch_atomic_##op(int i, atomic_t * v)		\
{									\
	unsigned long tmp;						\
	int result;							\
									\
	__asm__ __volatile__(						\
			"1:     l32i    %[tmp], %[mem]\n"		\
			"       wsr     %[tmp], scompare1\n"		\
			"       " #op " %[result], %[tmp], %[i]\n"	\
			"       s32c1i  %[result], %[mem]\n"		\
			"       bne     %[result], %[tmp], 1b\n"	\
			: [result] "=&a" (result), [tmp] "=&a" (tmp),	\
			  [mem] "+m" (*v)				\
			: [i] "a" (i)					\
			: "memory"					\
			);						\
}									\
#define ATOMIC_OP_RETURN(op)						\
static inline int arch_atomic_##op##_return(int i, atomic_t * v)	\
{									\
	unsigned long tmp;						\
	int result;							\
									\
	__asm__ __volatile__(						\
			"1:     l32i    %[tmp], %[mem]\n"		\
			"       wsr     %[tmp], scompare1\n"		\
			"       " #op " %[result], %[tmp], %[i]\n"	\
			"       s32c1i  %[result], %[mem]\n"		\
			"       bne     %[result], %[tmp], 1b\n"	\
			"       " #op " %[result], %[result], %[i]\n"	\
			: [result] "=&a" (result), [tmp] "=&a" (tmp),	\
			  [mem] "+m" (*v)				\
			: [i] "a" (i)					\
			: "memory"					\
			);						\
									\
	return result;							\
}
#define ATOMIC_FETCH_OP(op)						\
static inline int arch_atomic_fetch_##op(int i, atomic_t * v)		\
{									\
	unsigned long tmp;						\
	int result;							\
									\
	__asm__ __volatile__(						\
			"1:     l32i    %[tmp], %[mem]\n"		\
			"       wsr     %[tmp], scompare1\n"		\
			"       " #op " %[result], %[tmp], %[i]\n"	\
			"       s32c1i  %[result], %[mem]\n"		\
			"       bne     %[result], %[tmp], 1b\n"	\
			: [result] "=&a" (result), [tmp] "=&a" (tmp),	\
			  [mem] "+m" (*v)				\
			: [i] "a" (i)					\
			: "memory"					\
			);						\
									\
	return result;							\
}
#else  
#define ATOMIC_OP(op)							\
static inline void arch_atomic_##op(int i, atomic_t * v)		\
{									\
	unsigned int vval;						\
									\
	__asm__ __volatile__(						\
			"       rsil    a14, "__stringify(TOPLEVEL)"\n"	\
			"       l32i    %[result], %[mem]\n"		\
			"       " #op " %[result], %[result], %[i]\n"	\
			"       s32i    %[result], %[mem]\n"		\
			"       wsr     a14, ps\n"			\
			"       rsync\n"				\
			: [result] "=&a" (vval), [mem] "+m" (*v)	\
			: [i] "a" (i)					\
			: "a14", "memory"				\
			);						\
}									\
#define ATOMIC_OP_RETURN(op)						\
static inline int arch_atomic_##op##_return(int i, atomic_t * v)	\
{									\
	unsigned int vval;						\
									\
	__asm__ __volatile__(						\
			"       rsil    a14,"__stringify(TOPLEVEL)"\n"	\
			"       l32i    %[result], %[mem]\n"		\
			"       " #op " %[result], %[result], %[i]\n"	\
			"       s32i    %[result], %[mem]\n"		\
			"       wsr     a14, ps\n"			\
			"       rsync\n"				\
			: [result] "=&a" (vval), [mem] "+m" (*v)	\
			: [i] "a" (i)					\
			: "a14", "memory"				\
			);						\
									\
	return vval;							\
}
#define ATOMIC_FETCH_OP(op)						\
static inline int arch_atomic_fetch_##op(int i, atomic_t * v)		\
{									\
	unsigned int tmp, vval;						\
									\
	__asm__ __volatile__(						\
			"       rsil    a14,"__stringify(TOPLEVEL)"\n"	\
			"       l32i    %[result], %[mem]\n"		\
			"       " #op " %[tmp], %[result], %[i]\n"	\
			"       s32i    %[tmp], %[mem]\n"		\
			"       wsr     a14, ps\n"			\
			"       rsync\n"				\
			: [result] "=&a" (vval), [tmp] "=&a" (tmp),	\
			  [mem] "+m" (*v)				\
			: [i] "a" (i)					\
			: "a14", "memory"				\
			);						\
									\
	return vval;							\
}
#endif  
#define ATOMIC_OPS(op) ATOMIC_OP(op) ATOMIC_FETCH_OP(op) ATOMIC_OP_RETURN(op)
ATOMIC_OPS(add)
ATOMIC_OPS(sub)
#define arch_atomic_add_return			arch_atomic_add_return
#define arch_atomic_sub_return			arch_atomic_sub_return
#define arch_atomic_fetch_add			arch_atomic_fetch_add
#define arch_atomic_fetch_sub			arch_atomic_fetch_sub
#undef ATOMIC_OPS
#define ATOMIC_OPS(op) ATOMIC_OP(op) ATOMIC_FETCH_OP(op)
ATOMIC_OPS(and)
ATOMIC_OPS(or)
ATOMIC_OPS(xor)
#define arch_atomic_fetch_and			arch_atomic_fetch_and
#define arch_atomic_fetch_or			arch_atomic_fetch_or
#define arch_atomic_fetch_xor			arch_atomic_fetch_xor
#undef ATOMIC_OPS
#undef ATOMIC_FETCH_OP
#undef ATOMIC_OP_RETURN
#undef ATOMIC_OP
#endif  
