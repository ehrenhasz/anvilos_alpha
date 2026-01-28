#ifndef _ARCH_POWERPC_LOCAL_H
#define _ARCH_POWERPC_LOCAL_H
#ifdef CONFIG_PPC_BOOK3S_64
#include <linux/percpu.h>
#include <linux/atomic.h>
#include <linux/irqflags.h>
#include <asm/hw_irq.h>
typedef struct
{
	long v;
} local_t;
#define LOCAL_INIT(i)	{ (i) }
static __inline__ long local_read(const local_t *l)
{
	return READ_ONCE(l->v);
}
static __inline__ void local_set(local_t *l, long i)
{
	WRITE_ONCE(l->v, i);
}
#define LOCAL_OP(op, c_op)						\
static __inline__ void local_##op(long i, local_t *l)			\
{									\
	unsigned long flags;						\
									\
	powerpc_local_irq_pmu_save(flags);				\
	l->v c_op i;						\
	powerpc_local_irq_pmu_restore(flags);				\
}
#define LOCAL_OP_RETURN(op, c_op)					\
static __inline__ long local_##op##_return(long a, local_t *l)		\
{									\
	long t;								\
	unsigned long flags;						\
									\
	powerpc_local_irq_pmu_save(flags);				\
	t = (l->v c_op a);						\
	powerpc_local_irq_pmu_restore(flags);				\
									\
	return t;							\
}
#define LOCAL_OPS(op, c_op)		\
	LOCAL_OP(op, c_op)		\
	LOCAL_OP_RETURN(op, c_op)
LOCAL_OPS(add, +=)
LOCAL_OPS(sub, -=)
#define local_add_negative(a, l)	(local_add_return((a), (l)) < 0)
#define local_inc_return(l)		local_add_return(1LL, l)
#define local_inc(l)			local_inc_return(l)
#define local_inc_and_test(l)		(local_inc_return(l) == 0)
#define local_dec_return(l)		local_sub_return(1LL, l)
#define local_dec(l)			local_dec_return(l)
#define local_sub_and_test(a, l)	(local_sub_return((a), (l)) == 0)
#define local_dec_and_test(l)		(local_dec_return((l)) == 0)
static __inline__ long local_cmpxchg(local_t *l, long o, long n)
{
	long t;
	unsigned long flags;
	powerpc_local_irq_pmu_save(flags);
	t = l->v;
	if (t == o)
		l->v = n;
	powerpc_local_irq_pmu_restore(flags);
	return t;
}
static __inline__ bool local_try_cmpxchg(local_t *l, long *po, long n)
{
	long o = *po, r;
	r = local_cmpxchg(l, o, n);
	if (unlikely(r != o))
		*po = r;
	return likely(r == o);
}
static __inline__ long local_xchg(local_t *l, long n)
{
	long t;
	unsigned long flags;
	powerpc_local_irq_pmu_save(flags);
	t = l->v;
	l->v = n;
	powerpc_local_irq_pmu_restore(flags);
	return t;
}
static __inline__ int local_add_unless(local_t *l, long a, long u)
{
	unsigned long flags;
	int ret = 0;
	powerpc_local_irq_pmu_save(flags);
	if (l->v != u) {
		l->v += a;
		ret = 1;
	}
	powerpc_local_irq_pmu_restore(flags);
	return ret;
}
#define local_inc_not_zero(l)		local_add_unless((l), 1, 0)
#define __local_inc(l)		((l)->v++)
#define __local_dec(l)		((l)->v++)
#define __local_add(i,l)	((l)->v+=(i))
#define __local_sub(i,l)	((l)->v-=(i))
#else  
#include <asm-generic/local.h>
#endif  
#endif  
