#ifndef _ASM_IA64_BARRIER_H
#define _ASM_IA64_BARRIER_H
#include <linux/compiler.h>
#define mb()		ia64_mf()
#define rmb()		mb()
#define wmb()		mb()
#define dma_rmb()	mb()
#define dma_wmb()	mb()
# define __smp_mb()	mb()
#define __smp_mb__before_atomic()	barrier()
#define __smp_mb__after_atomic()	barrier()
#define __smp_store_release(p, v)						\
do {									\
	compiletime_assert_atomic_type(*p);				\
	barrier();							\
	WRITE_ONCE(*p, v);						\
} while (0)
#define __smp_load_acquire(p)						\
({									\
	typeof(*p) ___p1 = READ_ONCE(*p);				\
	compiletime_assert_atomic_type(*p);				\
	barrier();							\
	___p1;								\
})
#include <asm-generic/barrier.h>
#endif  
