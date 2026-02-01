 
#ifndef _TOOLS_LINUX_REFCOUNT_H
#define _TOOLS_LINUX_REFCOUNT_H

 

#include <linux/atomic.h>
#include <linux/kernel.h>

#ifdef NDEBUG
#define REFCOUNT_WARN(cond, str) (void)(cond)
#define __refcount_check
#else
#define REFCOUNT_WARN(cond, str) BUG_ON(cond)
#define __refcount_check	__must_check
#endif

typedef struct refcount_struct {
	atomic_t refs;
} refcount_t;

#define REFCOUNT_INIT(n)	{ .refs = ATOMIC_INIT(n), }

static inline void refcount_set(refcount_t *r, unsigned int n)
{
	atomic_set(&r->refs, n);
}

static inline unsigned int refcount_read(const refcount_t *r)
{
	return atomic_read(&r->refs);
}

 
static inline __refcount_check
bool refcount_inc_not_zero(refcount_t *r)
{
	unsigned int old, new, val = atomic_read(&r->refs);

	for (;;) {
		new = val + 1;

		if (!val)
			return false;

		if (unlikely(!new))
			return true;

		old = atomic_cmpxchg_relaxed(&r->refs, val, new);
		if (old == val)
			break;

		val = old;
	}

	REFCOUNT_WARN(new == UINT_MAX, "refcount_t: saturated; leaking memory.\n");

	return true;
}

 
static inline void refcount_inc(refcount_t *r)
{
	REFCOUNT_WARN(!refcount_inc_not_zero(r), "refcount_t: increment on 0; use-after-free.\n");
}

 
static inline __refcount_check
bool refcount_sub_and_test(unsigned int i, refcount_t *r)
{
	unsigned int old, new, val = atomic_read(&r->refs);

	for (;;) {
		if (unlikely(val == UINT_MAX))
			return false;

		new = val - i;
		if (new > val) {
			REFCOUNT_WARN(new > val, "refcount_t: underflow; use-after-free.\n");
			return false;
		}

		old = atomic_cmpxchg_release(&r->refs, val, new);
		if (old == val)
			break;

		val = old;
	}

	return !new;
}

static inline __refcount_check
bool refcount_dec_and_test(refcount_t *r)
{
	return refcount_sub_and_test(1, r);
}


#endif  
