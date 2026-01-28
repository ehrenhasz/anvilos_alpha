#ifndef _ASM_ARC_ATOMIC_SPLOCK_H
#define _ASM_ARC_ATOMIC_SPLOCK_H
static inline void arch_atomic_set(atomic_t *v, int i)
{
	unsigned long flags;
	atomic_ops_lock(flags);
	WRITE_ONCE(v->counter, i);
	atomic_ops_unlock(flags);
}
#define arch_atomic_set_release(v, i)	arch_atomic_set((v), (i))
#define ATOMIC_OP(op, c_op, asm_op)					\
static inline void arch_atomic_##op(int i, atomic_t *v)			\
{									\
	unsigned long flags;						\
									\
	atomic_ops_lock(flags);						\
	v->counter c_op i;						\
	atomic_ops_unlock(flags);					\
}
#define ATOMIC_OP_RETURN(op, c_op, asm_op)				\
static inline int arch_atomic_##op##_return(int i, atomic_t *v)		\
{									\
	unsigned long flags;						\
	unsigned int temp;						\
									\
	 								\
	atomic_ops_lock(flags);						\
	temp = v->counter;						\
	temp c_op i;							\
	v->counter = temp;						\
	atomic_ops_unlock(flags);					\
									\
	return temp;							\
}
#define ATOMIC_FETCH_OP(op, c_op, asm_op)				\
static inline int arch_atomic_fetch_##op(int i, atomic_t *v)		\
{									\
	unsigned long flags;						\
	unsigned int orig;						\
									\
	 								\
	atomic_ops_lock(flags);						\
	orig = v->counter;						\
	v->counter c_op i;						\
	atomic_ops_unlock(flags);					\
									\
	return orig;							\
}
#define ATOMIC_OPS(op, c_op, asm_op)					\
	ATOMIC_OP(op, c_op, asm_op)					\
	ATOMIC_OP_RETURN(op, c_op, asm_op)				\
	ATOMIC_FETCH_OP(op, c_op, asm_op)
ATOMIC_OPS(add, +=, add)
ATOMIC_OPS(sub, -=, sub)
#define arch_atomic_fetch_add		arch_atomic_fetch_add
#define arch_atomic_fetch_sub		arch_atomic_fetch_sub
#define arch_atomic_add_return		arch_atomic_add_return
#define arch_atomic_sub_return		arch_atomic_sub_return
#undef ATOMIC_OPS
#define ATOMIC_OPS(op, c_op, asm_op)					\
	ATOMIC_OP(op, c_op, asm_op)					\
	ATOMIC_FETCH_OP(op, c_op, asm_op)
ATOMIC_OPS(and, &=, and)
ATOMIC_OPS(andnot, &= ~, bic)
ATOMIC_OPS(or, |=, or)
ATOMIC_OPS(xor, ^=, xor)
#define arch_atomic_andnot		arch_atomic_andnot
#define arch_atomic_fetch_and		arch_atomic_fetch_and
#define arch_atomic_fetch_andnot	arch_atomic_fetch_andnot
#define arch_atomic_fetch_or		arch_atomic_fetch_or
#define arch_atomic_fetch_xor		arch_atomic_fetch_xor
#undef ATOMIC_OPS
#undef ATOMIC_FETCH_OP
#undef ATOMIC_OP_RETURN
#undef ATOMIC_OP
#endif
