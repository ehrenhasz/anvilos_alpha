#ifndef _OPENSOLARIS_SYS_ATOMIC_H_
#define	_OPENSOLARIS_SYS_ATOMIC_H_
#ifndef _STANDALONE
#include <sys/types.h>
#include <machine/atomic.h>
#define	atomic_sub_64	atomic_subtract_64
#if defined(__i386__) && (defined(_KERNEL) || defined(KLD_MODULE))
#define	I386_HAVE_ATOMIC64
#endif
#if defined(__i386__) || defined(__amd64__) || defined(__arm__)
#define	STRONG_FCMPSET
#endif
#if !defined(__LP64__) && !defined(__mips_n32) && \
	!defined(ARM_HAVE_ATOMIC64) && !defined(I386_HAVE_ATOMIC64) && \
	!defined(HAS_EMULATED_ATOMIC64)
extern void atomic_add_64(volatile uint64_t *target, int64_t delta);
extern void atomic_dec_64(volatile uint64_t *target);
extern uint64_t atomic_swap_64(volatile uint64_t *a, uint64_t value);
extern uint64_t atomic_load_64(volatile uint64_t *a);
extern uint64_t atomic_add_64_nv(volatile uint64_t *target, int64_t delta);
extern uint64_t atomic_cas_64(volatile uint64_t *target, uint64_t cmp,
    uint64_t newval);
#endif
#define	membar_consumer()		atomic_thread_fence_acq()
#define	membar_producer()		atomic_thread_fence_rel()
#define	membar_sync()			atomic_thread_fence_seq_cst()
static __inline uint32_t
atomic_add_32_nv(volatile uint32_t *target, int32_t delta)
{
	return (atomic_fetchadd_32(target, delta) + delta);
}
static __inline uint_t
atomic_add_int_nv(volatile uint_t *target, int delta)
{
	return (atomic_add_32_nv(target, delta));
}
static __inline void
atomic_inc_32(volatile uint32_t *target)
{
	atomic_add_32(target, 1);
}
static __inline uint32_t
atomic_inc_32_nv(volatile uint32_t *target)
{
	return (atomic_add_32_nv(target, 1));
}
static __inline void
atomic_dec_32(volatile uint32_t *target)
{
	atomic_subtract_32(target, 1);
}
static __inline uint32_t
atomic_dec_32_nv(volatile uint32_t *target)
{
	return (atomic_add_32_nv(target, -1));
}
#ifndef __sparc64__
static inline uint32_t
atomic_cas_32(volatile uint32_t *target, uint32_t cmp, uint32_t newval)
{
#ifdef STRONG_FCMPSET
	(void) atomic_fcmpset_32(target, &cmp, newval);
#else
	uint32_t expected = cmp;
	do {
		if (atomic_fcmpset_32(target, &cmp, newval))
			break;
	} while (cmp == expected);
#endif
	return (cmp);
}
#endif
#if defined(__LP64__) || defined(__mips_n32) || \
	defined(ARM_HAVE_ATOMIC64) || defined(I386_HAVE_ATOMIC64) || \
	defined(HAS_EMULATED_ATOMIC64)
static __inline void
atomic_dec_64(volatile uint64_t *target)
{
	atomic_subtract_64(target, 1);
}
static inline uint64_t
atomic_add_64_nv(volatile uint64_t *target, int64_t delta)
{
	return (atomic_fetchadd_64(target, delta) + delta);
}
#ifndef __sparc64__
static inline uint64_t
atomic_cas_64(volatile uint64_t *target, uint64_t cmp, uint64_t newval)
{
#ifdef STRONG_FCMPSET
	(void) atomic_fcmpset_64(target, &cmp, newval);
#else
	uint64_t expected = cmp;
	do {
		if (atomic_fcmpset_64(target, &cmp, newval))
			break;
	} while (cmp == expected);
#endif
	return (cmp);
}
#endif
#endif
static __inline void
atomic_inc_64(volatile uint64_t *target)
{
	atomic_add_64(target, 1);
}
static __inline uint64_t
atomic_inc_64_nv(volatile uint64_t *target)
{
	return (atomic_add_64_nv(target, 1));
}
static __inline uint64_t
atomic_dec_64_nv(volatile uint64_t *target)
{
	return (atomic_add_64_nv(target, -1));
}
#ifdef __LP64__
static __inline void *
atomic_cas_ptr(volatile void *target, void *cmp,  void *newval)
{
	return ((void *)atomic_cas_64((volatile uint64_t *)target,
	    (uint64_t)cmp, (uint64_t)newval));
}
#else
static __inline void *
atomic_cas_ptr(volatile void *target, void *cmp,  void *newval)
{
	return ((void *)atomic_cas_32((volatile uint32_t *)target,
	    (uint32_t)cmp, (uint32_t)newval));
}
#endif	 
#else  
#undef atomic_add_64
#define	atomic_add_64(ptr, val) *(ptr) += val
#undef atomic_sub_64
#define	atomic_sub_64(ptr, val) *(ptr) -= val
#endif  
#endif	 
