 

#ifndef _SPL_ATOMIC_H
#define	_SPL_ATOMIC_H

#include <linux/module.h>
#include <linux/spinlock.h>
#include <sys/types.h>

 
#define	atomic_inc_32(v)	atomic_inc((atomic_t *)(v))
#define	atomic_dec_32(v)	atomic_dec((atomic_t *)(v))
#define	atomic_add_32(v, i)	atomic_add((i), (atomic_t *)(v))
#define	atomic_sub_32(v, i)	atomic_sub((i), (atomic_t *)(v))
#define	atomic_inc_32_nv(v)	atomic_inc_return((atomic_t *)(v))
#define	atomic_dec_32_nv(v)	atomic_dec_return((atomic_t *)(v))
#define	atomic_add_32_nv(v, i)	atomic_add_return((i), (atomic_t *)(v))
#define	atomic_sub_32_nv(v, i)	atomic_sub_return((i), (atomic_t *)(v))
#define	atomic_cas_32(v, x, y)	atomic_cmpxchg((atomic_t *)(v), x, y)
#define	atomic_swap_32(v, x)	atomic_xchg((atomic_t *)(v), x)
#define	atomic_load_32(v)	atomic_read((atomic_t *)(v))
#define	atomic_store_32(v, x)	atomic_set((atomic_t *)(v), x)
#define	atomic_inc_64(v)	atomic64_inc((atomic64_t *)(v))
#define	atomic_dec_64(v)	atomic64_dec((atomic64_t *)(v))
#define	atomic_add_64(v, i)	atomic64_add((i), (atomic64_t *)(v))
#define	atomic_sub_64(v, i)	atomic64_sub((i), (atomic64_t *)(v))
#define	atomic_inc_64_nv(v)	atomic64_inc_return((atomic64_t *)(v))
#define	atomic_dec_64_nv(v)	atomic64_dec_return((atomic64_t *)(v))
#define	atomic_add_64_nv(v, i)	atomic64_add_return((i), (atomic64_t *)(v))
#define	atomic_sub_64_nv(v, i)	atomic64_sub_return((i), (atomic64_t *)(v))
#define	atomic_cas_64(v, x, y)	atomic64_cmpxchg((atomic64_t *)(v), x, y)
#define	atomic_swap_64(v, x)	atomic64_xchg((atomic64_t *)(v), x)
#define	atomic_load_64(v)	atomic64_read((atomic64_t *)(v))
#define	atomic_store_64(v, x)	atomic64_set((atomic64_t *)(v), x)

#ifdef _LP64
static __inline__ void *
atomic_cas_ptr(volatile void *target,  void *cmp, void *newval)
{
	return ((void *)atomic_cas_64((volatile uint64_t *)target,
	    (uint64_t)cmp, (uint64_t)newval));
}
#else  
static __inline__ void *
atomic_cas_ptr(volatile void *target,  void *cmp, void *newval)
{
	return ((void *)atomic_cas_32((volatile uint32_t *)target,
	    (uint32_t)cmp, (uint32_t)newval));
}
#endif  

#endif   
