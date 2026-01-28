
#ifndef	_LINUX_COMPILER_H_
#define	_LINUX_COMPILER_H_

#include <sys/cdefs.h>

#define	__user
#define	__kernel
#define	__safe
#define	__force
#define	__nocast
#define	__iomem
#define	__chk_user_ptr(x)		((void)0)
#define	__chk_io_ptr(x)			((void)0)
#define	__builtin_warning(x, y...)	(1)
#define	__acquires(x)
#define	__releases(x)
#define	__acquire(x)			do { } while (0)
#define	__release(x)			do { } while (0)
#define	__cond_lock(x, c)		(c)
#define	__bitwise
#define	__devinitdata
#define	__deprecated
#define	__init
#define	__initconst
#define	__devinit
#define	__devexit
#define	__exit
#define	__rcu
#define	__percpu
#define	__weak __weak_symbol
#define	__malloc
#define	___stringify(...)		#__VA_ARGS__
#define	__stringify(...)		___stringify(__VA_ARGS__)
#define	__attribute_const__		__attribute__((__const__))
#undef __always_inline
#define	__always_inline			inline
#define	noinline			__noinline
#define	____cacheline_aligned		__aligned(CACHE_LINE_SIZE)
#define	zfs_fallthrough			__attribute__((__fallthrough__))

#if !defined(_KERNEL) && !defined(_STANDALONE)
#define	likely(x)			__builtin_expect(!!(x), 1)
#define	unlikely(x)			__builtin_expect(!!(x), 0)
#endif
#define	typeof(x)			__typeof(x)

#define	uninitialized_var(x)		x = x
#define	__maybe_unused			__unused
#define	__always_unused			__unused
#define	__must_check			__result_use_check

#define	__printf(a, b)			__printflike(a, b)

#define	barrier()			__asm__ __volatile__("": : :"memory")
#define	___PASTE(a, b) a##b
#define	__PASTE(a, b) ___PASTE(a, b)

#define	ACCESS_ONCE(x)			(*(volatile __typeof(x) *)&(x))

#define	WRITE_ONCE(x, v) do {		\
	barrier();			\
	ACCESS_ONCE(x) = (v);		\
	barrier();			\
} while (0)

#define	lockless_dereference(p) READ_ONCE(p)

#define	_AT(T, X)	((T)(X))

#endif	
