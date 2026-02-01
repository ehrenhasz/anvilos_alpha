 
 
#ifndef _TOOLS_LINUX_ASM_IA64_BARRIER_H
#define _TOOLS_LINUX_ASM_IA64_BARRIER_H

#include <linux/compiler.h>

 

#define mb()		ia64_mf()
#define rmb()		mb()
#define wmb()		mb()

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
