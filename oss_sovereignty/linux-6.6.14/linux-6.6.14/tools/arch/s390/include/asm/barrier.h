#ifndef __TOOLS_LINUX_ASM_BARRIER_H
#define __TOOLS_LINUX_ASM_BARRIER_H
#ifdef CONFIG_HAVE_MARCH_Z196_FEATURES
#define __ASM_BARRIER "bcr 14,0\n"
#else
#define __ASM_BARRIER "bcr 15,0\n"
#endif
#define mb() do {  asm volatile(__ASM_BARRIER : : : "memory"); } while (0)
#define rmb()				mb()
#define wmb()				mb()
#define smp_store_release(p, v)			\
do {						\
	barrier();				\
	WRITE_ONCE(*p, v);			\
} while (0)
#define smp_load_acquire(p)			\
({						\
	typeof(*p) ___p1 = READ_ONCE(*p);	\
	barrier();				\
	___p1;					\
})
#endif  
