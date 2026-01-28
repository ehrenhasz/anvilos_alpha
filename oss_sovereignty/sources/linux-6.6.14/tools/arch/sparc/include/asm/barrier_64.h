
#ifndef __TOOLS_LINUX_SPARC64_BARRIER_H
#define __TOOLS_LINUX_SPARC64_BARRIER_H


#define membar_safe(type) \
do {	__asm__ __volatile__("ba,pt	%%xcc, 1f\n\t" \
			     " membar	" type "\n" \
			     "1:\n" \
			     : : : "memory"); \
} while (0)


#define mb()	membar_safe("#StoreLoad")
#define rmb()	__asm__ __volatile__("":::"memory")
#define wmb()	__asm__ __volatile__("":::"memory")

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
