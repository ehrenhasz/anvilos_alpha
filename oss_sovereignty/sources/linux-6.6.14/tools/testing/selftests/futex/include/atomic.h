


#ifndef _ATOMIC_H
#define _ATOMIC_H

typedef struct {
	volatile int val;
} atomic_t;

#define ATOMIC_INITIALIZER { 0 }


static inline int
atomic_cmpxchg(atomic_t *addr, int oldval, int newval)
{
	return __sync_val_compare_and_swap(&addr->val, oldval, newval);
}


static inline int
atomic_inc(atomic_t *addr)
{
	return __sync_add_and_fetch(&addr->val, 1);
}


static inline int
atomic_dec(atomic_t *addr)
{
	return __sync_sub_and_fetch(&addr->val, 1);
}


static inline int
atomic_set(atomic_t *addr, int newval)
{
	addr->val = newval;
	return newval;
}

#endif
