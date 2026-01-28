#ifndef _ASM_S390_NUMA_H
#define _ASM_S390_NUMA_H
#ifdef CONFIG_NUMA
#include <linux/numa.h>
void numa_setup(void);
#else
static inline void numa_setup(void) { }
#endif  
#endif  
