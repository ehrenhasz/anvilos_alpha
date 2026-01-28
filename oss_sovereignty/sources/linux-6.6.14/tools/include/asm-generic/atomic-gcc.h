
#ifndef __TOOLS_ASM_GENERIC_ATOMIC_H
#define __TOOLS_ASM_GENERIC_ATOMIC_H

#include <linux/compiler.h>
#include <linux/types.h>
#include <linux/bitops.h>



#define ATOMIC_INIT(i)	{ (i) }


static inline int atomic_read(const atomic_t *v)
{
	return READ_ONCE((v)->counter);
}


static inline void atomic_set(atomic_t *v, int i)
{
        v->counter = i;
}


static inline void atomic_inc(atomic_t *v)
{
	__sync_add_and_fetch(&v->counter, 1);
}


static inline int atomic_dec_and_test(atomic_t *v)
{
	return __sync_sub_and_fetch(&v->counter, 1) == 0;
}

#define cmpxchg(ptr, oldval, newval) \
	__sync_val_compare_and_swap(ptr, oldval, newval)

static inline int atomic_cmpxchg(atomic_t *v, int oldval, int newval)
{
	return cmpxchg(&(v)->counter, oldval, newval);
}

static inline int test_and_set_bit(long nr, unsigned long *addr)
{
	unsigned long mask = BIT_MASK(nr);
	long old;

	addr += BIT_WORD(nr);

	old = __sync_fetch_and_or(addr, mask);
	return !!(old & mask);
}

static inline int test_and_clear_bit(long nr, unsigned long *addr)
{
	unsigned long mask = BIT_MASK(nr);
	long old;

	addr += BIT_WORD(nr);

	old = __sync_fetch_and_and(addr, ~mask);
	return !!(old & mask);
}

#endif 
