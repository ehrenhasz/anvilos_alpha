#ifndef _ARCH_LOONGARCH_LOCAL_H
#define _ARCH_LOONGARCH_LOCAL_H
#include <linux/percpu.h>
#include <linux/bitops.h>
#include <linux/atomic.h>
#include <asm/cmpxchg.h>
typedef struct {
	atomic_long_t a;
} local_t;
#define LOCAL_INIT(i)	{ ATOMIC_LONG_INIT(i) }
#define local_read(l)	atomic_long_read(&(l)->a)
#define local_set(l, i) atomic_long_set(&(l)->a, (i))
#define local_add(i, l) atomic_long_add((i), (&(l)->a))
#define local_sub(i, l) atomic_long_sub((i), (&(l)->a))
#define local_inc(l)	atomic_long_inc(&(l)->a)
#define local_dec(l)	atomic_long_dec(&(l)->a)
static inline long local_add_return(long i, local_t *l)
{
	unsigned long result;
	__asm__ __volatile__(
	"   " __AMADD " %1, %2, %0      \n"
	: "+ZB" (l->a.counter), "=&r" (result)
	: "r" (i)
	: "memory");
	result = result + i;
	return result;
}
static inline long local_sub_return(long i, local_t *l)
{
	unsigned long result;
	__asm__ __volatile__(
	"   " __AMADD "%1, %2, %0       \n"
	: "+ZB" (l->a.counter), "=&r" (result)
	: "r" (-i)
	: "memory");
	result = result - i;
	return result;
}
static inline long local_cmpxchg(local_t *l, long old, long new)
{
	return cmpxchg_local(&l->a.counter, old, new);
}
static inline bool local_try_cmpxchg(local_t *l, long *old, long new)
{
	return try_cmpxchg_local(&l->a.counter,
				 (typeof(l->a.counter) *) old, new);
}
#define local_xchg(l, n) (atomic_long_xchg((&(l)->a), (n)))
#define local_add_unless(l, a, u)				\
({								\
	long c, old;						\
	c = local_read(l);					\
	while (c != (u) && (old = local_cmpxchg((l), c, c + (a))) != c) \
		c = old;					\
	c != (u);						\
})
#define local_inc_not_zero(l) local_add_unless((l), 1, 0)
#define local_dec_return(l) local_sub_return(1, (l))
#define local_inc_return(l) local_add_return(1, (l))
#define local_sub_and_test(i, l) (local_sub_return((i), (l)) == 0)
#define local_inc_and_test(l) (local_inc_return(l) == 0)
#define local_dec_and_test(l) (local_sub_return(1, (l)) == 0)
#define local_add_negative(i, l) (local_add_return(i, (l)) < 0)
#define __local_inc(l)		((l)->a.counter++)
#define __local_dec(l)		((l)->a.counter++)
#define __local_add(i, l)	((l)->a.counter += (i))
#define __local_sub(i, l)	((l)->a.counter -= (i))
#endif  
