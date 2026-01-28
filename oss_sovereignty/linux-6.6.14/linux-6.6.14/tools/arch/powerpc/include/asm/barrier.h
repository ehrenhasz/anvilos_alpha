#ifndef _TOOLS_LINUX_ASM_POWERPC_BARRIER_H
#define _TOOLS_LINUX_ASM_POWERPC_BARRIER_H
#define mb()   __asm__ __volatile__ ("sync" : : : "memory")
#define rmb()  __asm__ __volatile__ ("sync" : : : "memory")
#define wmb()  __asm__ __volatile__ ("sync" : : : "memory")
#if defined(__powerpc64__)
#define smp_lwsync()	__asm__ __volatile__ ("lwsync" : : : "memory")
#define smp_store_release(p, v)			\
do {						\
	smp_lwsync();				\
	WRITE_ONCE(*p, v);			\
} while (0)
#define smp_load_acquire(p)			\
({						\
	typeof(*p) ___p1 = READ_ONCE(*p);	\
	smp_lwsync();				\
	___p1;					\
})
#endif  
#endif  
